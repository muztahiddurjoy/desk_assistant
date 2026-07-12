// src/display.h — everything that lands on the GC9A01.
#pragma once

#include <Arduino.h>
#include "app_state.h"

namespace Ui {

void begin();

// Blocking. Fades the logo in, holds, clears.
//
// The fade is done in software, by scaling the image's own pixels: this panel
// has no BL pin, so the backlight is always on at full and there is nothing to
// ramp. Any "screen brightness" here has to be paid for in pixel values.
void splash(uint32_t holdMs);

void setState(AppState s);

// Subtitle under the state name. Pass "" to clear.
void setMessage(const char* msg);

// Non-blocking; call every loop. Repaints only what changed.
void update();

}  // namespace Ui
