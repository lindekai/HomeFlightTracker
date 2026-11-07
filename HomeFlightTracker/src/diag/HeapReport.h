// Heap-Messung für Ressourcen-Nachweise (Basiswerte, Langzeitstabilität).
#pragma once

#include <Print.h>
#include <stdint.h>

struct HeapSnapshot {
  uint32_t freeBytes;      // aktuell freier Heap
  uint32_t minFreeBytes;   // niedrigster freier Heap seit Start
  uint32_t maxAllocBytes;  // größter zusammenhängender allozierbarer Block
};

HeapSnapshot takeHeapSnapshot();
void printHeapSnapshot(Print& out, const char* label);
