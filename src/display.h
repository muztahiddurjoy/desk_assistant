// src/display.h — everything that lands on the GC9A01.
#pragma once

#include <Arduino.h>
#include "app_state.h"

namespace Ui {

void begin();

// Blocking. Shows the logo, then fades the backlight up.
void splash(uint32_t holdMs);

void setState(AppState s);

// Subtitle under the state name. Pass "" to clear.
void setMessage(const char* msg);

// Non-blocking; call every loop. Repaints only what changed.
void update();

// 0..100. No-op on breakouts whose BLK pin isn't actually wired to the panel.
void setBacklight(uint8_t percent);

}  // namespace Ui
