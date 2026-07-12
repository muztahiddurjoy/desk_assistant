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

// Accent colour per state. Carried in both spaces: RGB565 is what TFT_eSPI
// draws with, 0xRRGGBB is what the renderer scales brightness in (channels are
// independent there, so dimming doesn't shift the hue the way it does in 565).
struct StateStyle {
    uint16_t tft;  // RGB565, ready to draw
    uint32_t rgb;  // 0xRRGGBB, for brightness maths
};

StateStyle styleFor(AppState s);
