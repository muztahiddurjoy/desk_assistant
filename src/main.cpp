// Desk Assistant — ESP32-S3 Dev Module
//   1.28" round GC9A01 240x240 IPS  +  2x WS2812 8-LED stick
//   Wiring: PINOUT.txt      Pins: include/pins.h
//
// There is no mic, no WiFi and no LLM wired up yet. What this firmware gives you
// is the hardware brought fully up and a state machine to drive it — swap the
// serial console below for real events (wake word, request, response) and the
// display and LEDs already follow along.

#include <Arduino.h>

#include "app_state.h"
#include "display.h"
#include "leds.h"
#include "pins.h"

static AppState state = AppState::Boot;

// Demo mode walks the states on a timer so you can eyeball every animation
// without typing. Off once you send any manual command.
static bool     demo = true;
static uint32_t demoNext = 0;
static uint8_t  demoStep = 0;

static void go(AppState s, const char* msg) {
    state = s;
    Ui::setState(s);
    Ui::setMessage(msg);
    Leds::setState(s);
    Serial.printf("[state] %s  %s\n", stateName(s), msg);
}

static void help() {
    Serial.println(F(
        "\ncommands:\n"
        "  idle | listen | think | speak | alert   set state\n"
        "  demo                                    resume the auto state cycle\n"
        "  led <0-255>                             LED brightness\n"
        "  id                                      identify sticks (L=red, R=green)\n"
        "  help\n"));
}

static void handleCommand(String line) {
    line.trim();
    line.toLowerCase();
    if (!line.length()) return;

    if (line != "demo") demo = false;

    if      (line == "idle")   go(AppState::Idle,      "ready");
    else if (line == "listen") go(AppState::Listening, "listening...");
    else if (line == "think")  go(AppState::Thinking,  "working on it");
    else if (line == "speak")  go(AppState::Speaking,  "here's what I found");
    else if (line == "alert")  go(AppState::Alert,     "needs attention");
    else if (line == "id")     Leds::identify();
    else if (line == "help")   help();
    else if (line == "demo") {
        demo = true;
        demoNext = 0;
        Serial.println("[demo] on");
    }
    else if (line.startsWith("led ")) {
        uint8_t b = (uint8_t)constrain(line.substring(4).toInt(), 0, 255);
        Leds::setBrightness(b);
        Serial.printf("[leds] brightness %u\n", b);
    }
    else {
        Serial.printf("? %s  (try 'help')\n", line.c_str());
    }
}

static void pollSerial() {
    static String buf;
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            if (buf.length()) {
                handleCommand(buf);
                buf = "";
            }
        } else if (buf.length() < 64) {
            buf += c;
        }
    }
}

// Idle -> Listening -> Thinking -> Speaking -> Idle, on a loop.
static void pollDemo() {
    if (!demo || millis() < demoNext) return;

    switch (demoStep) {
        case 0: go(AppState::Idle,      "ready");                demoNext = millis() + 3500; break;
        case 1: go(AppState::Listening, "listening...");         demoNext = millis() + 2500; break;
        case 2: go(AppState::Thinking,  "working on it");        demoNext = millis() + 2000; break;
        case 3: go(AppState::Speaking,  "here's what I found");  demoNext = millis() + 3500; break;
    }
    demoStep = (demoStep + 1) % 4;
}

void setup() {
    Serial.begin(115200);
    // USB-CDC enumerates a moment after boot. Without this the first prints —
    // including anything useful about a failure — go into the void.
    uint32_t t0 = millis();
    while (!Serial && millis() - t0 < 1500) delay(10);

    Serial.println(F("\n=== desk assistant ==="));
    Serial.printf("board  : ESP32-S3  %u MHz, %u KB free heap\n",
                  getCpuFrequencyMhz(), ESP.getFreeHeap() / 1024);
    Serial.printf("display: GC9A01 240x240  mosi=%d sclk=%d cs=%d dc=%d rst=%d  (backlight: hard-wired)\n",
                  PIN_TFT_MOSI, PIN_TFT_SCLK, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);
    Serial.printf("leds   : 2 x %u WS2812 on GPIO %d and %d (cap %lu mA)\n",
                  LEDS_PER_STICK, PIN_LED_STICK_L, PIN_LED_STICK_R,
                  (unsigned long)LED_PSU_MILLIAMPS);

    Ui::begin();
    Leds::begin();

    Leds::setState(AppState::Boot);
    Ui::splash(1500);

    go(AppState::Idle, "ready");
    help();
}

void loop() {
    pollSerial();
    pollDemo();

    Ui::update();
    Leds::update();
}
