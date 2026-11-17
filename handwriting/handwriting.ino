// Handwriting -> text with a Seeed XIAO ESP32S3 Sense.
//
// The board takes a photo, sends it to a vision model on OpenRouter, and
// shows the transcribed text on a small web page and on the serial monitor.
//
// Trigger a capture from the web page button, or by sending any character
// over serial.

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include "esp_camera.h"
#include "img_converters.h"
#include "mbedtls/base64.h"
#include "secrets.h"  // WIFI_SSID, WIFI_PASS, OPENROUTER_API_KEY

// The board's name on the network: the web page is at http://<HOSTNAME>.local/
// If you change it, make a new QR code (qr.svg holds the one for "handwriting").
#define HOSTNAME "handwriting"

#include "index_html.h"  // uses HOSTNAME, so it comes after it

// Any OpenRouter model that accepts image input works here.
#define OCR_MODEL "google/gemini-3.8-flash"

#define OCR_PROMPT \
  "Transcribe the handwriting in this photo exactly as written, including any maths. " \
  "Write words as plain text. Write every mathematical expression in LaTeX, " \
  "wrapped in single dollar signs for inline maths or double dollar signs for an equation on its own line. " \
  "Do not solve, simplify or correct anything. " \
  "Keep the line breaks. Output only the transcription, with no commentary. " \
  "If there is no readable handwriting, reply with: [no handwriting found]"

// Status LEDs. Wire each one from its pin, through a resistor (about 220 ohm),
// to GND. These three pins are not used by the camera board.
#define LED_GREEN  D0  // working ok
#define LED_YELLOW D1  // the last read failed
#define LED_RED    D2  // any other error (no WiFi, camera or memory fault)

// Camera pins for the XIAO ESP32S3 Sense expansion board.
#define PWDN_GPIO_NUM  -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  10
#define SIOD_GPIO_NUM  40
#define SIOC_GPIO_NUM  39
#define Y9_GPIO_NUM    48
#define Y8_GPIO_NUM    11
#define Y7_GPIO_NUM    12
#define Y6_GPIO_NUM    14
#define Y5_GPIO_NUM    16
#define Y4_GPIO_NUM    18
#define Y3_GPIO_NUM    17
#define Y2_GPIO_NUM    15
#define VSYNC_GPIO_NUM 38
#define HREF_GPIO_NUM  47
#define PCLK_GPIO_NUM  13

static const char BODY_PREFIX[] =
  "{\"model\":\"" OCR_MODEL "\",\"max_tokens\":2000,\"reasoning\":{\"effort\":\"low\"},\"messages\":[{\"role\":\"user\",\"content\":["
  "{\"type\":\"text\",\"text\":\"" OCR_PROMPT "\"},"
  "{\"type\":\"image_url\",\"image_url\":{\"url\":\"data:image/jpeg;base64,";
static const char BODY_SUFFIX[] = "\"}}]}]}";


WebServer server(80);
String lastTiming;  // timing breakdown of the most recent read
String latestText;  // result of the most recent read
uint32_t readCount = 0;
bool lastReadFailed = false;

// Lights exactly one of the three status LEDs.
void showLed(int pin) {
  digitalWrite(LED_GREEN, pin == LED_GREEN);
  digitalWrite(LED_YELLOW, pin == LED_YELLOW);
  digitalWrite(LED_RED, pin == LED_RED);
}

void updateLeds() {
  if (WiFi.status() != WL_CONNECTED) showLed(LED_RED);
  else if (lastReadFailed) showLed(LED_YELLOW);
  else showLed(LED_GREEN);
}

// Live mode: the board watches a small greyscale copy of the camera picture
// and starts a read by itself when the page has changed and then gone still
// (so it does not read while the hand is moving).
#define THUMB_W 160
#define THUMB_H 120
#define THUMB_PIXELS (THUMB_W * THUMB_H)
#define PIXEL_DIFF 10        // brightness change (scale 0-125) that counts as "different"
#define MOTION_PERCENT 0.3f  // below this between two samples, the scene counts as still
#define NEW_PERCENT 0.15f    // above this compared with the last read, there is new writing

bool liveMode = false;
unsigned long lastPollMs = 0, lastSampleMs = 0;
uint8_t *rgbScratch, *thumbNow, *thumbPrev, *thumbRead;
bool havePrev = false, haveRead = false;
int stillCount = 0;
float lastMotion = 0, lastNew = 0;

bool initCamera() {
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_UXGA;  // 1600x1200: handwriting needs the detail
  config.jpeg_quality = 10;
  config.fb_count = 2;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }
  // The sensor delivers a mirror image by default, which makes text unreadable.
  sensor_t *s = esp_camera_sensor_get();
  s->set_hmirror(s, 1);
  return true;
}

// Returns a fresh frame at the given resolution. The preview uses small
// frames (quick to send); recognition uses full-size ones. After a resolution
// change the first few frames are stale or badly exposed, so they are dropped.
camera_fb_t *grabFrame(framesize_t size) {
  sensor_t *s = esp_camera_sensor_get();
  if (s->status.framesize != size) {
    s->set_framesize(s, size);
    for (int i = 0; i < 3; i++) {
      camera_fb_t *stale = esp_camera_fb_get();
      if (stale) esp_camera_fb_return(stale);
    }
  }
  return esp_camera_fb_get();
}

