## SIMPL_R Liquid Web Edition v0.23.13

This distribution adds a browser-based WebAssembly version. See [README_WEB_EDITION.md](README_WEB_EDITION.md) for GitHub Pages deployment, local listening, and test instructions. Native build instructions remain below.

---

# SIMPL_R Liquid v0.21.1 — click smoothing and movable cache

## What changed
- Keyboard and sequencer notes now ramp in over 5 ms instead of jumping directly to full volume. Release follows the existing adjustable NOTE RELEASE control (default 500 ms), decaying smoothly.
- A 2 ms minimum grain-edge fade protects even short grains or manually zeroed attack/release settings.
- Quiet-tail voice stealing is now more conservative (near-silent grains only). **True crossfaded voice stealing is not implemented**; audible clicks are still possible under unusual overloads.
- Imported samples are resampled to 48 kHz stereo PCM16 and stored in a temporary disk-backed file; the granular engine holds **at most 20 seconds** of stereo float32 (~7.68 MB).
- New SAMPLE CACHE WINDOW control and LOAD WINDOW button allow seeking to different parts of longer samples, including junior_mesa.mp3. Seek temporarily replaces the DSP sample window and resets currently playing grains. The trim UI applies within the currently loaded window.
- This is a **movable cache, not gapless async streaming**. Seek requests read from disk outside of render() and may briefly mute or interrupt playback. MP3 import still decodes the full file temporarily; peak *import* memory is not bounded. Source and preview overview depict the current 20-second window, not the complete waveform.
- Current live-capture behavior remains independent of the imported-sample cache.

## Build / play on macOS
```sh
cd ~/Downloads/simplr_liquid_v21
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/liquid_mac web
```
Open http://127.0.0.1:8765

## Run performance tests
```sh
./build/liquid_benchmark --seconds 60 --buffer 128 --limit 64 --density 64 --fx shimmer
```

## Remaining for Teensy
Implement codec-specific streaming/random-access file decoding, asynchronous SD prefetch into fixed-size buffers, and real crossfaded voice stealing. macOS CoreAudio playback not verified in this build environment.

---

# SIMPL_R Liquid v0.19 — Grain budget and stress testing

This update preserves v0.18.1 shimmer and recording while adding a **MAX ACTIVE GRAINS** granulator control (16–128 in steps of 16). It limits *new allocations* to the selected number of grain slots. If the cap is lowered while more grains are already playing, existing grains finish naturally; the steady-state count then obeys the lower cap. Requests above the cap count as dropped grains. This is a fixed preallocated voice pool, **not dynamic voice stealing**.

## Mac build and run

```bash
cd ~/Downloads/simplr_liquid_v19
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/liquid_mac web
```

Open http://127.0.0.1:8765. Change **Granulator → Max Active Grains** to 32/64/128 and compare the sound and counter values. Stop an older instance before starting this one.

## Reproducible offline benchmark

```bash
./build/liquid_benchmark --seconds 60 --buffer 128 --limit 64 --density 64 --fx shimmer
./build/liquid_benchmark --seconds 60 --buffer 128 --limit 64 --density 64 --fx pingpong
./build/liquid_benchmark --seconds 60 --buffer 64 --limit 32 --density 32 --fx shimmer
./build/liquid_benchmark --seconds 30 --all --limit 64 --density 64
./build/liquid_benchmark --help
```

FX choices: off, hall, pingpong, shimmer. `--all` runs off/pingpong/shimmer at buffer sizes 64/128/256/512. Benchmark measures **elapsed process time** around the DSP render call, not the sound-card driver or hardware I/O. `misses` means render elapsed time exceeded the selected buffer's sample duration; it is **not** an observed physical audio underrun. The test preloads an 8-second mono-equivalent stereo synthetic tone, runs sequencer and effects, and omits MP3 decoding, live I/O, UI rendering, thread-scheduling jitter under normal device load, and sample paging.

Do not compare benchmark CPU percentages across Mac, Teensy and Orange Pi as though the processors were identical. Compile and run the same DSP on the target processor. In particular, the 8s float32 stereo test sample occupies 3,072,000 bytes, whereas `junior_mesa.mp3` expands to approximately 249 MB float32 stereo at 48kHz. For Teensy, plan a bounded PSRAM cache and microSD streaming before porting the desktop app.

## Development checks

`ctest` covers the existing eight tests plus `configurable_grain_cap`. Native macOS CoreAudio compilation and live audio have not been verified here. Linux x86-64 offline timings are not Teensy predictions.

---

# SIMPL_R Liquid v0.17 — Experimental performance build

