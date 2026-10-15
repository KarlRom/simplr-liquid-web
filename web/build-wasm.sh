#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
# Standalone freestanding clang WebAssembly compilation: preserves original C++ engine;
# a tiny header compatibility layer replaces the unavailable WASI/Emscripten C++ runtime.
CLANG="${CLANG:-clang++}"
"$CLANG" --target=wasm32 -std=c++17 -O2 -ffreestanding -fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit \
  -nostdinc++ -nostdlib -Iweb/wasm-compat -Iinclude -Wl,--no-entry -Wl,--export-memory \
  -Wl,--initial-memory=16777216 -Wl,--max-memory=536870912 -Wl,--import-undefined \
  -Wl,-z,stack-size=524288 -Wl,--strip-all \
  $(for n in app_init app_prepare_sample app_commit_sample app_param app_note app_step app_audio_ptr app_render app_visual_ptr app_visual_count app_active app_dropped app_started app_stolen app_overs app_peak; do printf "%s " "-Wl,--export=$n"; done) \
  src/LiquidEngine.cpp web/wasm-src/bridge.cpp web/wasm-src/runtime.cpp -o web/liquid-dsp.wasm
ls -lh web/liquid-dsp.wasm
