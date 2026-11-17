# Handwriting reader

Turns handwriting into text with a Seeed XIAO ESP32S3 Sense. The board takes a photo, sends it to a vision model on [OpenRouter](https://openrouter.ai), and shows the transcription on a small web page and on the serial monitor. Maths is transcribed as LaTeX and typeset on the page.

## What you need

- Seeed XIAO ESP32S3 Sense, with the camera board attached
- Arduino IDE with the **esp32** board package
- The **ArduinoJson** library (version 7)
- An OpenRouter API key, from <https://openrouter.ai/keys>
- A 2.4 GHz WiFi network
- Optional: a green, a yellow and a red LED, each with a resistor of about 220 ohm

## Status LEDs

Wire each LED from its pin, through a resistor, to GND. The long leg of the LED goes towards the pin.

| LED | Pin | Meaning |
| --- | --- | --- |
| Green | D0 | Working. It blinks while the board connects to WiFi. |
| Yellow | D1 | The last read failed, or found no handwriting. It clears on the next good read. |
| Red | D2 | Any other error: no WiFi, or a camera or memory fault at startup. |

The pins are set by `LED_GREEN`, `LED_YELLOW` and `LED_RED` at the top of `handwriting/handwriting.ino`.

## Setup

1. Copy `handwriting/secrets.example.h` to `handwriting/secrets.h` and fill in your WiFi name, WiFi password and OpenRouter key. `secrets.h` is in `.gitignore`, so your key is not committed.
2. Open `handwriting/handwriting.ino` in the Arduino IDE.
3. Select the board **XIAO_ESP32S3** and set **Tools > PSRAM > OPI PSRAM**.
4. Upload, then open the serial monitor at 115200 baud. It prints the board's address once WiFi connects.

## Use

Open <http://handwriting.local/> in a browser on the same network, or scan this QR code with a phone. The page shows a live camera preview.

![QR code for http://handwriting.local/](qr.svg)

The address is always the same, so you can print the QR code ([qr.svg](qr.svg)) and stick it on the board. The page also shows the QR code, for passing the address from a computer to a phone.

If the name does not work, use the number address that the serial monitor prints (`http://<board-ip>/`). Some networks, such as university and guest WiFi, block the name lookup (mDNS), and some older Android phones do not support it.

- **Read handwriting**: takes a photo and shows the transcription.
- **Live mode**: the board reads by itself when the writing has changed and the page is still. It turns off 15 seconds after the web page is closed.
- **Serial**: send any character in the serial monitor to trigger a read.

Each read also shows how long the photo, connection, upload and reply took.

## Settings

These are at the top of `handwriting/handwriting.ino`:

| Setting | What it does |
| --- | --- |
| `HOSTNAME` | The name in the address (`http://<HOSTNAME>.local/`). If you change it, make a new QR code for `qr.svg`. |
| `OCR_MODEL` | The OpenRouter model. Any model that accepts image input works. |
| `OCR_PROMPT` | The instructions sent with the photo. |
| `MOTION_PERCENT`, `NEW_PERCENT` | How sensitive live mode is to movement and to new writing. |

## Notes

- Each read sends a photo to OpenRouter and uses API credit. Live mode can make many reads.
- The web page has no password, so anyone on the same network can open it.
- The board does not check OpenRouter's TLS certificate.
- The web page loads KaTeX from a CDN to typeset maths. Without internet access in the browser, the LaTeX is shown as plain text.
