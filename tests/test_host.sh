#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD="$ROOT/tests/build-host"
rm -rf "$BUILD"
mkdir -p "$BUILD"
clang++ -std=c++17 -Wall -Wextra -Werror -Wpedantic -Wconversion -Wshadow -fPIC -I"$ROOT/include" -c "$ROOT/src/A64SlotInstrument.cpp" -o "$BUILD/A64SlotInstrument.o"
clang++ -std=c++17 -Wall -Wextra -Werror -Wpedantic -Wconversion -Wshadow -I"$ROOT/include" "$ROOT/tests/test_host.cpp" "$BUILD/A64SlotInstrument.o" -pthread -o "$BUILD/test_host"
"$BUILD/test_host"
