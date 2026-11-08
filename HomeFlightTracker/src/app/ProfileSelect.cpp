#include "ProfileSelect.h"

#include <string.h>

namespace {

// Stärkstes Signal des WLANs ssid, INT16_MIN wenn nicht sichtbar.
int16_t strongestRssi(const char* ssid, const profile_select::VisibleNetwork* visible, uint8_t visibleCount) {
  int16_t best = INT16_MIN;
  for (uint8_t i = 0; i < visibleCount; ++i) {
    if (strcmp(visible[i].ssid, ssid) == 0 && visible[i].rssi > best) best = visible[i].rssi;
  }
  return best;
}

}  // namespace

int8_t profile_select::choose(const char* const* profileSsids, const bool* allowed, uint8_t profileCount,
                              int8_t active, const VisibleNetwork* visible, uint8_t visibleCount) {
  const bool activeValid = active >= 0 && active < profileCount;
  if (activeValid && strongestRssi(profileSsids[active], visible, visibleCount) != INT16_MIN) return active;

  int8_t best = -1;
  int16_t bestRssi = INT16_MIN;
  for (uint8_t i = 0; i < profileCount; ++i) {
    if (!allowed[i] || profileSsids[i][0] == '\0') continue;
    const int16_t rssi = strongestRssi(profileSsids[i], visible, visibleCount);
    if (rssi != INT16_MIN && rssi > bestRssi) {
      best = static_cast<int8_t>(i);
      bestRssi = rssi;
    }
  }
  return best >= 0 ? best : active;
}
