// include/pins.h
//
// Single source of truth for application code. TFT_eSPI cannot read its pins
// from here — it only sees the -D flags in platformio.ini — so the two are
// mirrored, and the static_asserts at the bottom break the build if they drift.
//
// See PINOUT.txt for the wiring sheet and the reasoning behind the choices.
#pragma once

#include <Arduino.h>

// ── GC9A01 1.28" round IPS LCD, 240x240, 4-wire SPI ──────────────────────────
// 7-pin breakout: VCC GND SCL SDA RES DC CS. There is no BL pin — the backlight
// is tied to VCC on the board, always on, not dimmable. Nothing to allocate.
#define PIN_TFT_MOSI 4  // panel "SDA"
#define PIN_TFT_SCLK 5  // panel "SCL"
#define PIN_TFT_CS   6
#define PIN_TFT_DC   7
#define PIN_TFT_RST  8  // panel "RES"

// ── WS2812 sticks, 8 pixels each, independently addressable ──────────────────
#define PIN_LED_STICK_L 10
#define PIN_LED_STICK_R 11

// ── Reserved ─────────────────────────────────────────────────────────────────
// Free inside the 4..13 block. GPIO9 came back when the backlight turned out to
// be hard-wired; 12/13 are the natural home for a mode button and/or I2C.
#define PIN_SPARE_A 9
#define PIN_SPARE_B 12
#define PIN_SPARE_C 13

// ── Hardware constants ───────────────────────────────────────────────────────
constexpr uint8_t  LEDS_PER_STICK = 8;
constexpr uint8_t  LED_COUNT      = LEDS_PER_STICK * 2;  // 16 total

// WS2812s pull ~60 mA each at full white. 16 of them can ask for ~960 mA, which
// a USB-powered dev module will not survive. FastLED's power manager scales
// brightness to hold the whole strip under this budget.
constexpr uint8_t  LED_PSU_VOLTS = 5;
constexpr uint32_t LED_PSU_MILLIAMPS = 600;

constexpr int16_t  SCREEN_W  = 240;
constexpr int16_t  SCREEN_H  = 240;
constexpr int16_t  SCREEN_CX = SCREEN_W / 2;  // 120 — the panel is round, so
constexpr int16_t  SCREEN_CY = SCREEN_H / 2;  // everything composes about this
constexpr int16_t  SCREEN_R  = 120;           // usable radius; corners are glass

// ── Drift guards ─────────────────────────────────────────────────────────────
#ifdef TFT_MOSI
static_assert(PIN_TFT_MOSI == TFT_MOSI, "pins.h TFT_MOSI disagrees with platformio.ini");
static_assert(PIN_TFT_SCLK == TFT_SCLK, "pins.h TFT_SCLK disagrees with platformio.ini");
static_assert(PIN_TFT_CS   == TFT_CS,   "pins.h TFT_CS disagrees with platformio.ini");
static_assert(PIN_TFT_DC   == TFT_DC,   "pins.h TFT_DC disagrees with platformio.ini");
static_assert(PIN_TFT_RST  == TFT_RST,  "pins.h TFT_RST disagrees with platformio.ini");
#endif

#ifdef TFT_BL
#error "TFT_BL is defined, but this panel has no BL pin. Remove it from platformio.ini."
#endif
