// Host-Unit-Tests für die Arduino-unabhängigen Module (Geometrie, OpenSky-Kontingent).
// Ausführen: tests/host/run.sh
#include <math.h>
#include <stdio.h>

#include <string.h>

#include "../../HomeFlightTracker/src/app/NightSchedule.h"
#include "../../HomeFlightTracker/src/geo/Geo.h"
#include "../../HomeFlightTracker/src/model/FlightList.h"
#include "../../HomeFlightTracker/src/opensky/Quota.h"
#include "../../HomeFlightTracker/src/opensky/StatesParser.h"

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

class StringSource : public opensky::JsonByteSource {
 public:
  explicit StringSource(const char* text) : text_(text) {}
  int read() override { return *text_ ? static_cast<unsigned char>(*text_++) : -1; }
  int peek() override { return *text_ ? static_cast<unsigned char>(*text_) : -1; }

 private:
  const char* text_;
};

bool parse(const char* json, FlightList& list, opensky::ParseStats& stats, double rangeKm = 50.0) {
  StringSource source(json);
  list.clear();
  return opensky::parseStates(source, kCenter, rangeKm, list, stats);
}

void testParser() {
  FlightList list;
  opensky::ParseStats stats;

  // Vollständiger Datensatz, Kurs 359°, Callsign mit Leerzeichen am Ende
  const char* full =
      "{\"time\":1762600000,\"states\":[[\"3c6589\",\"DLH123  \",\"Germany\",1762599998,1762599999,"
      "13.0,52.4,7600.5,false,206.1,359.0,-6.5,null,7725.0,\"1000\",false,0]]}";
  CHECK(parse(full, list, stats));
  CHECK(stats.apiTime == 1762600000);
  CHECK(stats.received == 1 && stats.inRange == 1 && list.size() == 1);
  CHECK(strcmp(list[0].callsign, "DLH123") == 0);
  CHECK(strcmp(list[0].icao24, "3c6589") == 0);
  CHECK(strcmp(list[0].squawk, "1000") == 0);
  CHECK_NEAR(list[0].trackDeg, 359.0, 1e-3);
  CHECK_NEAR(list[0].baroAltitudeM, 7600.5, 1e-3);
  CHECK(list[0].positionSource == 0);
  CHECK(list[0].timePosition == 1762599998);
  CHECK(list[0].bearingDeg > 270.0f);  // nordwestlich des Zentrums

  // Null-Felder: kein Callsign, keine Höhe/Geschwindigkeit/Kurs, kein Squawk, Kurs 0°
  const char* nulls =
      "{\"time\":1,\"states\":[[\"abc123\",null,\"Poland\",null,5,13.1,52.31,null,true,null,0,null,null,"
      "null,null,false,null]]}";
  CHECK(parse(nulls, list, stats));
  CHECK(list.size() == 1);
  CHECK(list[0].callsign[0] == '\0');
  CHECK(isnan(list[0].baroAltitudeM) && isnan(list[0].velocityMs) && isnan(list[0].verticalRateMs));
  CHECK_NEAR(list[0].trackDeg, 0.0, 1e-6);
  CHECK(list[0].squawk[0] == '\0');
  CHECK(list[0].timePosition == 0 && list[0].lastContact == 5);
  CHECK(list[0].onGround);
  CHECK(list[0].positionSource == 255);

  // Fehlende Koordinaten, außerhalb des Radius, doppelte Callsigns
  const char* mixed =
      "{\"time\":2,\"states\":[\n"
      "[\"a00001\",\"DUP1\",\"X\",1,1,null,null,1000,false,100,90,0,null,1000,null,false,0],\n"
      "[\"a00002\",\"DUP1\",\"X\",1,1,13.2,52.35,1000,false,100,90,0,null,1000,null,false,0],\n"
      "[\"a00003\",\"DUP1\",\"X\",1,1,13.3,52.32,1000,false,100,90,0,null,1000,null,false,0],\n"
      "[\"a00004\",\"FAR\",\"X\",1,1,16.0,52.3,1000,false,100,90,0,null,1000,null,false,0]\n"
      "]}";
  CHECK(parse(mixed, list, stats));
  CHECK(stats.received == 4 && stats.withPosition == 3 && stats.inRange == 2 && list.size() == 2);

  // Leere Ergebnisse
  CHECK(parse("{\"time\":3,\"states\":null}", list, stats));
  CHECK(list.size() == 0 && stats.apiTime == 3);
  CHECK(parse("{\"time\":4,\"states\":[]}", list, stats));
  CHECK(list.size() == 0 && stats.received == 0);

  // Abgebrochener Datenstrom und falsches Format
  CHECK(!parse("{\"time\":5,\"states\":[[\"a\",\"B\",\"C\",1,1,13.1,52.3", list, stats));
  CHECK(!parse("<html>Too many requests</html>", list, stats));
  CHECK(!parse("{\"time\":6,\"states\":[[1,2]x", list, stats));
}

void testFlightList() {
  FlightList list;
  list.clear();
  Aircraft aircraft{};
  for (int i = 0; i < FlightList::kCapacity; ++i) {
    aircraft.distanceKm = 10.0f + i;
    list.add(aircraft);
  }
  CHECK(list.size() == FlightList::kCapacity);
  aircraft.distanceKm = 5.0f;  // näher als alle: ersetzt das entfernteste (109 km)
  list.add(aircraft);
  aircraft.distanceKm = 500.0f;  // weiter als alle: verworfen
  list.add(aircraft);
  CHECK(list.size() == FlightList::kCapacity && list.dropped() == 2);
  list.sortByDistance();
  CHECK_NEAR(list[0].distanceKm, 5.0, 1e-6);
  CHECK_NEAR(list[FlightList::kCapacity - 1].distanceKm, 108.0, 1e-6);
  bool sorted = true;
  for (int i = 1; i < list.size(); ++i) sorted &= list[i - 1].distanceKm <= list[i].distanceKm;
  CHECK(sorted);
}

void testNightSchedule() {
  using night_schedule::isInWindow;
  using night_schedule::secondsUntil;
  const uint16_t start = 23 * 60, end = 6 * 60;  // über Mitternacht
  CHECK(isInWindow(23 * 3600, start, end));
  CHECK(isInWindow(2 * 3600, start, end));
  CHECK(!isInWindow(6 * 3600, start, end));        // Ende exklusiv
  CHECK(isInWindow(6 * 3600 - 1, start, end));
  CHECK(!isInWindow(12 * 3600, start, end));
  CHECK(!isInWindow(23 * 3600 - 1, start, end));
  CHECK(isInWindow(13 * 3600, 12 * 60, 14 * 60));  // innerhalb eines Tages
  CHECK(!isInWindow(15 * 3600, 12 * 60, 14 * 60));
  CHECK(!isInWindow(13 * 3600, 600, 600));         // leeres Fenster
  CHECK(secondsUntil(23 * 3600, end) == 7 * 3600);
  CHECK(secondsUntil(5 * 3600 + 59 * 60 + 30, end) == 30);
  CHECK(secondsUntil(6 * 3600, end) == 86400);
}

}  // namespace

int main() {
  testDistance();
  testBearing();
  testBoundingBox();
  testValidity();
  testQuota();
  testParser();
  testFlightList();
  testNightSchedule();
  printf("%d Prüfungen, %d Fehler\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
