// Radar-Darstellung: Entfernungsringe, Nordausrichtung, Flugzeugsymbole in Bewegungsrichtung,
// Auswahl mit Kurzinfo. Gezeichnet wird flackerfrei in ein 8-Bit-Sprite (240 × 240, ca. 57 KB);
// steht der Speicher nicht zur Verfügung, wird direkt auf das Display gezeichnet.
// Projektion: azimutal äquidistant um den Standort (Abstand und Peilung exakt).
#pragma once

#include <TFT_eSPI.h>

#include "../model/FlightList.h"

class RadarView {
 public:
  enum class DataState : uint8_t { Waiting, Fresh, Stale, Expired };

  struct Info {
    uint16_t rangeKm = 50;
    DataState dataState = DataState::Waiting;
    int64_t apiTime = 0;          // Zeitstempel der Daten für das Positionsalter
    const char* waitingText = "";  // Hinweis, solange keine Daten vorliegen
    // Flugzeug mit Kurzinfo: das ausgewählte (orange hervorgehoben) oder ohne Auswahl
    // das nächstgelegene (neutrale Kurzinfo).
    const char* labelIcao24 = "";
    bool labelIsSelection = false;
  };

  static constexpr int16_t kSize = 240;
  static constexpr uint32_t kStalePositionS = 60;  // ältere Positionen grau

  explicit RadarView(TFT_eSPI& tft) : tft_(tft), sprite_(&tft) {}

  // Reserviert das Sprite; muss vor dem Aufbau von TLS-Verbindungen erfolgen (zusammenhängender Speicher).
  bool begin(int16_t top);
  bool usesSprite() const { return spriteOk_; }

  void draw(const FlightList& flights, const Info& info);

  // Indizes der Flugzeuge im Trefferradius um den Bildschirmpunkt, nach Abstand sortiert.
  uint8_t hitTest(const FlightList& flights, uint16_t rangeKm, int16_t screenX, int16_t screenY, uint16_t* out,
                  uint8_t maxCount) const;
  bool labelHit(int16_t screenX, int16_t screenY) const;
  bool contains(int16_t screenX, int16_t screenY) const;

 private:
  static constexpr int16_t kCenter = kSize / 2;
  static constexpr int16_t kRadius = 112;
  static constexpr int16_t kHitRadius = 20;

  bool project(const Aircraft& aircraft, uint16_t rangeKm, int16_t& x, int16_t& y) const;
  void drawBackground(TFT_eSPI& canvas, int16_t oy, const Info& info, uint16_t count);
  void drawAircraft(TFT_eSPI& canvas, int16_t x, int16_t y, float trackDeg, uint8_t category, uint16_t color);
  void drawLabel(TFT_eSPI& canvas, int16_t oy, const Aircraft& aircraft, int16_t x, int16_t y, bool selected);
  uint16_t aircraftColor(const Aircraft& aircraft, const Info& info) const;

  TFT_eSPI& tft_;
  TFT_eSprite sprite_;
  bool spriteOk_ = false;
  int16_t top_ = 0;
  // Kurzinfo-Feld des ausgewählten Flugzeugs (Bildschirmkoordinaten), w = 0: keins
  int16_t labelX_ = 0, labelY_ = 0, labelW_ = 0, labelH_ = 0;
};
