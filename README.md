# Handwriting reader

Turns handwriting into text with a Seeed XIAO ESP32S3 Sense. The board takes a photo, sends it to a vision model on [OpenRouter](https://openrouter.ai), and shows the transcription on a small web page and on the serial monitor. Maths is transcribed as LaTeX and typeset on the page.

## What you need

- Seeed XIAO ESP32S3 Sense, with the camera board attached
- Arduino IDE with the **esp32** board package
- The **ArduinoJson** library (version 7)
- An OpenRouter API key, from <https://openrouter.ai/keys>
- A 2.4 GHz WiFi network

## Setup

1. Copy `handwriting/secrets.example.h` to `handwriting/secrets.h` and fill in your WiFi name, WiFi password and OpenRouter key. `secrets.h` is in `.gitignore`, so your key is not committed.
2. Open `handwriting/handwriting.ino` in the Arduino IDE.
3. Select the board **XIAO_ESP32S3** and set **Tools > PSRAM > OPI PSRAM**.
4. Upload, then open the serial monitor at 115200 baud. It prints the board's address once WiFi connects.

## Use

Open the printed address (`http://<board-ip>/`) in a browser on the same network. The page shows a live camera preview.

- **Read handwriting**: takes a photo and shows the transcription.
- **Live mode**: the board reads by itself when the writing has changed and the page is still. It turns off 15 seconds after the web page is closed.
- **Serial**: send any character in the serial monitor to trigger a read.

Each read also shows how long the photo, connection, upload and reply took.

## Settings

These are at the top of `handwriting/handwriting.ino`:

| Setting | What it does |
| --- | --- |
| `OCR_MODEL` | The OpenRouter model. Any model that accepts image input works. |
| `OCR_PROMPT` | The instructions sent with the photo. |
| `MOTION_PERCENT`, `NEW_PERCENT` | How sensitive live mode is to movement and to new writing. |

## Notes

- Each read sends a photo to OpenRouter and uses API credit. Live mode can make many reads.
- The web page has no password, so anyone on the same network can open it.
- The board does not check OpenRouter's TLS certificate.
- The web page loads KaTeX from a CDN to typeset maths. Without internet access in the browser, the LaTeX is shown as plain text.
