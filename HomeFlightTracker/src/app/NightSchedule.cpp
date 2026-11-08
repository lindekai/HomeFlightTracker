#include "NightSchedule.h"

bool night_schedule::isInWindow(uint32_t secondOfDay, uint16_t startMin, uint16_t endMin) {
  const uint32_t start = startMin * 60UL;
  const uint32_t end = endMin * 60UL;
  if (start == end) return false;
  if (start < end) return secondOfDay >= start && secondOfDay < end;
  return secondOfDay >= start || secondOfDay < end;  // über Mitternacht
}

uint32_t night_schedule::secondsUntil(uint32_t secondOfDay, uint16_t targetMin) {
  const uint32_t target = targetMin * 60UL;
  const uint32_t delta = (target + kSecondsPerDay - secondOfDay) % kSecondsPerDay;
  return delta == 0 ? kSecondsPerDay : delta;
}
