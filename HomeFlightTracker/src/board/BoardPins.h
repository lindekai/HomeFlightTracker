// Feste ESP32-Verdrahtung der AZ-Touch-MOD-Trägerplatine V01-03 (Schaltplan V01-03-01).
// Quelle und Prüfstatus: docs/hardware.md. Display-/Touch-Pins stehen zusätzlich in tft_setup.h
// und werden in HomeFlightTracker.ino per static_assert gegen diese Werte geprüft.
#pragma once

#include <stdint.h>

namespace board {

// Gemeinsamer SPI-Bus (VSPI)
constexpr uint8_t kPinSpiMosi = 23;
constexpr uint8_t kPinSpiMiso = 19;
constexpr uint8_t kPinSpiSck = 18;

// Display ILI9341
constexpr uint8_t kPinTftCs = 5;
constexpr uint8_t kPinTftDc = 4;
constexpr uint8_t kPinTftReset = 22;
constexpr uint8_t kPinTftBacklight = 15;  // aktiv LOW (PNP-Treiber)

// Touch XPT2046
constexpr uint8_t kPinTouchCs = 14;
constexpr uint8_t kPinTouchIrq = 27;  // PENIRQ, aktiv LOW

// Piezo LS1 über NPN BC817-40, aktiv HIGH. Im MVP nicht genutzt, wird beim Start auf LOW gehalten.
constexpr uint8_t kPinBeeper = 21;

}  // namespace board
