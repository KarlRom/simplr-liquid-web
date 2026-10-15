# SIMPL_R Liquid v0.23.5 — Teensy 4.1 Preview

This adds a Mac-hosted **Teensy 4.1 preview interface**, NOT Teensy firmware. Use the **TEENSY 4.1 MODE →** button in Full Editor. The five knob arcs use the supplied reference design: BLOOM, TEXTURE, MOTION, COLOR, SPACE. Drag vertically, scroll the knob, or focus and use arrow keys (Shift for larger steps). All knobs write to the existing DSP parameter model. Full Editor displays the changed settings. Entering the preview temporarily sets Global density and 64 max grains; leaving restores the previous settings.

Macro controls represent multiple DSP parameters and cannot perfectly reverse-map arbitrary full editor settings. On mode entry the arc value is inferred from the macro's primary parameter, but no audio parameters are changed by that inferred position until the knob is adjusted. Existing sample, sequence, audio playback and recording remain in the same running engine; no audio is reset.

Build on Mac:

```bash
cd ~/Downloads/simplr_liquid_v23_5
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/liquid_mac web
```

Open http://127.0.0.1:8765. This is a browser UI preview of a possible future embedded interface; no Teensy 4.1 hardware or firmware is required.
