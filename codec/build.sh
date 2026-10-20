#!/usr/bin/env bash
# Build the research codec (FLIF v0.4 + v12 experts + v17 gated context mixing + v18 polarity canonicalization).
# Requires: clang++ (or g++), make, zlib. Tested on macOS arm64 (Apple clang 21).
set -euo pipefail
cd "$(dirname "$0")/flif_pcc/src"
make -B -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)" flif \
  CXX="${CXX:-clang++}" CPPFLAGS=-DFLIF_USE_STB_IMAGE \
  OPTIMIZATIONS='-DNDEBUG -O2 -ffp-contract=off' LDFLAGS=-lz
echo "built: $(pwd)/flif"
