#!/usr/bin/env python3
"""Erzeugt HomeFlightTracker/src/ui/AircraftIcons.h: Flugzeugsymbol in 24 Richtungen (15°-Schritte),
2 Bit Deckung pro Pixel (0 = leer … 3 = voll) für Kantenglättung auf dem Display.

Aufruf: python3 tools/gen_icons.py HomeFlightTracker/src/ui/AircraftIcons.h [vorschau.png]
Benötigt Pillow.
"""
import math
import sys

from PIL import Image, ImageDraw

SIZE = 17       # Symbolgröße in Pixeln
SUPERSAMPLING = 16
STEPS = 24      # Richtungen
# Silhouette mit Nase nach oben, Koordinaten -50 … 50 (rechte Hälfte, wird gespiegelt)
RIGHT = [(0, -48), (5, -40), (6, -14), (46, 10), (46, 18), (6, 8), (5, 30), (20, 40), (20, 46), (2, 42), (0, 47)]
POLYGON = RIGHT + [(-x, y) for x, y in reversed(RIGHT[1:-1])]


def coverage(degrees):
    a = math.radians(degrees)
    c, s = math.cos(a), math.sin(a)
    big = Image.new("L", (SIZE * SUPERSAMPLING, SIZE * SUPERSAMPLING), 0)
    scale = (SIZE - 1) / 100.0 * SUPERSAMPLING
    centre = SIZE * SUPERSAMPLING / 2
    ImageDraw.Draw(big).polygon(
        [((x * c - y * s) * scale + centre, (x * s + y * c) * scale + centre) for x, y in POLYGON], fill=255)
    small = big.resize((SIZE, SIZE), Image.BOX)
    return [[round(small.getpixel((x, y)) / 255.0 * 3) for x in range(SIZE)] for y in range(SIZE)]


def main():
    bytes_per_row = (SIZE + 3) // 4
    lines = [
        "// Erzeugt mit tools/gen_icons.py – nicht von Hand ändern.",
        "// Flugzeugsymbol %d x %d px in %d Richtungen (je %d°, Nase bei 0° nach Norden)," % (SIZE, SIZE, STEPS, 360 // STEPS),
        "// 2 Bit Deckung pro Pixel (0 = leer, 3 = voll), 4 Pixel je Byte, linkes Pixel in den oberen Bits.",
        "#pragma once", "", "#include <stdint.h>", "", "namespace icons {", "",
        "constexpr int16_t kAircraftSize = %d;" % SIZE,
        "constexpr uint8_t kAircraftSteps = %d;" % STEPS,
        "constexpr uint8_t kAircraftBytesPerRow = %d;" % bytes_per_row, "",
        "static const uint8_t kAircraft[kAircraftSteps][%d] = {" % (SIZE * bytes_per_row),
    ]
    scale = 4
    preview = Image.new("RGB", (STEPS * (SIZE + 3) * scale, (SIZE + 2) * scale), (5, 11, 16))
    for step in range(STEPS):
        grid = coverage(step * 360 / STEPS)
        data = []
        for y, row in enumerate(grid):
            for b in range(bytes_per_row):
                value = 0
                for k in range(4):
                    x = b * 4 + k
                    if x < SIZE:
                        value |= row[x] << (6 - 2 * k)
                data.append(value)
            for x, level in enumerate(row):
                if level:
                    colour = tuple(int(bg + (fg - bg) * level / 3) for fg, bg in zip((255, 211, 77), (5, 11, 16)))
                    for dy in range(scale):
                        for dx in range(scale):
                            preview.putpixel((step * (SIZE + 3) * scale + x * scale + dx, scale + y * scale + dy), colour)
        lines.append("    {" + ", ".join("0x%02X" % v for v in data) + "},  // %d°" % (step * 360 // STEPS))
    lines += ["};", "", "}  // namespace icons", ""]
    with open(sys.argv[1], "w") as f:
        f.write("\n".join(lines))
    if len(sys.argv) > 2:
        preview.save(sys.argv[2])


if __name__ == "__main__":
    main()