// Takes a photo and returns the transcription (or an "Error: ..." message).
String recogniseHandwriting() {
  unsigned long t0 = millis();
  lastTiming = "";
  camera_fb_t *fb = grabFrame(FRAMESIZE_UXGA);
  unsigned long tCapture = millis();
  if (!fb) return "Error: camera capture failed";

  size_t prefixLen = strlen(BODY_PREFIX);
  size_t suffixLen = strlen(BODY_SUFFIX);
  size_t b64Len = 4 * ((fb->len + 2) / 3);
  size_t bodyLen = prefixLen + b64Len + suffixLen;

  char *body = (char *)ps_malloc(bodyLen + 1);
  if (!body) {
    esp_camera_fb_return(fb);
    return "Error: out of memory";
  }
  memcpy(body, BODY_PREFIX, prefixLen);
  size_t written = 0;
  mbedtls_base64_encode((unsigned char *)body + prefixLen, b64Len + 1, &written, fb->buf, fb->len);
  memcpy(body + prefixLen + written, BODY_SUFFIX, suffixLen);
  Serial.printf("Captured %u byte JPEG, sending to %s...\n", (unsigned)fb->len, OCR_MODEL);
  esp_camera_fb_return(fb);

  WiFiClientSecure client;
  client.setInsecure();  // no certificate check; fine for a hobby project
  // Connect first (timed separately); the request below reuses this connection.
  if (!client.connect("openrouter.ai", 443)) {
    free(body);
    return "Error: could not connect to OpenRouter";
  }
  unsigned long tConnected = millis();
  HTTPClient http;
  http.begin(client, "https://openrouter.ai/api/v1/chat/completions");
  http.setTimeout(60000);
  http.useHTTP10(true);  // plain (un-chunked) reply, so it can be parsed as it arrives
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " OPENROUTER_API_KEY);
  int status = http.POST((uint8_t *)body, bodyLen);
  free(body);
  unsigned long tSent = millis();

  if (status <= 0) {
    String err = "Error: request failed (" + HTTPClient::errorToString(status) + ")";
    http.end();
    return err;
  }
  // OpenRouter sends blank padding while the model works, then the JSON.
  // Keep reading until a complete JSON reply has arrived, the server hangs
  // up, or 60 seconds pass.
  JsonDocument doc;
  DeserializationError parseError = DeserializationError::EmptyInput;
  String reply;
  uint8_t buf[512];
  unsigned long readStart = millis();
  while (millis() - readStart < 60000) {
    int n = client.available() > 0 ? client.read(buf, sizeof(buf)) : 0;
    if (n > 0) {
      reply.concat((const char *)buf, n);
      int last = n - 1;
      while (last >= 0 && isspace(buf[last])) last--;
      if (last >= 0 && buf[last] == '}') {  // might be the end of the JSON
        parseError = deserializeJson(doc, reply);
        if (!parseError) break;
      }
    } else if (!client.connected()) {
      parseError = deserializeJson(doc, reply);
      break;
    } else {
      delay(10);
    }
  }
  http.end();
  unsigned long tDone = millis();
  // Where the time went, so changes to model or image size can be compared.
  lastTiming = "Total " + String((tDone - t0) / 1000.0, 1) + " s: photo " + String((tCapture - t0) / 1000.0, 1) +
               " s, connect " + String((tConnected - tCapture) / 1000.0, 1) +
               " s, upload and wait " + String((tSent - tConnected) / 1000.0, 1) +
               " s, reply " + String((tDone - tSent) / 1000.0, 1) + " s";
  Serial.println(lastTiming);
  if (parseError) {
    return "Error: HTTP " + String(status) + ", could not read the reply (" + parseError.c_str() + ")";
  }
  if (doc["error"]["message"].is<const char *>()) {
    return "Error: " + String(doc["error"]["message"].as<const char *>());
  }
  const char *text = doc["choices"][0]["message"]["content"];
  if (!text) return "Error: HTTP " + String(status) + ", no text in response";
  String result = text;
  result.trim();
  return result;
}

void doRead() {
  latestText = recogniseHandwriting();
  readCount++;
  // A read fails if something went wrong, or if no handwriting was found.
  lastReadFailed = latestText.startsWith("Error:") || latestText == "[no handwriting found]";
  updateLeds();
  Serial.println(latestText);
}

// Fills `luma` with a 160x120 greyscale copy of the current camera picture.
bool takeThumbnail(uint8_t *luma) {
  camera_fb_t *fb = grabFrame(FRAMESIZE_VGA);
  if (!fb) return false;
  bool ok = jpg2rgb565(fb->buf, fb->len, rgbScratch, JPG_SCALE_4X);
  esp_camera_fb_return(fb);
  if (!ok) return false;
  for (int i = 0; i < THUMB_PIXELS; i++) {
    uint8_t hi = rgbScratch[2 * i], lo = rgbScratch[2 * i + 1];  // RRRRRGGG GGGBBBBB
    luma[i] = (hi >> 3) * 2 + (((hi & 7) << 3) | (lo >> 5));
  }
  return true;
}

