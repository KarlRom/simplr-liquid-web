# SIMPL_R Liquid v0.23.11 — PITCH macro and clearer grains

- Replaced the Teensy Mode `LIQUID` macro with `PITCH`, connected to the existing C++ DSP `pitch` parameter. The center position is 0 semitones, with a -12 to +12 semitone range and semitone-stepped encoder motion.
- Pitch tooltip reads `Pitch · Semitone Transposition`.
- The LIQUID parameter panel is still available in the Full Editor; no liquid visual parameters were removed.
- On the Teensy Preview only, real per-grain telemetry is rendered at a larger visible scale with more responsive position/size tracking and more visible short grain envelopes. No artificial grain events are introduced. Polling is 90 ms rather than 125 ms.
- Other seven macros, effects, 4x4 note/action pads, recorded-note sequencer and continuous playback retained.

## Mac build

```bash
cd ~/Downloads/simplr_liquid_v23_11
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/liquid_mac web
```

Open http://127.0.0.1:8765 and switch to Teensy 4.1 Mode.

## Validation

C++ project compiled on Linux and 14/14 CTest tests passed. `node --check` on embedded browser JavaScript passed. Mac playback and the visual adjustments still require user verification.
