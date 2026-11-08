// HomeFlightTracker – Flugradar für AZ-Touch MOD (ESP32) mit Positionsdaten des OpenSky Network.
// Einrichtung: docs/setup.md, Architektur: docs/architecture.md

#include <TFT_eSPI.h>

#include "src/app/App.h"
#include "src/app/BootRequest.h"
#include "src/app/PowerManager.h"
#include "src/board/BoardPins.h"
#include "src/hwtest/HardwareTestApp.h"

#ifndef HOMEFLIGHTTRACKER_TFT_SETUP
#error "TFT_eSPI hat tft_setup.h aus dem Sketch-Ordner nicht geladen - siehe docs/setup.md"
#endif

static_assert(TFT_MOSI == board::kPinSpiMosi, "tft_setup.h und BoardPins.h widersprechen sich (MOSI)");
static_assert(TFT_MISO == board::kPinSpiMiso, "tft_setup.h und BoardPins.h widersprechen sich (MISO)");
static_assert(TFT_SCLK == board::kPinSpiSck, "tft_setup.h und BoardPins.h widersprechen sich (SCK)");
static_assert(TFT_CS == board::kPinTftCs, "tft_setup.h und BoardPins.h widersprechen sich (TFT_CS)");
static_assert(TFT_DC == board::kPinTftDc, "tft_setup.h und BoardPins.h widersprechen sich (TFT_DC)");
static_assert(TFT_RST == board::kPinTftReset, "tft_setup.h und BoardPins.h widersprechen sich (TFT_RST)");
static_assert(TFT_BL == board::kPinTftBacklight, "tft_setup.h und BoardPins.h widersprechen sich (TFT_BL)");
static_assert(TOUCH_CS == board::kPinTouchCs, "tft_setup.h und BoardPins.h widersprechen sich (TOUCH_CS)");

TFT_eSPI tft;
App app(tft);
HardwareTestApp diagnostics(tft);
bool diagnosticsMode = false;

void setup() {
  PowerManager::releaseSleepHolds();  // nach dem Tiefschlaf gehaltene Pin-Pegel freigeben

  // Piezo-Treiber hat keinen Pull-down: Pin sofort definiert auf LOW halten (stumm).
  pinMode(board::kPinBeeper, OUTPUT);
  digitalWrite(board::kPinBeeper, LOW);

  Serial.begin(115200);
  Serial.println();
  Serial.println("HomeFlightTracker");

  const BootRequest request = takeBootRequest();
  diagnosticsMode = request == BootRequest::Diagnostics;
  if (diagnosticsMode) {
    diagnostics.begin();
  } else {
    app.begin(request == BootRequest::Setup);
  }
}

void loop() {
  if (diagnosticsMode) {
    diagnostics.loop();
  } else {
    app.loop();
  }
}