This C++17 Mac prototype includes a 16-step / 13-note wet-grain sequencer, sample-clock BPM scheduling, quarter-beat metronome, a ping-pong delay with beat subdivisions, smoother exponential keyboard releases, 30-second processed-output recording, and a DSP benchmark executable. A copy of `junior_mesa.mp3` is bundled and attempts to load at startup on macOS; Linux preview uses the synthetic fallback because AudioToolbox decoding is macOS-only.

## macOS build and run

```bash
cd ~/Downloads/simplr_liquid_v17
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/liquid_benchmark
./build/liquid_mac web
```

Open http://127.0.0.1:8765. Stop the previous server with Ctrl+C before starting this one. The web app and audio engine share a single local server.

## Controls

The 16-step grid has one row for each of the 13 A W S E D F T G Y H U J K piano keys. Click steps to toggle; press START SEQUENCE. The grid uses one sixteenth-note per step at the requested BPM. Toggle METRONOME and click level. POST-FX > PINGPONG DELAY accepts synced 1/4, 1/8, 1/16, dotted eighth and eighth triplet timing. Press RECORD OUTPUT, play the sequencer or keys, STOP OUTPUT, then EXPORT PROCESSED WAV. Recording includes FX and is limited to 30 seconds; it is a real-time capture, not yet an offline pattern-bounce system.

## Benchmark and hardware scope

Run `./build/liquid_benchmark` for 32, 64, 128 ambient grains/s plus a small sequencer pattern and ping-pong delay, rendering ~10s of 48 kHz stereo blocks of 512 frames. Reports average, p95, p99, max callback compute time, processing deadline misses, peak active grains, dropped grain allocations. A 512-frame / 48 kHz processing deadline is 10,667 µs. These measurements are from YOUR compilation target and only represent offline engine execution; the tests cannot certify actual Teensy/Orange Pi hardware performance or end-to-end I/O latency. The Mac live callback also logs mean/max/misses in /status after macOS playback starts. Its instrumentation includes extra application operations and lock time.

**Important memory observation:** `junior_mesa.mp3` (10m49s) takes ~249 MB as 48 kHz stereo float audio, and decoding can temporarily use considerably more. A Teensy 4.1 with 16/32 MB PSRAM cannot hold it all: use short clips, mono 16-bit samples or streaming/caching. Keeping the entire MP3 in the ZIP makes the ZIP ~8.5 MB larger. Orange Pi Zero 2W should have sufficient RAM in 1GB+ configurations, but real-time sound I/O must still be integrated and measured.

## Known limitations

- Mac CoreAudio compilation, decoding of bundled MP3, and actual audio I/O are **not verified on macOS in this environment**.
- Sequencer is a single 16-step pattern with multiple simultaneously selectable notes per step, not full multi-pattern arrangement/persistence. Transport stop may let already-started grains and delay trails finish.
- Metronome is embedded in the output audio. The click implementation is a simple sine burst.
- Hall/Plate/Room remain earlier approximations, not convolution reverbs. Ping-pong delay is lightweight feedback cross-channel delay with division sync.
- Processed output export is 16-bit PCM stereo 48 kHz, maximum 30s and requires the live Mac audio callback.
- The default sample has substantial RAM overhead on macOS; avoid running other memory-heavy apps while testing.

## v0.18: shimmer + output capture controls

- Post-FX > SHIMMER REVERB uses a windowed two-head time-domain pitch shift feeding the delay-network reflections. Choose +7, +12, +19 semitone interval, amount and tone. This is intentionally lightweight and not a studio-grade spectral shifter.
- Output has three distinct actions: RECORD OUTPUT, STOP OUTPUT, EXPORT PROCESSED WAV. Recording includes granular, sequencer, metronome, and post-FX audio. Capturing automatically stops after 30 seconds and keeps the completed WAV available until recording is restarted. For reverb tails, stop playing and let the effect decay **before** pressing STOP OUTPUT.
- The default `junior_mesa.mp3` remains in `assets/` and requires the native macOS decoder; Linux builds use fallback synthetic sample.
- `./build/liquid_benchmark` now benchmarks both ping-pong delay and shimmer at densities 32/64/128. Results obtained on x86-64 Linux are not transferable directly to Apple Silicon, Teensy 4.1, or Orange Pi Zero 2W.
- **Limitation:** macOS CoreAudio compilation and listening tests still need to be run on the user's Mac. Output recording currently stores up to 30 seconds in float memory before WAV export. No normalized or offline rendering mode is included.


