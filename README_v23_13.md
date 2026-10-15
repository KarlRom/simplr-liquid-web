# SIMPL_R Liquid v0.23.13

Built from v0.23.11 to restore the earlier MOTION control and grain movement behavior. The v0.23.12 independent slow-scanning mechanism and nonlinear 0.01x–2x MOTION mapping are **not included**.

- COLOR macro Grain Attack range now begins at 0% (upper mapping stays 65%). The DSP's short grain-edge safety fade remains.
- In Teensy preview, the former HOLD performance pad is now DIRECTION. Repeated presses cycle FORWARD, REVERSE, OUTWARD, INWARD, PINGPONG, RANDOM and change the same grain-direction setting as Full Editor.
- PLAY, SEQ, K/REC and the other 12 pad notes are unchanged. Freezing is still available in Full Editor.

## Build on macOS

```sh
cd ~/Downloads/simplr_liquid_v23_13
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/liquid_mac web
```

Open http://127.0.0.1:8765 and switch to Teensy Mode.

## Validation

Linux CMake build succeeded, all 14 CTest tests passed, and browser JavaScript passed `node --check`. Mac native playback and browser interactions should be verified on the user's machine.
