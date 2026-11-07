// Host-Unit-Tests für die Arduino-unabhängigen Module (Geometrie, OpenSky-Kontingent).
// Ausführen: tests/host/run.sh
#include <math.h>
#include <stdio.h>

#include "../../HomeFlightTracker/src/geo/Geo.h"
#include "../../HomeFlightTracker/src/opensky/Quota.h"

namespace {

int failures = 0;
int checks = 0;

void check(bool condition, const char* what, int line) {
  ++checks;
  if (!condition) {
    ++failures;
    printf("FEHLER (Zeile %d): %s\n", line, what);
  }
}

bool near(double actual, double expected, double tolerance) {
  return fabs(actual - expected) <= tolerance;
}

#define CHECK(cond) check((cond), #cond, __LINE__)
#define CHECK_NEAR(a, e, tol) check(near((a), (e), (tol)), #a " ~ " #e, __LINE__)

const geo::LatLon kCenter{52.3, 13.1};

void testDistance() {
  CHECK_NEAR(geo::distanceKm(kCenter, kCenter), 0.0, 1e-9);
  // 1° Breite ≈ 111,195 km
  CHECK_NEAR(geo::distanceKm({0, 0}, {1, 0}), 111.195, 0.01);
  // Berlin – München (Referenzwert ca. 504 km)
  CHECK_NEAR(geo::distanceKm({52.52, 13.405}, {48.137, 11.575}), 504.0, 2.0);
  // symmetrisch
  CHECK_NEAR(geo::distanceKm({10, 20}, {-5, 40}), geo::distanceKm({-5, 40}, {10, 20}), 1e-9);
}

void testBearing() {
  CHECK_NEAR(geo::initialBearingDeg(kCenter, {53.0, 13.1}), 0.0, 1e-6);    // Nord
  CHECK_NEAR(geo::initialBearingDeg(kCenter, {51.5, 13.1}), 180.0, 1e-6);  // Süd
  CHECK_NEAR(geo::initialBearingDeg({0, 0}, {0, 1}), 90.0, 1e-6);          // Ost
  CHECK_NEAR(geo::initialBearingDeg({0, 0}, {0, -1}), 270.0, 1e-6);        // West
  const double nw = geo::initialBearingDeg(kCenter, {52.4, 12.9});
  CHECK(nw > 270.0 && nw < 360.0);
  // Ergebnis immer 0 … < 360
  CHECK(geo::initialBearingDeg(kCenter, {52.3001, 13.0999}) < 360.0);
}

void testBoundingBox() {
  const geo::BoundingBox box = geo::boundingBox(kCenter, 100.0);
  CHECK(!box.crossesAntimeridian);
  CHECK(!box.containsPole);
  CHECK_NEAR(box.latMax - kCenter.lat, 100.0 / 111.195, 0.001);
  // Längenausdehnung wächst mit 1/cos(Breite)
  CHECK(box.lonSpan() > box.latSpan());
  // Punkte genau auf dem Kreis in allen vier Richtungen liegen in der Box
  const double d = 100.0 / geo::kEarthRadiusKm * 180.0 / M_PI;
  CHECK(kCenter.lat + d <= box.latMax + 1e-9 && kCenter.lat - d >= box.latMin - 1e-9);
  const geo::LatLon east{kCenter.lat, box.lonMax};
  CHECK(geo::distanceKm(kCenter, east) >= 100.0 - 0.01);
  // Ca. 5,3 Quadratgrad bei 100 km um 52,3° N → 1 Credit
  CHECK_NEAR(box.areaSquareDegrees(), 5.3, 0.2);

  // Datumsgrenze
  const geo::BoundingBox dateline = geo::boundingBox({0.0, 179.8}, 50.0);
  CHECK(dateline.crossesAntimeridian);
  CHECK(dateline.lonMin > dateline.lonMax);
  CHECK_NEAR(dateline.lonSpan(), 2 * 50.0 / 111.195, 0.01);

  // Polnähe
  const geo::BoundingBox polar = geo::boundingBox({89.5, 0.0}, 100.0);
  CHECK(polar.containsPole);
  CHECK_NEAR(polar.latMax, 90.0, 1e-9);
  CHECK_NEAR(polar.lonSpan(), 360.0, 1e-9);

  // Hohe Breite ohne Pol: Box deutlich breiter als hoch
  const geo::BoundingBox high = geo::boundingBox({70.0, 20.0}, 100.0);
  CHECK(!high.containsPole);
  CHECK(high.lonSpan() > 2.5 * high.latSpan());
}

void testValidity() {
  CHECK(geo::isValid({0, 0}));
  CHECK(geo::isValid({-90, 180}));
  CHECK(!geo::isValid({90.1, 0}));
  CHECK(!geo::isValid({0, -180.1}));
  CHECK(!geo::isValid({NAN, 0}));
  CHECK_NEAR(geo::normalizeLon(190.0), -170.0, 1e-9);
  CHECK_NEAR(geo::normalizeLon(-190.0), 170.0, 1e-9);
  CHECK_NEAR(geo::normalizeLon(180.0), -180.0, 1e-9);
}

void testQuota() {
  CHECK(opensky::creditsPerRequest(5.3) == 1);
  CHECK(opensky::creditsPerRequest(25.0) == 1);
  CHECK(opensky::creditsPerRequest(25.1) == 2);
  CHECK(opensky::creditsPerRequest(100.0) == 2);
  CHECK(opensky::creditsPerRequest(400.0) == 3);
  CHECK(opensky::creditsPerRequest(400.1) == 4);

  CHECK(opensky::dailyCredits(60, 1) == 1440);
  CHECK(opensky::dailyCredits(30, 1) == 2880);
  CHECK(opensky::dailyCredits(300, 1) == 288);

  CHECK(opensky::minIntervalSeconds(5.3, true) == 30);
  CHECK(opensky::minIntervalSeconds(5.3, false) == 300);
  // 2 Credits: 3200 / 2 = 1600 Abrufe → mindestens 54 s
  CHECK(opensky::minIntervalSeconds(50.0, true) == 54);
  CHECK(opensky::dailyCredits(opensky::minIntervalSeconds(50.0, true), 2) <= 3200);

  CHECK(opensky::isIntervalAllowed(60, 5.3, true));
  CHECK(!opensky::isIntervalAllowed(20, 5.3, true));
  CHECK(!opensky::isIntervalAllowed(60, 5.3, false));
  CHECK(!opensky::isIntervalAllowed(1000, 5.3, true));
}

}  // namespace

int main() {
  testDistance();
  testBearing();
  testBoundingBox();
  testValidity();
  testQuota();
  printf("%d Prüfungen, %d Fehler\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