## v0.18.1 shimmer repair
The pitch-shifted signal is now routed directly into the audible wet mix as well as feedback. `SHIMMER AMOUNT` changes the wet blend independently of feedback. For audition set `SHIMMER REVERB`, Mix=100%, Amount=100%, Tone=100%, and +12 interval, then compare Amount=0%. The shimmer test now measures energy around 440 Hz when processing a 220 Hz tone. Mac compilation requires verification locally.

## v0.20: density allocation and bounded working window

- **DENSITY MODE** in the Granulator panel: PER NOTE (legacy mode: density applies to each held note) or GLOBAL (a shared grain generation clock round-robins active notes). Ambient generation is controlled separately by Play and still adds grains; for a precise global total, pause ambient playback.
- **Quiet-tail voice stealing**: when the configured grain limit is full, the engine selects the quietest grain near its envelope tail and reuses that slot only if its weighted envelope is below 0.025. Otherwise it increments Dropped. Stolen and Dropped are separate counters. This is intentionally conservative and is not a guaranteed click-free crossfade.
- **Bounded source working window (prototype)**: imported files now retain only the first 20 seconds at 48 kHz, stereo float in the engine (7.68 MB decimal). Source duration is shown beside the cached duration. The original MP3 still gets fully decoded transiently for import, and only the first 20 seconds are playable. Full-file random access, movable windows, and asynchronous disk prefetch are **not implemented**. Avoid treating this as an SD-card streaming implementation for Teensy.
- Live capture remains a separate 30-second ring buffer, so its memory exceeds this import-window cap.
- Tests: `ctest --test-dir build --output-on-failure` checks 10 suites, including global vs per-note density. Benchmark: `./build/liquid_benchmark --seconds 60 --buffer 128 --limit 64 --density 64 --fx shimmer`.

Memory note: the importer currently allocates a full temporary decoded vector before constructing the bounded DSP window; this avoids long-term residence but **not peak import memory**. A real Teensy port requires streaming decoding to a finite cache on a non-audio thread, prefetch/underrun handling, and bounded peak allocations.

## v0.21.1 Load Window fix
- Slider selection is now preserved during periodic status refreshes until LOAD WINDOW completes.
- LOAD WINDOW shows progress and a success/error message, and the HTTP response includes the actual cache start and length.
- Verified the `/cache/seek` endpoint with a 31-second stereo WAV: requesting 10 seconds yielded a 20-second working window with `cacheStart=10`.
- Rebuild after extracting; do not reuse the v0.21 build directory.

## v0.22 audio-discontinuity patch
- Repeated sequencer steps actively retrigger their grains instead of relying on the prior step gate.
- Releasing a key no longer continues scheduling fresh grains from the fading note envelope; grains already launched complete their envelope.
- Near-silent voice stealing preserves the outgoing grain in one of 16 preallocated, 3 ms fading transition slots (zero heap allocation in the render loop).
- Slice grain sample reads crossfade over up to 4 ms at the wrap point. This protects against discontinuities when sample positions jump from the end of a slice to its beginning.
- Additional FX-off click regression with a harmonically rich test signal and repeated keyboard / sequencer triggers. It checks sample-to-sample jumps, but passing does not guarantee no audible click for every imported recording.
- The 20-second movable sample window behavior is unchanged; the app still does not do seamless continuous SD/MP3 streaming.

Native Mac CoreAudio listening tests are still required. Large spikes in an offline benchmark may also reflect OS scheduling rather than DSP load; run multiple repetitions and compare percentiles.

## v0.23 grain bloom, overload handling and parameter help
- `GRAIN BLOOM` (0–3000 ms) ramps grain scheduling rate after note onset; 0 ms immediately uses target density. This is separate from note-volume attack. Both Global and Per-Note modes use note ages.
- Granular mixing uses smoothed overlap-aware gain before the existing soft-saturation stage. The status line reports `OVER` (pre-limiter frames above full scale) and `PEAK` (maximum pre-limiter sample amplitude). These diagnostics help identify overload but are not proof of artifact-free sound.
- All known sliders and effect selectors in the right-hand sidebar have concise descriptions accessible by hovering the control, focusing it with the keyboard, or pressing the `?` help icon on touch devices. Escape closes the icon's help popup.
- This build passed the existing 12 C++ tests and browser JavaScript syntax checking on Linux. No new v0.23-specific automated regression suite has been added yet; test density ramp and clipping behavior on your Mac and compare listening results with v0.22.


## v0.23.1 responsive right panel
The right panel now paginates its Granulator, Liquid and Post-FX controls based on viewport height. Use Prev/Next to access all settings without scrolling the sidebar. Controls, values, and tooltips remain live across pages. On narrow screens the sidebar moves below the main stage.
