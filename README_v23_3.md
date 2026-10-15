# SIMPL_R Liquid v0.23.3

- Grain Bloom now supports REGULAR/ORGANIC arrival timing, selectable bloom curve, and bloom variation to avoid mechanically repeating grain patterns.
- Retains compact single-view Granulator sidebar, existing cache, effects and recording.
- Added `liquid_density_diagnostic` slice-size sweep at 5%, 25%, 100% for Global and Per-Note densities 8/32/64/128. `jumps_gt_0.15` is an *audio output discontinuity heuristic*, not a definitive click count.
- Important: the slice-size sweep found more large sample-to-sample jumps for very short slices even when `prelimit_overs`, `dropped` and `stolen` are all zero. The underlying click issue is NOT fixed in this version. The next audio correction should focus on short-loop continuity and short-slice grain read/wrap paths and listen to actual samples.

## Mac build
```
cd ~/Downloads/simplr_liquid_v23_3
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/liquid_mac web
```
Browser: http://127.0.0.1:8765

## Inspect small slice artifacts
```
./build/liquid_density_diagnostic
```

All 13 C++ tests pass on Linux. Mac playback and actual audibility are unverified.
