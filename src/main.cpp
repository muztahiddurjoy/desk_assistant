// ─────────────────────────────────────────────────────────────────────────────
//  Desk Assistant — mirrored logo plate
//
//  Display : GC9A01 1.28" round, 240x240. Everything is MIRRORED in hardware,
//            for viewing as a reflection (Pepper's-ghost / hologram rig).
//  Logo    : rendered inside a centred square, strictly clipped to it.
//
//  No LEDs. Config is the one proven during bring-up — in particular
//  USE_HSPI_PORT, which the S3 will not work without. See platformio.ini.
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "logo.h"
#include "pins.h"

static TFT_eSPI tft;

// ── Mirroring ────────────────────────────────────────────────────────────────
// Done in the panel, not in software. GC9A01_Rotation.h shows rotation 0 writes
// MADCTL = TFT_MAD_BGR; adding TFT_MAD_MX reverses the column order inside the
// panel's own address counter. So every pixel that ever gets drawn — this logo,
// text, future UI — comes out mirrored with no per-draw cost and nothing for the
// drawing code to remember. BGR is preserved, so the colour config still holds.
//
// TFT_MAD_MX = horizontal (left<->right). Swap to TFT_MAD_MY for vertical.
//
// CAUTION: tft.setRotation() rewrites MADCTL from scratch and would silently
// undo this. Call applyMirror() after it, or just never call it again.
static constexpr bool MIRROR = true;

static void applyMirror() {
    tft.writecommand(TFT_MADCTL);
    tft.writedata(MIRROR ? (TFT_MAD_MX | TFT_MAD_BGR) : TFT_MAD_BGR);
}

// ── The square ───────────────────────────────────────────────────────────────
//
//  Panel: 1.28" round, 32.4 mm of active glass across 240 px
//      => 32.4 / 240 = 0.135 mm per pixel
//
//  2.54 cm would be 188 px, whose corners sit at r = 133 px — but the glass ends
//  at r = 120, so they could never be seen. A square cannot be 1 inch across on
//  a 1.28 inch circle. This is the largest one that fits, keeping 1 px clear of
//  the bezel (r = 119):  side = 2 * 119 / sqrt(2) = 168.3  ->  168 px
//  = 22.7 mm = 2.27 cm, corners at r = 118.8. Inside.
//
//  Change SQUARE_PX if you want it smaller; the logo rescales to match.
static constexpr float   MM_PER_PX = 32.4f / 240.0f;
static constexpr int16_t SQUARE_PX = 168;
static constexpr int16_t SQUARE_X  = (SCREEN_W - SQUARE_PX) / 2;  // 36
static constexpr int16_t SQUARE_Y  = (SCREEN_H - SQUARE_PX) / 2;  // 36

static_assert(SQUARE_PX * SQUARE_PX / 2 <= (SCREEN_R - 1) * (SCREEN_R - 1),
              "square corners fall outside the round glass — reduce SQUARE_PX");

// Logo scaled to the square's width, aspect preserved. 240x125 -> 168x88.
static constexpr int16_t LOGO_W = SQUARE_PX;
static constexpr int16_t LOGO_H = (int16_t)((logo_height * SQUARE_PX + logo_width / 2) / logo_width);
static constexpr int16_t LOGO_Y = (SQUARE_PX - LOGO_H) / 2;  // centred in the square

static_assert(LOGO_H <= SQUARE_PX, "scaled logo is taller than the square");

static constexpr uint16_t BORDER_COLOUR = 0x4A69;  // dim grey — makes the square visible
static constexpr bool     DRAW_BORDER   = true;

// ── Render ───────────────────────────────────────────────────────────────────
// Everything is drawn into a sprite the exact size of the square, then pushed as
// a block. The sprite IS the clip region: there is no way for a pixel to land
// outside it, so "nothing exceeds the square" is structural, not something the
// drawing code has to remember to check.
//
// Note the sprite is composed UNMIRRORED. The panel does the flip on the way in,
// so the sprite stays in normal reading order and only the glass sees it
// reversed — which also means the square, being centred, still lands on exactly
// the same pixels.
static void drawPlate() {
    TFT_eSprite plate(&tft);
    plate.setColorDepth(16);

    if (!plate.createSprite(SQUARE_PX, SQUARE_PX)) {
        Serial.println(F("!! sprite alloc failed — not enough heap"));
        return;
    }

    plate.fillSprite(TFT_BLACK);

    // Nearest-neighbour downscale of the logo. Integer-only: for each destination
    // pixel, pick the source pixel it maps back to.
    for (int16_t dy = 0; dy < LOGO_H; dy++) {
        const int32_t sy = (int32_t)dy * logo_height / LOGO_H;
        const uint16_t* srcRow = &desk_logo[sy * logo_width];

        for (int16_t dx = 0; dx < LOGO_W; dx++) {
            const int32_t sx = (int32_t)dx * logo_width / LOGO_W;
            plate.drawPixel(dx, LOGO_Y + dy, pgm_read_word(&srcRow[sx]));
        }
    }

    if (DRAW_BORDER) {
        plate.drawRect(0, 0, SQUARE_PX, SQUARE_PX, BORDER_COLOUR);
    }

    // pushSprite() sets swapBytes itself, so the sprite's native-endian uint16_t
    // pixels — the logo's included — reach the panel high byte first.
    plate.pushSprite(SQUARE_X, SQUARE_Y);
    plate.deleteSprite();
}

void setup() {
    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && millis() - t0 < 2000) delay(10);

    Serial.println(F("\n\n=== desk assistant ================================"));
    Serial.printf("panel   : GC9A01 %dx%d round, %.4f mm/px\n",
                  SCREEN_W, SCREEN_H, MM_PER_PX);
    Serial.printf("mirror  : %s (hardware, MADCTL MX)\n", MIRROR ? "ON" : "off");
    Serial.printf("square  : %d px = %.2f cm, at (%d,%d)\n",
                  SQUARE_PX, SQUARE_PX * MM_PER_PX / 10.0f, SQUARE_X, SQUARE_Y);
    Serial.printf("          corners at r=%.1f px, glass ends at r=%d — inside\n",
                  SQUARE_PX * 0.70711f, SCREEN_R);
    Serial.printf("logo    : %dx%d -> %dx%d, inset %d px from square top\n",
                  logo_width, logo_height, LOGO_W, LOGO_H, LOGO_Y);
    Serial.println(F("---------------------------------------------------"));

    tft.init();
    tft.setRotation(0);
    applyMirror();  // must come after setRotation — it rewrites MADCTL
    tft.fillScreen(TFT_BLACK);

    drawPlate();

    Serial.println(F("logo up, mirrored."));
    Serial.println(F("==================================================="));
}

void loop() {
    // The screen is static. Nothing to do.
    delay(1000);
}
