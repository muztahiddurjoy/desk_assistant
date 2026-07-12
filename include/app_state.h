// include/app_state.h
//
// The one thing the display and the LED sticks both need to agree on: what the
// assistant is currently doing. Everything else about them is independent.
#pragma once

#include <Arduino.h>

enum class AppState : uint8_t {
    Boot,       // splash is up, nothing is listening yet
    Idle,       // at rest, waiting for a wake word
    Listening,  // capturing the user
    Thinking,   // request is in flight
    Speaking,   // reading the answer back
    Alert,      // something needs attention
};

const char* stateName(AppState s);

// Accent colour per state, used by BOTH renderers so the ring and the sticks
// always agree. Display wants RGB565, FastLED wants 0xRRGGBB, so each state
// carries both — derived from the same intent, converted once, here.
struct StateStyle {
    uint16_t tft;  // RGB565 for TFT_eSPI
    uint32_t rgb;  // 0xRRGGBB for FastLED
};

StateStyle styleFor(AppState s);
