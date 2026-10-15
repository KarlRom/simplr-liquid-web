# v0.23.9 — Teensy performance note capture

Teensy mode uses 12 chromatic note pads A–J, and four action pads: K / REC (record/stop note events), PLAY (unchanged continuous granular playback), SEQ (start/stop looping recorded note sequence), HOLD (freeze). Note capture includes note-on/off times and overlaps. Full Editor's original 13-key piano and programmed 16-step sequencer are preserved.

Recorded performance data is browser-session-local and currently uses JavaScript timers for replay, rather than sample-clock-accurate DSP sequencing. Timing can drift with browser load, and recordings disappear when the page reloads. WAV processed-audio capture remains separate.
