#!/usr/bin/env python3
"""Erzeugt HomeFlightTracker/src/ui/AircraftIcons.h: Symbole (Verkehrsflugzeug, Kleinflugzeug, Hubschrauber)
in 24 Richtungen (15°-Schritte), 2 Bit Deckung pro Pixel (0 = leer … 3 = voll) für Kantenglättung auf dem Display.

Aufruf: python3 tools/gen_icons.py HomeFlightTracker/src/ui/AircraftIcons.h [vorschau.png]
Benötigt Pillow.
"""
import math
import sys

from PIL import Image, ImageDraw

SIZE = 17       # Symbolgröße in Pixeln
SUPERSAMPLING = 16
STEPS = 24      # Richtungen


def mirrored(right):
    """Symmetrisches Polygon aus der rechten Hälfte (erster und letzter Punkt auf der Mittelachse)."""
    return right + [(-x, y) for x, y in reversed(right[1:-1])]


def ellipse(cx, cy, rx, ry, points=32):
    return [(cx + rx * math.sin(2 * math.pi * i / points), cy - ry * math.cos(2 * math.pi * i / points))
            for i in range(points)]


def bar(x1, y1, x2, y2, width):
    """Rechteck der Breite width entlang der Strecke (x1, y1) – (x2, y2)."""
    length = math.hypot(x2 - x1, y2 - y1)
    nx, ny = -(y2 - y1) / length * width / 2, (x2 - x1) / length * width / 2
    return [(x1 + nx, y1 + ny), (x2 + nx, y2 + ny), (x2 - nx, y2 - ny), (x1 - nx, y1 - ny)]


# Silhouetten mit Nase nach oben, Koordinaten -50 … 50; jede Form ist eine Liste von Polygonen,
# ein Eintrag ("aus", Polygon) stanzt eine Fläche wieder aus.
SHAPES = [
    ("Verkehrsflugzeug", [mirrored([(0, -48), (5, -40), (6, -14), (46, 10), (46, 18), (6, 8), (5, 30), (20, 40),
                                    (20, 46), (2, 42), (0, 47)])]),
    ("Kleinflugzeug", [mirrored([(0, -44), (6, -38), (6, -20), (48, -20), (48, -6), (6, -6), (4, 30), (18, 32),
                                 (18, 42), (0, 42)]),
                       bar(-14, -45, 14, -45, 5)]),  # Propeller
    ("Hubschrauber", [ellipse(0, -10, 34, 34), ("aus", ellipse(0, -10, 27, 27)),  # Rotorkreis
                      ellipse(0, -10, 12, 18), bar(0, 4, 0, 46, 7), bar(-12, 43, 12, 43, 6)]),  # Rumpf, Heck
]


def coverage(polygons, degrees):
    a = math.radians(degrees)
    c, s = math.cos(a), math.sin(a)
    big = Image.new("L", (SIZE * SUPERSAMPLING, SIZE * SUPERSAMPLING), 0)
    scale = (SIZE - 1) / 100.0 * SUPERSAMPLING
    centre = SIZE * SUPERSAMPLING / 2
    draw = ImageDraw.Draw(big)
    for polygon in polygons:
        fill = 255
        if polygon[0] == "aus":
            fill, polygon = 0, polygon[1]
        draw.polygon([((x * c - y * s) * scale + centre, (x * s + y * c) * scale + centre) for x, y in polygon],
                     fill=fill)
    small = big.resize((SIZE, SIZE), Image.BOX)
    return [[round(small.getpixel((x, y)) / 255.0 * 3) for x in range(SIZE)] for y in range(SIZE)]


def main():
    bytes_per_row = (SIZE + 3) // 4
    lines = [
        "// Erzeugt mit tools/gen_icons.py – nicht von Hand ändern.",
        "// Symbole %d x %d px in %d Richtungen (je %d°, Nase bei 0° nach Norden)," % (SIZE, SIZE, STEPS, 360 // STEPS),
        "// 2 Bit Deckung pro Pixel (0 = leer, 3 = voll), 4 Pixel je Byte, linkes Pixel in den oberen Bits.",
        "#pragma once", "", "#include <stdint.h>", "", "namespace icons {", "",
        "constexpr int16_t kAircraftSize = %d;" % SIZE,
        "constexpr uint8_t kAircraftSteps = %d;" % STEPS,
        "constexpr uint8_t kAircraftBytesPerRow = %d;" % bytes_per_row,
        "constexpr uint8_t kAircraftShapes = %d;  // %s" % (len(SHAPES), ", ".join(n for n, _ in SHAPES)), "",
        "static const uint8_t kAircraft[kAircraftShapes][kAircraftSteps][%d] = {" % (SIZE * bytes_per_row),
    ]
    scale = 4
    preview = Image.new("RGB", (STEPS * (SIZE + 3) * scale, len(SHAPES) * (SIZE + 2) * scale), (5, 11, 16))
    for shape, (name, polygons) in enumerate(SHAPES):
        lines.append("  {  // %s" % name)
        add_shape(lines, preview, shape, polygons, bytes_per_row, scale)
        lines.append("  },")
    lines += ["};", "", "}  // namespace icons", ""]
    with open(sys.argv[1], "w") as f:
        f.write("\n".join(lines))
    if len(sys.argv) > 2:
        preview.save(sys.argv[2])


def add_shape(lines, preview, shape, polygons, bytes_per_row, scale):
    top = shape * (SIZE + 2) * scale
    for step in range(STEPS):
        grid = coverage(polygons, step * 360 / STEPS)
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
                            preview.putpixel((step * (SIZE + 3) * scale + x * scale + dx,
                                              top + scale + y * scale + dy), colour)
        lines.append("    {" + ", ".join("0x%02X" % v for v in data) + "},  // %d°" % (step * 360 // STEPS))


if __name__ == "__main__":
    main()
