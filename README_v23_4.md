# SIMPL_R Liquid v0.23.4 — adaptive short-slice wrap fix

Audio engine: corrected the slice-wrap blend to approach **exactly the next sample position**. Previously it blended to an offset inside the new slice, then jumped back to the actual slice start. The new fade adjusts its source-frame width for slice duration and grain playback step, with a separate reverse-direction path. The rest of the Granulator UI and features are preserved.

New automated regression test covers 5%, 25% and 100% slice sizes, densities 8/32/64/128, and Global/Per-Note modes. Benchmark diagnostics are reproducible using `./build/liquid_density_diagnostic`. These tests are synthetic; the precise sound on Mac and with `junior_mesa.mp3` still requires listening validation.

Build: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j && ctest --test-dir build --output-on-failure`

## Future hardware macro mapping (not yet implemented)

Keep DSP knobs separate internally. Offer five performance macro pages: BLOOM (density, bloom time, organic variation), TEXTURE (grain duration, slice size, grain shape), MOTION (travel, speed, spread), COLOR (pitch, envelopes), SPACE (reverb/delay). Provide a modifier/advanced layer for exact settings; avoid removing capabilities or making hidden parameter coupling permanent.
