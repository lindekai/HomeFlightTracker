// Geometrie für kleine Radarradien: Distanz, Peilung und Abfrage-Bounding-Box.
// Reines C++ ohne Arduino-Abhängigkeit, damit auf dem Entwicklungsrechner testbar (tests/host).
#pragma once

namespace geo {

constexpr double kEarthRadiusKm = 6371.0088;  // mittlerer Erdradius (IUGG)

struct LatLon {
  double lat;  // Grad, -90 … 90
  double lon;  // Grad, -180 … 180
};

struct BoundingBox {
  double latMin;
  double lonMin;
  double latMax;
  double lonMax;
  // true, wenn die Box über ±180° hinausreicht. lonMin/lonMax sind dann auf -180 … 180
  // normalisiert und es gilt lonMin > lonMax (Abfrage muss in zwei Teile zerlegt werden).
  bool crossesAntimeridian;
  // true, wenn ein Pol innerhalb der Box liegt; die Box umfasst dann alle Längengrade.
  bool containsPole;

  double latSpan() const { return latMax - latMin; }
  double lonSpan() const;
  double areaSquareDegrees() const { return latSpan() * lonSpan(); }
};

bool isValid(LatLon p);

// Großkreisentfernung (Haversine).
double distanceKm(LatLon from, LatLon to);

// Anfangspeilung vom Standort zum Ziel, 0° = Nord, im Uhrzeigersinn, Ergebnis 0 … < 360.
double initialBearingDeg(LatLon from, LatLon to);

// Kleinste Box, die den Kreis mit radiusKm um center vollständig enthält.
BoundingBox boundingBox(LatLon center, double radiusKm);

// Normalisiert einen Längengrad auf -180 … < 180.
double normalizeLon(double lon);

}  // namespace geo
