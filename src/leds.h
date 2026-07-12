// src/leds.h — the two WS2812 sticks.
#pragma once

#include <Arduino.h>
#include "app_state.h"

namespace Leds {

void begin();

void setState(AppState s);

// Non-blocking; call every loop. Drives its own ~60 fps clock.
void update();

// 0..255, applied on top of the power cap. Default 120.
void setBrightness(uint8_t b);

// Runs the two sticks head-to-tail in red/green/blue so you can confirm which
// physical stick is L, which is R, and which end each one starts from. Blocking.
void identify();

}  // namespace Leds
