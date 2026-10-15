# SIMPL_R Liquid v0.23.6 — liquid Teensy instrument view

Mac-hosted Teensy UI preview (NOT Teensy firmware). Reuses the live waveform/grain canvas and the existing 13-key piano, moving the same DOM elements between Full Editor and Teensy Mode so note event handlers and DSP telemetry are shared.

Five macro knobs now animate with damped spring-follow arcs, deforming endpoints and a small trailing liquid bead. Drag, mouse wheel, keyboard arrows, Home/End remain supported. Hover, keyboard focus, or tap opens tooltips listing mapped parameters only.

Macro mappings:
- BLOOM: Density, Grain Bloom, Grain Timing, Bloom Variation.
- TEXTURE: Grain Length, Slice Size, Grain Attack, Grain Decay. The existing engine has no separately labeled 'Grain Shape' slider, so this tooltip lists the four actually changed parameters.
- MOTION: Playback Speed, Grain Spread. Does not alter Travel.
- COLOR: Grain Attack, Grain Decay, Density Jitter. Does not alter Pitch Randomization.
- SPACE: Reverb Mix, Reverb Size, Feedback, Shimmer Amount.

Mac/Linux build:
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/liquid_mac web
```
Browse to http://127.0.0.1:8765 and click TEENSY 4.1 MODE. Browser JS syntax checked with Node 22. CMake build and 14 C++ tests passed in the Linux build environment. Native macOS UI/audio still needs user verification.
