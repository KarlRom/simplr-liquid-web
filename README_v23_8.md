# SIMPL_R Liquid v0.23.8 — Teensy 8-macro interface

Teensy Mode uses two rows of four liquid-physics arc controls, with SPACE at the start of the second row.

Row 1: BLOOM, TEXTURE, MOTION, COLOR
Row 2: SPACE, LIQUID, FX SYNC, FX TYPE

- BLOOM includes Note Release mapped exponentially from 50 to 3000 ms.
- SPACE Shimmer Amount maximum is 70%, while Shimmer Tone stays warm and Feedback constrained.
- LIQUID affects Liquid Scale, Droplet Detail, Liquid Cohesion, Viscosity and Surface Tension.
- FX SYNC selects existing tempo-synced divisions (1/4, 1/8, 1/16, dotted 1/8, triplet 1/8).
- FX TYPE selects the existing effect choices (Off, Hall, Plate, Room, Delay, Pingpong, Shimmer).
- Macro tooltips list only affected parameters. Waveform and grains stay on the left and the 4x4 performance pads on the right.

This is still a Mac-hosted Teensy UI simulation, not native Teensy firmware. FX SYNC only affects tempo-synced delay behavior where supported.

## Build on Mac

```bash
cd ~/Downloads/simplr_liquid_v23_8
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/liquid_mac web
```

Visit http://127.0.0.1:8765.

Linux C++ build: 14/14 tests passed. Browser JavaScript syntax checked with Node. Mac build/playback and final UI layout require confirmation on actual Mac.
