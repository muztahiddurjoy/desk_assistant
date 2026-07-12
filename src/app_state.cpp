#include "app_state.h"

const char* stateName(AppState s) {
    switch (s) {
        case AppState::Boot:      return "BOOT";
        case AppState::Idle:      return "IDLE";
        case AppState::Listening: return "LISTENING";
        case AppState::Thinking:  return "THINKING";
        case AppState::Speaking:  return "SPEAKING";
        case AppState::Alert:     return "ALERT";
    }
    return "?";
}

// Pack 8-8-8 into 5-6-5. Kept here so the two colour spaces below can be read
// off the same hex literal and cannot drift apart.
static constexpr uint16_t to565(uint32_t rgb) {
    return (uint16_t)(((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x001F));
}

StateStyle styleFor(AppState s) {
    uint32_t rgb;
    switch (s) {
        case AppState::Boot:      rgb = 0x303840; break;  // slate, barely there
        case AppState::Idle:      rgb = 0x1E90FF; break;  // calm blue
        case AppState::Listening: rgb = 0x00E5A0; break;  // green, "go ahead"
        case AppState::Thinking:  rgb = 0xB060FF; break;  // violet
        case AppState::Speaking:  rgb = 0xFFC021; break;  // warm amber
        case AppState::Alert:     rgb = 0xFF2D45; break;  // red
        default:                  rgb = 0x1E90FF; break;
    }
    return StateStyle{ to565(rgb), rgb };
}
