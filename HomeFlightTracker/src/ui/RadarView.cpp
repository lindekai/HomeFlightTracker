#include "RadarView.h"

#include <math.h>
#include <string.h>

#include "Widgets.h"

namespace {

constexpr float kDegToRad = static_cast<float>(M_PI / 180.0);

// Flugzeugsilhouette mit Nase nach oben (Bildschirm-y nach unten), als Dreiecke.
struct Point {
  float x;
  float y;
};
constexpr Point kShape[][3] = {
    {{0, -7}, {-1.6f, 5}, {1.6f, 5}},   // Rumpf
    {{0, -2}, {-7, 2.5f}, {7, 2.5f}},   // Tragflächen
    {{0, 3.5f}, {-3, 7}, {3, 7}},       // Leitwerk
};

}  // namespace

bool RadarView::begin(int16_t top) {
  top_ = top;
  sprite_.setColorDepth(8);
  spriteOk_ = sprite_.createSprite(kSize, kSize) != nullptr;
  Serial.printf("[radar] Sprite %s\n", spriteOk_ ? "aktiv (8 Bit)" : "nicht verfuegbar - direktes Zeichnen");
  return spriteOk_;
}

bool RadarView::contains(int16_t screenX, int16_t screenY) const {
  return screenX >= 0 && screenX < kSize && screenY >= top_ && screenY < top_ + kSize;
}

bool RadarView::project(const Aircraft& aircraft, uint16_t rangeKm, int16_t& x, int16_t& y) const {
  if (aircraft.distanceKm > rangeKm) return false;
  const float r = aircraft.distanceKm / rangeKm * kRadius;
  const float bearing = aircraft.bearingDeg * kDegToRad;
  x = kCenter + static_cast<int16_t>(lroundf(r * sinf(bearing)));
  y = kCenter - static_cast<int16_t>(lroundf(r * cosf(bearing)));
  return true;
}

void RadarView::draw(const FlightList& flights, const Info& info) {
  TFT_eSPI& canvas = spriteOk_ ? static_cast<TFT_eSPI&>(sprite_) : tft_;
  const int16_t oy = spriteOk_ ? 0 : top_;

  uint16_t visible = 0;
  for (uint16_t i = 0; i < flights.size(); ++i) visible += flights[i].distanceKm <= info.rangeKm ? 1 : 0;
  drawBackground(canvas, oy, info, visible);
  labelW_ = 0;

  if (info.dataState == DataState::Waiting || info.dataState == DataState::Expired) {
    canvas.setTextColor(ui::kColorMuted, ui::kColorBackground);
    const char* text = info.dataState == DataState::Expired ? "Keine aktuellen Daten" : info.waitingText;
    const int16_t w = ui::textWidth(canvas, text);
    canvas.fillRect(kCenter - w / 2 - 4, oy + kCenter + 14, w + 8, 18, ui::kColorBackground);
    ui::drawText(canvas, text, kCenter - w / 2, oy + kCenter + 16);
  } else {
    int16_t selectedIndex = -1;
    int16_t selectedX = 0, selectedY = 0;
    for (uint16_t i = flights.size(); i-- > 0;) {  // entfernte zuerst, nahe liegen oben
      const Aircraft& aircraft = flights[i];
      int16_t x, y;
      if (!project(aircraft, info.rangeKm, x, y)) continue;
      if (info.selectedIcao24[0] != '\0' && strcmp(aircraft.icao24, info.selectedIcao24) == 0) {
        selectedIndex = i;
        selectedX = x;
        selectedY = y;
        continue;
      }
      const int32_t reference = aircraft.timePosition > 0 ? aircraft.timePosition : aircraft.lastContact;
      const bool stalePosition = info.dataState == DataState::Stale || info.apiTime - reference > kStalePositionS;
      const uint16_t color = stalePosition ? ui::kColorStale : aircraft.onGround ? ui::kColorGround : ui::kColorAircraft;
      drawAircraft(canvas, x, y + oy, stalePosition ? NAN : aircraft.trackDeg, color);
    }
    if (selectedIndex >= 0) {
      const Aircraft& aircraft = flights[selectedIndex];
      canvas.drawCircle(selectedX, selectedY + oy, 11, ui::kColorSelected);
      drawAircraft(canvas, selectedX, selectedY + oy, aircraft.trackDeg, ui::kColorSelected);
      drawLabel(canvas, oy, aircraft, selectedX, selectedY);
    }
    if (info.dataState == DataState::Stale) {
      canvas.setTextColor(ui::kColorWarning, ui::kColorBackground);
      const char* text = "Daten veraltet";
      const int16_t w = ui::textWidth(canvas, text);
      ui::drawText(canvas, text, kCenter - w / 2, oy + kCenter + 40);
    }
  }

  if (spriteOk_) sprite_.pushSprite(0, top_);
}

