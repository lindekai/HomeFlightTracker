// ADS-B-Emitterkategorie aus OpenSky (Feld 17, nur mit extended=1): Symbolform und Klartext.
// Reines C++ ohne Arduino-Abhängigkeit, damit auf dem Entwicklungsrechner testbar (tests/host).
#pragma once

#include <stdint.h>

namespace aircraft_category {

// Reihenfolge entspricht icons::kAircraft (tools/gen_icons.py).
enum class Shape : uint8_t { Airliner = 0, Light = 1, Helicopter = 2 };

inline Shape shapeFor(uint8_t category) {
  switch (category) {
    case 8:  // Drehflügler
      return Shape::Helicopter;
    case 2:   // Leichtflugzeug (< 7 t)
    case 9:   // Segelflugzeug
    case 12:  // Ultraleicht
      return Shape::Light;
    default:  // ohne Angabe und alle übrigen Kategorien
      return Shape::Airliner;
  }
}

inline const char* name(uint8_t category) {
  switch (category) {
    case 2: return "Leichtflugzeug";
    case 3: return "Mittel (7-34 t)";
    case 4: return "Groß (34-136 t)";
    case 5: return "Groß (B757)";
    case 6: return "Schwer (>136 t)";
    case 7: return "Hochleistung";
    case 8: return "Hubschrauber";
    case 9: return "Segelflugzeug";
    case 10: return "Ballon";
    case 11: return "Fallschirm";
    case 12: return "Ultraleicht";
    case 14: return "Drohne";
    case 15: return "Raumfahrzeug";
    case 16:
    case 17: return "Bodenfahrzeug";
    case 18:
    case 19:
    case 20: return "Hindernis";
    default: return "k. A.";  // 0, 1, 13 und unbekannte Werte
  }
}

}  // namespace aircraft_category
