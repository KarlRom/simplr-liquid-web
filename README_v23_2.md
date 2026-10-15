# SIMPL_R Liquid v0.23.2 — single-view parameters and profiling

- Restores **every Granulator control on one page**; no sidebar pagination or internal sidebar scrolling. Three columns on wider screens, two columns when the browser is narrower. Liquid and Post-FX remain tabbed.
- Reduces grain telemetry polling from approximately 15.4 to 8 times per second, and limits the costly SVG fluid simulation redraw to approximately 30 FPS. Waveform/audio DSP settings and parameter tooltips remain available.
- Adds `liquid_density_diagnostic`, an FX-OFF, four-key synthetic workload that reports the output peak, sample-to-sample jumps, pre-limiter overloads, stolen grains, and dropped grains in global/per-note mode at densities 8/32/64/128.

## Important audio caveat
This revision **does not claim to fix the audible pops/clicks**. Synthetic measurements of four sustained notes with a 2-second generated tone showed no pre-limiter overs and no allocation losses at high densities, but that does not reproduce the user's actual junior_mesa sample, sequencer retriggers or CoreAudio device path. Existing overlap gain compensation and soft clipping remain unchanged. Captured output WAV comparison with real playback is the next diagnostic step.

## Mac instructions

```sh
cd ~/Downloads/simplr_liquid_v23_2
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/liquid_density_diagnostic
./build/liquid_mac web
```

Then open http://127.0.0.1:8765 . Stop older versions first.

For full DSP workload benchmarks:

```sh
./build/liquid_benchmark --seconds 60 --buffer 128 --limit 128 --density 128 --fx off
```

Tests validated on Linux in this environment, not macOS. UI checked with Node JavaScript syntax validation, not manually in a browser.
