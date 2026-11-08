#include "TimeSync.h"

#include <Arduino.h>

namespace {

constexpr char kTimeZone[] = "CET-1CEST,M3.5.0,M10.5.0/3";  // Europa/Berlin
constexpr time_t kPlausibleEpoch = 1735689600;              // 2025-01-01

}  // namespace

void time_sync::begin() {
  configTzTime(kTimeZone, "pool.ntp.org", "time.cloudflare.com", "ptbtime1.ptb.de");
}

bool time_sync::isSynced() {
  return time(nullptr) > kPlausibleEpoch;
}

void time_sync::formatLocalTime(time_t time, char* buffer, size_t size, bool withSeconds) {
  struct tm local;
  localtime_r(&time, &local);
  strftime(buffer, size, withSeconds ? "%H:%M:%S" : "%H:%M", &local);
}
