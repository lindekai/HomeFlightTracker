// Startwunsch über einen Software-Neustart hinweg (RTC-Speicher, übersteht keinen Stromausfall).
// Dadurch startet nach einem Stromausfall nie automatisch der Setup- oder Diagnosemodus.
#pragma once

#include <stdint.h>

enum class BootRequest : uint32_t {
  None = 0,
  Setup = 0x53455455,        // "SETU"
  Diagnostics = 0x44494147,  // "DIAG"
};

// Liest den Startwunsch und löscht ihn. Gültig nur nach einem Software-Neustart.
BootRequest takeBootRequest();

// Neustart mit Startwunsch; kehrt nicht zurück.
[[noreturn]] void restartWith(BootRequest request);
