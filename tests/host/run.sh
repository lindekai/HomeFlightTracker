#!/usr/bin/env bash
# Baut und startet die Host-Unit-Tests (Arduino-unabhängige Module).
set -euo pipefail
cd "$(dirname "$0")"
SRC=../../HomeFlightTracker/src
OUT="${TMPDIR:-/tmp}/homeflighttracker-hosttests"
c++ -std=c++17 -Wall -Wextra -Werror -O1 -o "$OUT" \
  test_main.cpp "$SRC/geo/Geo.cpp" "$SRC/opensky/Quota.cpp"
"$OUT"
