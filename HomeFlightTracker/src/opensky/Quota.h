// OpenSky-Credit-Berechnung für /states/all (Stand der Dokumentation: siehe docs/opensky.md).
// Reines C++ ohne Arduino-Abhängigkeit, damit auf dem Entwicklungsrechner testbar (tests/host).
#pragma once

#include <stdint.h>

namespace opensky {

constexpr uint32_t kDailyCreditsAuthenticated = 4000;
constexpr uint32_t kDailyCreditsAnonymous = 400;

// Höchstens dieser Anteil des Tagesbudgets darf durch das reguläre Abrufintervall verplant werden;
// der Rest bleibt als Reserve für Wiederholungen nach Fehlern.
constexpr uint8_t kMaxPlannedBudgetPercent = 80;

constexpr uint16_t kMinIntervalAuthenticatedS = 30;  // kürzer bringt bei 5 s Zeitauflösung kaum Nutzen
constexpr uint16_t kMinIntervalAnonymousS = 300;
constexpr uint16_t kMaxIntervalS = 900;

// Die Abfrage-Box wird um diesen Faktor größer als der Radarradius gewählt, damit Flugzeuge
// am Rand nicht durch Rundung oder Projektionsfehler herausfallen.
constexpr double kQueryRadiusFactor = 1.05;

// Credits pro Abruf in Abhängigkeit von der Fläche der Bounding Box in Quadratgrad.
uint8_t creditsPerRequest(double areaSquareDegrees);

uint32_t dailyCredits(uint16_t intervalS, uint8_t creditsPerRequest);

// Kürzestes zulässiges Intervall für die gegebene Box-Fläche und Nutzerstufe.
uint16_t minIntervalSeconds(double areaSquareDegrees, bool authenticated);

bool isIntervalAllowed(uint16_t intervalS, double areaSquareDegrees, bool authenticated);

}  // namespace opensky