// Percentage of pixels that differ between two thumbnails, ignoring an
// overall brightness shift (the camera adjusts exposure as the hand moves).
float percentChanged(const uint8_t *a, const uint8_t *b) {
  long sumA = 0, sumB = 0;
  for (int i = 0; i < THUMB_PIXELS; i++) {
    sumA += a[i];
    sumB += b[i];
  }
  int offset = (sumA - sumB) / THUMB_PIXELS;
  int changed = 0;
  for (int i = 0; i < THUMB_PIXELS; i++) {
    if (abs((int)a[i] - (int)b[i] - offset) > PIXEL_DIFF) changed++;
  }
  return 100.0f * changed / THUMB_PIXELS;
}

void liveTick() {
  if (!liveMode) return;
  if (millis() - lastPollMs > 15000) {  // nobody is looking at the dashboard
    liveMode = false;
    Serial.println("Live mode off: dashboard closed");
    return;
  }
  if (millis() - lastSampleMs < 400) return;
  lastSampleMs = millis();
  if (!takeThumbnail(thumbNow)) return;

  lastMotion = havePrev ? percentChanged(thumbNow, thumbPrev) : 100;
  lastNew = haveRead ? percentChanged(thumbNow, thumbRead) : 100;
  memcpy(thumbPrev, thumbNow, THUMB_PIXELS);
  havePrev = true;
  stillCount = lastMotion < MOTION_PERCENT ? stillCount + 1 : 0;

  if (stillCount >= 2 && lastNew > NEW_PERCENT) {
    memcpy(thumbRead, thumbNow, THUMB_PIXELS);
    haveRead = true;
    Serial.printf("Live: new writing detected (%.2f%% changed)\n", lastNew);
    doRead();
    stillCount = 0;
    havePrev = false;
    lastPollMs = millis();  // the dashboard could not poll during the read
  }
}

void handlePreview() {
  camera_fb_t *fb = grabFrame(FRAMESIZE_VGA);
  if (!fb) {
    Serial.println("Preview: camera capture failed");
    server.send(503, "text/plain", "camera capture failed");
    return;
  }
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "image/jpeg", (const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

void handleOcr() {
  doRead();
  server.sendHeader("X-Timing", lastTiming);
  server.send(200, "text/plain; charset=utf-8", latestText);
}

void handleLive() {
  liveMode = server.arg("on") == "1";
  havePrev = haveRead = false;
  stillCount = 0;
  lastPollMs = millis();
  Serial.printf("Live mode %s\n", liveMode ? "on" : "off");
  server.send(200, "text/plain", "ok");
}

void handleState() {
  lastPollMs = millis();
  JsonDocument doc;
  doc["live"] = liveMode;
  doc["count"] = readCount;
  doc["motion"] = String(lastMotion, 2);
  doc["fresh"] = String(lastNew, 2);
  doc["timing"] = lastTiming;
  doc["text"] = latestText;
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  showLed(-1);  // all off until the board is ready
  delay(1500);

  if (!psramFound()) {
    Serial.println("PSRAM not found: set Tools > PSRAM > OPI PSRAM and re-upload.");
    showLed(LED_RED);
    while (true) delay(1000);
  }
  if (!initCamera()) {
    Serial.println("Check the camera board is firmly seated on the XIAO.");
    showLed(LED_RED);
    while (true) delay(1000);
  }
  rgbScratch = (uint8_t *)ps_malloc(THUMB_PIXELS * 2);
  thumbNow = (uint8_t *)ps_malloc(THUMB_PIXELS);
  thumbPrev = (uint8_t *)ps_malloc(THUMB_PIXELS);
  thumbRead = (uint8_t *)ps_malloc(THUMB_PIXELS);

  WiFi.setHostname(HOSTNAME);  // must come before WiFi.mode()
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("Connecting to WiFi \"%s\"", WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    digitalWrite(LED_GREEN, !digitalRead(LED_GREEN));  // green blinks while connecting
  }
  updateLeds();
  // Announce the name on the local network, so the page has a fixed address.
  String ip = WiFi.localIP().toString();
  if (MDNS.begin(HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("\nReady. Open http://%s.local/ in a browser\n", HOSTNAME);
    Serial.printf("(or http://%s/ if the name does not work),\n", ip.c_str());
  } else {
    Serial.printf("\nReady, but the name could not be set up. Open http://%s/ in a browser,\n", ip.c_str());
  }
  Serial.println("or send any character here to read.");

  server.on("/", []() { server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/preview.jpg", handlePreview);
  server.on("/ocr", handleOcr);
  server.on("/live", handleLive);
  server.on("/state", handleState);
  server.begin();
}

void loop() {
  server.handleClient();

  if (Serial.available()) {
    while (Serial.available()) Serial.read();
    doRead();
  }
  liveTick();
  updateLeds();  // also catches WiFi dropping out and coming back
  delay(2);
}
