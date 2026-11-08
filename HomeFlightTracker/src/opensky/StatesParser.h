// Streaming-Auswertung der Antwort von /states/all: jeder State-Vektor wird einzeln gelesen,
// sodass der Speicherbedarf nicht mit der Zahl der Flugzeuge wächst.
// Arduino-unabhängig (nur ArduinoJson), damit auf dem Entwicklungsrechner testbar (tests/host).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../geo/Geo.h"
#include "../model/FlightList.h"

namespace opensky {

// Zeichenquelle für den Parser (HTTP-Stream auf dem Gerät, Text im Test).
class JsonByteSource {
 public:
  virtual ~JsonByteSource() = default;
  virtual int read() = 0;  // -1 = Ende/Timeout
  virtual int peek() = 0;

  // Für ArduinoJson (Custom Reader).
  size_t readBytes(char* buffer, size_t length);
};

struct ParseStats {
  int64_t apiTime = 0;        // Zeitstempel der Antwort (Unix)
  uint16_t received = 0;      // State-Vektoren in der Antwort
  uint16_t withPosition = 0;  // davon mit gültiger Position
  uint16_t inRange = 0;       // davon innerhalb des Radius
};

// Liest {"time":…,"states":[…]} und übernimmt alle Flugzeuge mit Position innerhalb von rangeKm
// um center in out (wird vorher NICHT geleert, damit Teilabfragen zusammengeführt werden können).
// Liefert false bei unerwartetem Format oder abgebrochenem Datenstrom.
bool parseStates(JsonByteSource& in, geo::LatLon center, double rangeKm, FlightList& out, ParseStats& stats);

}  // namespace opensky
