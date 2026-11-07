#include "HeapReport.h"

#include <Esp.h>

HeapSnapshot takeHeapSnapshot() {
  return HeapSnapshot{ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap()};
}

void printHeapSnapshot(Print& out, const char* label) {
  const HeapSnapshot heap = takeHeapSnapshot();
  out.printf("[heap] %-24s frei=%6lu  min=%6lu  maxBlock=%6lu\n", label,
             static_cast<unsigned long>(heap.freeBytes), static_cast<unsigned long>(heap.minFreeBytes),
             static_cast<unsigned long>(heap.maxAllocBytes));
}
