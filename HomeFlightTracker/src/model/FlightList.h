// Flugzeugdaten aus OpenSky-State-Vektoren in einer Tabelle fester Größe.
// Reines C++ ohne Arduino-Abhängigkeit, damit auf dem Entwicklungsrechner testbar (tests/host).
#pragma once

#include <stdint.h>

struct Aircraft {
  char icao24[7];
  char callsign[9];         // leer = nicht verfügbar
  char originCountry[32];
  char squawk[5];           // leer = nicht verfügbar
  float latitude;
  float longitude;
  float baroAltitudeM;      // NAN = nicht verfügbar
  float geoAltitudeM;       // NAN = nicht verfügbar
  float velocityMs;         // NAN = nicht verfügbar
  float trackDeg;           // 0° = Nord, im Uhrzeigersinn; NAN = nicht verfügbar
  float verticalRateMs;     // NAN = nicht verfügbar
  int32_t timePosition;     // Unix-Zeit der letzten Position, 0 = unbekannt
  int32_t lastContact;      // Unix-Zeit des letzten Kontakts, 0 = unbekannt
  bool onGround;
  uint8_t positionSource;   // 0 ADS-B, 1 ASTERIX, 2 MLAT, 3 FLARM, 255 = unbekannt
  uint8_t category;         // ADS-B-Emitterkategorie (0 = keine Angabe), siehe AircraftCategory.h
  float distanceKm;         // vom Radarzentrum
  float bearingDeg;         // vom Radarzentrum, 0° = Nord
};

class FlightList {
 public:
  static constexpr uint16_t kCapacity = 100;

  void clear() {
    size_ = 0;
    dropped_ = 0;
  }

  // Fügt ein Flugzeug hinzu. Ist die Tabelle voll, ersetzt es das am weitesten entfernte,
  // sofern es selbst näher liegt; sonst wird es verworfen (zählt in dropped()).
  void add(const Aircraft& aircraft);

  void sortByDistance();

  uint16_t size() const { return size_; }
  uint16_t dropped() const { return dropped_; }
  const Aircraft& operator[](uint16_t index) const { return items_[index]; }

 private:
  Aircraft items_[kCapacity];
  uint16_t size_ = 0;
  uint16_t dropped_ = 0;
};
