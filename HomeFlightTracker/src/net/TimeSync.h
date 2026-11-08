// Uhrzeit per NTP (Voraussetzung für die TLS-Zertifikatsprüfung und die Altersangaben).
#pragma once

#include <time.h>

namespace time_sync {

// Startet die NTP-Synchronisation mit mitteleuropäischer Zeitzone (inkl. Sommerzeit).
void begin();

// true, sobald eine plausible Uhrzeit vorliegt.
bool isSynced();

// Formatiert eine Unix-Zeit als lokale Uhrzeit hh:mm bzw. hh:mm:ss.
void formatLocalTime(time_t time, char* buffer, size_t size, bool withSeconds);

}  // namespace time_sync