void RadarView::drawBackground(TFT_eSPI& canvas, int16_t oy, const Info& info, uint16_t count) {
  canvas.fillRect(0, oy, kSize, kSize, ui::kColorBackground);
  canvas.drawLine(kCenter - kRadius, oy + kCenter, kCenter + kRadius, oy + kCenter, ui::kColorCross);
  canvas.drawLine(kCenter, oy + kCenter - kRadius, kCenter, oy + kCenter + kRadius, ui::kColorCross);
  for (int ring = 1; ring <= 4; ++ring) {
    canvas.drawCircle(kCenter, oy + kCenter, kRadius * ring / 4, ring % 2 == 0 ? ui::kColorRing : ui::kColorRingDim);
  }

  // Ringbeschriftung (halber und voller Radius) und Himmelsrichtungen
  char text[16];
  canvas.setTextColor(ui::kColorRingLabel, ui::kColorBackground);
  snprintf(text, sizeof(text), "%u", info.rangeKm / 2);
  ui::drawText(canvas, text, kCenter + 3, oy + kCenter - kRadius / 2 - 15);
  snprintf(text, sizeof(text), "%u km", info.rangeKm);
  ui::drawText(canvas, text, kCenter + 3, oy + kCenter - kRadius + 1);
  canvas.setTextColor(ui::kColorAccent, ui::kColorBackground);
  ui::drawText(canvas, "N", kCenter - 4, oy);
  canvas.setTextColor(ui::kColorRingLabel, ui::kColorBackground);
  ui::drawText(canvas, "O", kSize - 10, oy + kCenter - 8);
  ui::drawText(canvas, "S", kCenter - 4, oy + kSize - 16);
  ui::drawText(canvas, "W", 1, oy + kCenter - 8);

  // Ecken: Anzahl und Radius
  canvas.setTextColor(ui::kColorMuted, ui::kColorBackground);
  if (info.dataState != DataState::Waiting) {
    snprintf(text, sizeof(text), "%u Flugz.", count);
    ui::drawText(canvas, text, 2, oy);
  }
  snprintf(text, sizeof(text), "%u km", info.rangeKm);
  ui::drawText(canvas, text, kSize - 2, oy, 2, true);

  // Standort
  canvas.drawFastHLine(kCenter - 4, oy + kCenter, 9, ui::kColorText);
  canvas.drawFastVLine(kCenter, oy + kCenter - 4, 9, ui::kColorText);
}

void RadarView::drawAircraft(TFT_eSPI& canvas, int16_t x, int16_t y, float trackDeg, uint16_t color) {
  if (isnan(trackDeg)) {
    // Ohne Kursangabe kein Richtungssymbol, sondern ein Kreis.
    canvas.fillCircle(x, y, 3, color);
    return;
  }
  const float a = trackDeg * kDegToRad;
  const float s = sinf(a);
  const float c = cosf(a);
  for (const auto& triangle : kShape) {
    int16_t px[3], py[3];
    for (int i = 0; i < 3; ++i) {
      // Drehung im Uhrzeigersinn (Bildschirm-y zeigt nach unten).
      px[i] = x + static_cast<int16_t>(lroundf(triangle[i].x * c - triangle[i].y * s));
      py[i] = y + static_cast<int16_t>(lroundf(triangle[i].x * s + triangle[i].y * c));
    }
    canvas.fillTriangle(px[0], py[0], px[1], py[1], px[2], py[2], color);
  }
}

void RadarView::drawLabel(TFT_eSPI& canvas, int16_t oy, const Aircraft& aircraft, int16_t x, int16_t y) {
  char line1[16];
  char line2[32];
  char distance[12];
  snprintf(line1, sizeof(line1), "%s", aircraft.callsign[0] ? aircraft.callsign : aircraft.icao24);
  ui::formatDecimal(distance, sizeof(distance), aircraft.distanceKm, 1);
  if (isnan(aircraft.baroAltitudeM)) {
    snprintf(line2, sizeof(line2), "- m  %s km", distance);
  } else {
    snprintf(line2, sizeof(line2), "%.0f m  %s km", aircraft.baroAltitudeM, distance);
  }
  const int16_t w = max(ui::textWidth(canvas, line1), ui::textWidth(canvas, line2)) + 8;
  const int16_t h = 36;
  // Unterhalb des Symbols, bei Platzmangel darüber; seitlich im Sprite halten.
  int16_t lx = constrain(x - w / 2, 0, kSize - w);
  int16_t ly = y + 14;
  if (ly + h > kSize) ly = y - 14 - h;
  canvas.fillRoundRect(lx, oy + ly, w, h, 3, ui::kColorSelectedBg);
  canvas.drawRoundRect(lx, oy + ly, w, h, 3, ui::kColorSelected);
  canvas.setTextColor(ui::kColorSelectedText, ui::kColorSelectedBg);
  ui::drawText(canvas, line1, lx + 4, oy + ly + 2);
  ui::drawText(canvas, line2, lx + 4, oy + ly + 18);
  labelX_ = lx;
  labelY_ = top_ + ly;
  labelW_ = w;
  labelH_ = h;
}

uint8_t RadarView::hitTest(const FlightList& flights, uint16_t rangeKm, int16_t screenX, int16_t screenY,
                           uint16_t* out, uint8_t maxCount) const {
  const int16_t sy = screenY - top_;
  uint16_t indices[FlightList::kCapacity];
  int32_t distances[FlightList::kCapacity];
  uint8_t count = 0;
  for (uint16_t i = 0; i < flights.size(); ++i) {
    int16_t x, y;
    if (!project(flights[i], rangeKm, x, y)) continue;
    const int32_t d2 = (x - screenX) * (x - screenX) + (y - sy) * (y - sy);
    if (d2 > kHitRadius * kHitRadius) continue;
    // Nach Abstand sortiert einfügen
    uint8_t pos = count++;
    while (pos > 0 && distances[pos - 1] > d2) {
      distances[pos] = distances[pos - 1];
      indices[pos] = indices[pos - 1];
      --pos;
    }
    distances[pos] = d2;
    indices[pos] = i;
  }
  if (count > maxCount) count = maxCount;
  for (uint8_t i = 0; i < count; ++i) out[i] = indices[i];
  return count;
}

bool RadarView::labelHit(int16_t screenX, int16_t screenY) const {
  return labelW_ > 0 && screenX >= labelX_ && screenX < labelX_ + labelW_ && screenY >= labelY_ &&
         screenY < labelY_ + labelH_;
}
