#include "display.h"

#include <TFT_eSPI.h>
#include <math.h>

#include "logo.h"
#include "pins.h"

namespace {

TFT_eSPI tft;

// The centre block (state name + subtitle) is composed off-screen and pushed in
// one go — drawing text straight to the panel flickers, because erasing the old
// glyphs and drawing the new ones are two visible steps.
TFT_eSprite centre(&tft);

constexpr int16_t CENTRE_W = 200;
constexpr int16_t CENTRE_H = 80;  // 200*80*2 = 32 KB, comfortable in internal RAM
constexpr int16_t CENTRE_X = SCREEN_CX - CENTRE_W / 2;
constexpr int16_t CENTRE_Y = SCREEN_CY - CENTRE_H / 2;

// Activity ring. 36 dots is enough to read as a circle without costing a full
// arc redraw every frame.
constexpr uint8_t RING_DOTS   = 36;
constexpr int16_t RING_RADIUS = 103;  // + dot radius 5 = 108, clear of the bezel
constexpr int16_t DOT_RADIUS  = 5;

int16_t  dotX[RING_DOTS];
int16_t  dotY[RING_DOTS];
uint16_t dotCache[RING_DOTS];  // last colour actually pushed to the panel

AppState state = AppState::Boot;
char     message[40] = "";
bool     centreDirty = true;

constexpr uint16_t COL_BG    = TFT_BLACK;
constexpr uint16_t COL_TRACK = 0x18E3;  // near-black grey: the unlit ring dots
constexpr uint16_t COL_LABEL = 0x9CF3;  // muted grey for the subtitle

uint16_t to565(uint32_t rgb) {
    return (uint16_t)(((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x001F));
}

// scale is 0..255.
uint16_t dim565(uint32_t rgb, uint8_t scale) {
    uint32_t r = ((rgb >> 16) & 0xFF) * scale / 255;
    uint32_t g = ((rgb >> 8) & 0xFF) * scale / 255;
    uint32_t b = (rgb & 0xFF) * scale / 255;
    return to565((r << 16) | (g << 8) | b);
}

// 0..255 triangle wave over `periodMs`, eased so the turnaround isn't a visible
// corner. Used for anything that "breathes".
uint8_t breathe(uint32_t ms, uint32_t periodMs) {
    float phase = (float)(ms % periodMs) / (float)periodMs;   // 0..1
    float v = 0.5f - 0.5f * cosf(phase * 2.0f * (float)PI);   // 0..1, smooth
    return (uint8_t)(v * 255.0f);
}

void drawDot(uint8_t i, uint16_t colour) {
    if (dotCache[i] == colour) return;  // the whole point of the cache
    tft.fillSmoothCircle(dotX[i], dotY[i], DOT_RADIUS, colour, COL_BG);
    dotCache[i] = colour;
}

// Ring pattern per state. Each returns the colour for dot `i` this frame.
uint16_t ringColour(uint8_t i, uint32_t ms, uint32_t accent) {
    switch (state) {
        case AppState::Boot:
            return COL_TRACK;

        case AppState::Idle: {
            // Whole ring breathes together, slowly. Deliberately dim — this is
            // the state it sits in for hours.
            uint8_t b = 24 + (uint8_t)((uint32_t)breathe(ms, 4000) * 70 / 255);
            return dim565(accent, b);
        }

        case AppState::Listening: {
            // A comet: one bright head with a fading tail, chasing clockwise.
            uint8_t head = (ms / 40) % RING_DOTS;
            uint8_t back = (head + RING_DOTS - i) % RING_DOTS;  // 0 = head
            if (back >= 8) return COL_TRACK;
            return dim565(accent, (uint8_t)(255 - back * 30));
        }

        case AppState::Thinking: {
            // Two comets opposite each other, faster. Reads as "working".
            uint8_t head = (ms / 25) % RING_DOTS;
            uint8_t b1 = (head + RING_DOTS - i) % RING_DOTS;
            uint8_t b2 = (head + RING_DOTS / 2 + RING_DOTS - i) % RING_DOTS;
            uint8_t back = b1 < b2 ? b1 : b2;
            if (back >= 6) return COL_TRACK;
            return dim565(accent, (uint8_t)(255 - back * 40));
        }

        case AppState::Speaking: {
            // Standing wave — every dot lit, amplitude rippling around the ring.
            float phase = (float)i / RING_DOTS * 4.0f * (float)PI + (float)ms / 180.0f;
            uint8_t b = (uint8_t)(90.0f + 165.0f * (0.5f + 0.5f * sinf(phase)));
            return dim565(accent, b);
        }

        case AppState::Alert: {
            // Hard blink, no easing. It should be annoying.
            return ((ms / 250) % 2) ? dim565(accent, 255) : COL_TRACK;
        }
    }
    return COL_TRACK;
}

// Push the logo with every pixel scaled to `level` (0..255). One row is built in
// RAM at a time, so this costs a 480-byte buffer rather than a 60 KB copy of the
// whole image.
static_assert(logo_width <= SCREEN_W, "row buffer below assumes the logo is no wider than the panel");

void pushLogoAt(uint8_t level) {
    uint16_t row[logo_width];

    const int16_t x0 = (SCREEN_W - logo_width) / 2;
    const int16_t y0 = (SCREEN_H - logo_height) / 2;

    for (int16_t y = 0; y < logo_height; y++) {
        const uint16_t* src = &desk_logo[(uint32_t)y * logo_width];
        for (int16_t x = 0; x < logo_width; x++) {
            const uint16_t c = pgm_read_word(&src[x]);
            // Scale in-place in 565; the channels are independent so there's no
            // need to expand to 888 and back.
            const uint16_t r = (uint16_t)(((c >> 11) & 0x1F) * level / 255);
            const uint16_t g = (uint16_t)(((c >> 5) & 0x3F) * level / 255);
            const uint16_t b = (uint16_t)((c & 0x1F) * level / 255);
            row[x] = (uint16_t)((r << 11) | (g << 5) | b);
        }
        tft.pushImage(x0, y0 + y, logo_width, 1, row);
    }
}

void renderCentre(uint32_t accent) {
    centre.fillSprite(COL_BG);

    centre.setTextDatum(MC_DATUM);

    centre.setTextColor(to565(accent), COL_BG);
    centre.drawString(stateName(state), CENTRE_W / 2, CENTRE_H / 2 - 14, 4);

    if (message[0]) {
        centre.setTextColor(COL_LABEL, COL_BG);
        centre.drawString(message, CENTRE_W / 2, CENTRE_H / 2 + 20, 2);
    }

    centre.pushSprite(CENTRE_X, CENTRE_Y);
}

}  // namespace

namespace Ui {

void begin() {
    tft.init();
    tft.setRotation(0);  // square panel; rotation only matters for text origin
    tft.fillScreen(COL_BG);

    // logo.h is a native-endian uint16_t array; the panel wants high byte first.
    // Set once — pushLogoAt() builds its rows the same way.
    tft.setSwapBytes(true);

    for (uint8_t i = 0; i < RING_DOTS; i++) {
        float a = ((float)i / RING_DOTS) * 2.0f * (float)PI - (float)PI / 2.0f;  // i=0 at 12 o'clock
        dotX[i] = SCREEN_CX + (int16_t)lroundf(cosf(a) * RING_RADIUS);
        dotY[i] = SCREEN_CY + (int16_t)lroundf(sinf(a) * RING_RADIUS);
        dotCache[i] = 0xFFFF;  // impossible colour => first update() paints all
    }

    centre.setColorDepth(16);
    centre.createSprite(CENTRE_W, CENTRE_H);
    centre.fillSprite(COL_BG);
}

void splash(uint32_t holdMs) {
    tft.fillScreen(COL_BG);

    for (uint16_t level = 0; level <= 255; level += 15) {  // ~17 steps
        pushLogoAt((uint8_t)level);
    }
    pushLogoAt(255);  // land exactly on full, whatever the step size divides to

    delay(holdMs);

    tft.fillScreen(COL_BG);
    for (uint8_t i = 0; i < RING_DOTS; i++) dotCache[i] = 0xFFFF;  // screen is clear; cache is stale
    centreDirty = true;
}

void setState(AppState s) {
    if (s == state) return;
    state = s;
    centreDirty = true;
}

void setMessage(const char* msg) {
    strncpy(message, msg ? msg : "", sizeof(message) - 1);
    message[sizeof(message) - 1] = '\0';
    centreDirty = true;
}

void update() {
    static uint32_t last = 0;
    uint32_t now = millis();
    if (now - last < 25) return;  // ~40 fps ceiling; the ring is the only animation
    last = now;

    const uint32_t accent = styleFor(state).rgb;

    for (uint8_t i = 0; i < RING_DOTS; i++) {
        drawDot(i, ringColour(i, now, accent));
    }

    if (centreDirty) {
        renderCentre(accent);
        centreDirty = false;
    }
}

}  // namespace Ui
