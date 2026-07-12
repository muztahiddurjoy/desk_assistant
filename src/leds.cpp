#define FASTLED_INTERNAL  // silence the "no hardware SPI pins defined" pragma spam
#include <FastLED.h>

#include "leds.h"
#include "pins.h"

namespace {

// One flat array; the two sticks are views into it. Left = [0..7], right = [8..15].
CRGB leds[LED_COUNT];

// Which end of each stick the DIN wire enters. Both false means pixel 0 is the
// outer pixel on both sticks, so effects mirror about the display. If a stick
// animates "backwards" on your desk, flip its flag — that is the whole fix, no
// resoldering.
constexpr bool STICK_L_REVERSED = false;
constexpr bool STICK_R_REVERSED = false;

AppState state = AppState::Boot;
uint8_t  brightness = 120;

// stick: 0 = left, 1 = right. i: 0..7, logical (0 = closest to the display).
CRGB& px(uint8_t stick, uint8_t i) {
    const bool rev = stick == 0 ? STICK_L_REVERSED : STICK_R_REVERSED;
    const uint8_t phys = rev ? (LEDS_PER_STICK - 1 - i) : i;
    return leds[stick * LEDS_PER_STICK + phys];
}

// Same pixel on both sticks — every effect below is symmetric, so this is what
// they nearly all use.
void both(uint8_t i, const CRGB& c) {
    px(0, i) = c;
    px(1, i) = c;
}

void renderBoot(uint32_t ms, const CRGB& accent) {
    // Fill outward from the display, once per second, as a "coming up" cue.
    fill_solid(leds, LED_COUNT, CRGB::Black);
    uint8_t n = (ms / 90) % (LEDS_PER_STICK + 3);
    for (uint8_t i = 0; i < LEDS_PER_STICK && i < n; i++) {
        both(i, accent);
    }
    fadeToBlackBy(leds, LED_COUNT, 90);
}

void renderIdle(uint32_t ms, const CRGB& accent) {
    // Slow, dim breathing. This runs for hours; it must not be distracting.
    uint8_t b = 10 + scale8(quadwave8(ms / 16), 45);
    CRGB c = accent;
    c.nscale8_video(b);
    fill_solid(leds, LED_COUNT, c);
}

void renderListening(uint32_t ms, const CRGB& accent) {
    // Both sticks fill from the display outward and hold, with a bright pixel
    // scanning the tip — "I'm open, keep going".
    fill_solid(leds, LED_COUNT, CRGB::Black);
    uint8_t scan = beatsin8(40, 0, LEDS_PER_STICK - 1);
    for (uint8_t i = 0; i < LEDS_PER_STICK; i++) {
        CRGB c = accent;
        uint8_t b = (i <= scan) ? (uint8_t)(40 + (uint16_t)i * 200 / LEDS_PER_STICK) : 12;
        c.nscale8_video(b);
        both(i, c);
    }
}

void renderThinking(uint32_t ms, const CRGB& accent) {
    // Knight-rider bounce with a decaying trail. Fast enough to read as busy.
    fadeToBlackBy(leds, LED_COUNT, 60);
    uint8_t pos = beatsin8(90, 0, LEDS_PER_STICK - 1);
    both(pos, accent);
}

void renderSpeaking(uint32_t ms, const CRGB& accent) {
    // A VU meter with no audio behind it: Perlin noise gives an envelope that
    // moves like speech instead of like a sine wave. Swap inoise8() for a real
    // amplitude when there's an I2S mic/DAC in the loop.
    uint8_t level = inoise8((uint16_t)(ms * 3), 0);
    level = qsub8(level, 40);  // noise rarely bottoms out; pull the floor down

    // level 0..255 -> lit 0..LEDS_PER_STICK. Do NOT route this through scale8():
    // its scale argument is a uint8_t, so anything that computes to 256 wraps to
    // zero and the sticks stay dark.
    uint8_t lit = (uint8_t)(((uint16_t)level * (LEDS_PER_STICK + 1)) / 256);

    for (uint8_t i = 0; i < LEDS_PER_STICK; i++) {
        if (i < lit) {
            // Warm at the base, hotter toward the tip.
            CRGB c = blend(accent, CRGB(255, 60, 0), (uint8_t)(i * 255 / LEDS_PER_STICK));
            both(i, c);
        } else {
            both(i, CRGB::Black);
        }
    }
    fadeToBlackBy(leds, LED_COUNT, 40);
}

void renderAlert(uint32_t ms, const CRGB& accent) {
    bool on = (ms / 250) % 2;
    fill_solid(leds, LED_COUNT, on ? accent : CRGB::Black);
}

}  // namespace

namespace Leds {

void begin() {
    // Two independent strips, one data pin each, sharing one buffer. FastLED
    // drives each on its own RMT channel; the S3 has four, so two is fine.
    FastLED.addLeds<WS2812B, PIN_LED_STICK_L, GRB>(leds, 0, LEDS_PER_STICK);
    FastLED.addLeds<WS2812B, PIN_LED_STICK_R, GRB>(leds, LEDS_PER_STICK, LEDS_PER_STICK);

    // Hard cap. 16 pixels at full white would pull ~960 mA and brown out a
    // USB-powered board mid-frame; FastLED scales global brightness to stay
    // under this instead. See PINOUT.txt section 2(b).
    FastLED.setMaxPowerInVoltsAndMilliamps(LED_PSU_VOLTS, LED_PSU_MILLIAMPS);

    FastLED.setBrightness(brightness);
    FastLED.setCorrection(TypicalLEDStrip);

    FastLED.clear(true);
}

void setState(AppState s) {
    state = s;
}

void setBrightness(uint8_t b) {
    brightness = b;
    FastLED.setBrightness(b);
}

void update() {
    static uint32_t last = 0;
    uint32_t now = millis();
    if (now - last < 16) return;  // ~60 fps
    last = now;

    const StateStyle st = styleFor(state);
    const CRGB accent = CRGB(st.rgb);

    switch (state) {
        case AppState::Boot:      renderBoot(now, accent);      break;
        case AppState::Idle:      renderIdle(now, accent);      break;
        case AppState::Listening: renderListening(now, accent); break;
        case AppState::Thinking:  renderThinking(now, accent);  break;
        case AppState::Speaking:  renderSpeaking(now, accent);  break;
        case AppState::Alert:     renderAlert(now, accent);     break;
    }

    FastLED.show();
}

void identify() {
    FastLED.clear(true);

    // Left stick red, one pixel at a time from its DIN end; then right in green.
    for (uint8_t s = 0; s < 2; s++) {
        for (uint8_t i = 0; i < LEDS_PER_STICK; i++) {
            leds[s * LEDS_PER_STICK + i] = (s == 0) ? CRGB::Red : CRGB::Green;
            FastLED.show();
            delay(80);
        }
    }

    delay(300);
    // Then logical pixel 0 on both, in blue: this is the end effects treat as
    // "closest to the display".
    FastLED.clear();
    both(0, CRGB::Blue);
    FastLED.show();
    delay(800);

    FastLED.clear(true);
}

}  // namespace Leds
