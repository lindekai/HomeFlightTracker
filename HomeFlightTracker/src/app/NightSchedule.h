// Zeitfenster der Nachtruhe (auch über Mitternacht).
// Reines C++ ohne Arduino-Abhängigkeit, damit auf dem Entwicklungsrechner testbar (tests/host).
#pragma once

#include <stdint.h>

namespace night_schedule {

constexpr uint32_t kSecondsPerDay = 86400;

// true, wenn secondOfDay (0 … 86399, Ortszeit) im Fenster [startMin, endMin) liegt.
bool isInWindow(uint32_t secondOfDay, uint16_t startMin, uint16_t endMin);

// Sekunden von secondOfDay bis zur nächsten Uhrzeit targetMin (1 … 86400).
uint32_t secondsUntil(uint32_t secondOfDay, uint16_t targetMin);

}  // namespace night_schedule
