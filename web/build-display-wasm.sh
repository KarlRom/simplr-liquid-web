#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
CLANG="${CLANG:-clang++}"
"$CLANG" --target=wasm32 -std=c++17 -O2 -ffreestanding -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit \
 -nostdinc++ -nostdlib -Iweb/wasm-compat -Iembedded -Wl,--no-entry -Wl,--export-memory \
 -Wl,--initial-memory=8388608 -Wl,--max-memory=134217728 -Wl,--import-undefined \
 -Wl,-z,stack-size=1048576 -Wl,--strip-all \
 $(for n in display_init display_input_ptr display_output_ptr display_output_bytes display_render; do printf '%s ' "-Wl,--export=$n";done) \
 embedded/LiquidDisplay.cpp web/wasm-src/display-bridge.cpp web/wasm-src/display-format.cpp web/wasm-src/runtime.cpp -o web/liquid-display.wasm
ls -lh web/liquid-display.wasm
