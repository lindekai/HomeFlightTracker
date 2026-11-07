#include "Geo.h"

#include <math.h>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;
constexpr double kRadToDeg = 180.0 / kPi;

}  // namespace

double geo::BoundingBox::lonSpan() const {
  if (containsPole) return 360.0;
  return crossesAntimeridian ? (lonMax + 360.0) - lonMin : lonMax - lonMin;
}

bool geo::isValid(LatLon p) {
  return isfinite(p.lat) && isfinite(p.lon) && p.lat >= -90.0 && p.lat <= 90.0 && p.lon >= -180.0 && p.lon <= 180.0;
}

double geo::distanceKm(LatLon from, LatLon to) {
  const double lat1 = from.lat * kDegToRad;
  const double lat2 = to.lat * kDegToRad;
  const double dLat = lat2 - lat1;
  const double dLon = (to.lon - from.lon) * kDegToRad;
  const double a = sin(dLat / 2) * sin(dLat / 2) + cos(lat1) * cos(lat2) * sin(dLon / 2) * sin(dLon / 2);
  return 2.0 * kEarthRadiusKm * atan2(sqrt(a), sqrt(1.0 - a));
}

double geo::initialBearingDeg(LatLon from, LatLon to) {
  const double lat1 = from.lat * kDegToRad;
  const double lat2 = to.lat * kDegToRad;
  const double dLon = (to.lon - from.lon) * kDegToRad;
  const double y = sin(dLon) * cos(lat2);
  const double x = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(dLon);
  const double bearing = fmod(atan2(y, x) * kRadToDeg + 360.0, 360.0);
  return bearing >= 360.0 ? 0.0 : bearing;
}

double geo::normalizeLon(double lon) {
  double result = fmod(lon + 180.0, 360.0);
  if (result < 0) result += 360.0;
  return result - 180.0;
}

geo::BoundingBox geo::boundingBox(LatLon center, double radiusKm) {
  const double angular = radiusKm / kEarthRadiusKm;  // Winkelradius in rad
  const double dLat = angular * kRadToDeg;

  BoundingBox box{};
  box.latMin = center.lat - dLat;
  box.latMax = center.lat + dLat;

  if (box.latMax >= 90.0 || box.latMin <= -90.0) {
    box.latMin = box.latMin < -90.0 ? -90.0 : box.latMin;
    box.latMax = box.latMax > 90.0 ? 90.0 : box.latMax;
    box.lonMin = -180.0;
    box.lonMax = 180.0;
    box.containsPole = true;
    return box;
  }

  // Längenausdehnung des Kreises an seiner breitesten Stelle (Tangentenpunkte).
  const double dLon = asin(sin(angular) / cos(center.lat * kDegToRad)) * kRadToDeg;
  const double rawMin = center.lon - dLon;
  const double rawMax = center.lon + dLon;
  box.crossesAntimeridian = rawMin < -180.0 || rawMax > 180.0;
  box.lonMin = box.crossesAntimeridian ? normalizeLon(rawMin) : rawMin;
  box.lonMax = box.crossesAntimeridian ? normalizeLon(rawMax) : rawMax;
  return box;
}
