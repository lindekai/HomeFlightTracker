// Automatische Profilwahl beim Start anhand der sichtbaren WLANs.
// Reines C++ ohne Arduino-Abhängigkeit, damit auf dem Entwicklungsrechner testbar (tests/host).
#pragma once

#include <stdint.h>

namespace profile_select {

struct VisibleNetwork {
  const char* ssid;
  int8_t rssi;  // dBm
};

// Wählt das Profil für den Start:
// 1. Das aktive Profil, wenn sein WLAN sichtbar ist.
// 2. Sonst das zulässige Profil (allowed[i]) mit dem stärksten sichtbaren WLAN.
// 3. Sonst bleibt das aktive Profil.
// profileSsids und allowed haben profileCount Einträge; active ist -1, wenn keins aktiv ist.
int8_t choose(const char* const* profileSsids, const bool* allowed, uint8_t profileCount, int8_t active,
              const VisibleNetwork* visible, uint8_t visibleCount);

}  // namespace profile_select
