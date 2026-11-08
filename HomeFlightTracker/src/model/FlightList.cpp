#include "FlightList.h"

void FlightList::add(const Aircraft& aircraft) {
  if (size_ < kCapacity) {
    items_[size_++] = aircraft;
    return;
  }
  uint16_t farthest = 0;
  for (uint16_t i = 1; i < size_; ++i) {
    if (items_[i].distanceKm > items_[farthest].distanceKm) farthest = i;
  }
  ++dropped_;
  if (aircraft.distanceKm < items_[farthest].distanceKm) items_[farthest] = aircraft;
}

void FlightList::sortByDistance() {
  // Einfügesortierung: wenige Einträge, keine dynamische Allokation.
  for (uint16_t i = 1; i < size_; ++i) {
    const Aircraft current = items_[i];
    uint16_t j = i;
    while (j > 0 && items_[j - 1].distanceKm > current.distanceKm) {
      items_[j] = items_[j - 1];
      --j;
    }
    items_[j] = current;
  }
}
