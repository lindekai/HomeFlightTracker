// Umrechnung in Luftfahrt-Einheiten (Fuß, Knoten, Fuß pro Minute, Seemeilen, Flugfläche).
// Reines C++ ohne Arduino-Abhängigkeit, damit auf dem Entwicklungsrechner testbar (tests/host).
#pragma once

#include <math.h>

namespace units {

constexpr float kMetersPerFoot = 0.3048f;
constexpr float kKmPerNauticalMile = 1.852f;

constexpr float metersToFeet(float meters) { return meters / kMetersPerFoot; }
constexpr float msToKnots(float ms) { return ms * 3.6f / kKmPerNauticalMile; }
constexpr float msToFeetPerMinute(float ms) { return ms * 60.0f / kMetersPerFoot; }
constexpr float kmToNauticalMiles(float km) { return km / kKmPerNauticalMile; }

// Flugfläche aus der barometrischen Höhe (Standardatmosphäre, 1013,25 hPa): Hunderte Fuß.
// Liefert -1 für fehlende oder negative Höhen.
inline long flightLevel(float baroMeters) {
  if (isnan(baroMeters) || baroMeters < 0.0f) return -1;
  return lroundf(metersToFeet(baroMeters) / 100.0f);
}

}  // namespace units
