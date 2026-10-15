# SIMPL_R Liquid — Web Edition v0.23.13-web.1

**A static, browser-playable port of SIMPL_R Liquid v0.23.13.** No server DSP,
Node runtime or account is needed for visitors. Each browser executes the
original C++ `LiquidEngine.cpp` compiled to WebAssembly inside an AudioWorklet.

## What's new compared with Web Edition v0.23.10

- Restored v0.23.13's original MOTION mapping (speed 0.25–0.95; spread 0–0.9).
- COLOR macro now maps grain attack from **0–65%** (plus grain release/jitter).
- DIRECTION action pad replaces HOLD and cycles **FORWARD → REVERSE → OUTWARD →
  INWARD → PINGPONG → RANDOM**. The full-editor selector stays in sync.
- PITCH replaces the LIQUID macro and adjusts **−12 to +12 semitones** in steps.
- v0.23.13's higher-visibility, more responsive instrument-view grain animation.
- Same v0.23.13 native C++ engine as supplied; identical to v0.23.10 at the
  binary source level. No DSP approximations were introduced.

## Play on a Mac

```bash
cd ~/Downloads/SIMPL_R_Liquid_Web_Edition_v0.23.13
python3 -m http.server 8000 --directory web
```

Open <http://localhost:8000> and click **ENABLE AUDIO**. Then try the included
browser-generated sample, or press **LOAD AUDIO** to import WAV/MP3. Switch to
**PERFORMANCE MODE** for the 8 macros and 16 pads. The full editor includes
waveform trim, 13-key slice/pitch keyboard, effects, and a 16-step sequencer.
Use **RECORD OUTPUT**, **STOP OUTPUT**, then **EXPORT PROCESSED WAV** to save
up to 30 seconds of stereo audio on your device.

**Do not double-click `index.html` to run from `file://`.** AudioWorklet needs
HTTPS or localhost, and loading sibling files requires an HTTP origin.

## Publish a NEW GitHub Pages site (shortest path)

1. On your Mac, install GitHub CLI if needed: `brew install gh`.
2. Authenticate once: `gh auth login` (choose GitHub.com, HTTPS, browser login).
3. From this extracted folder, run `./publish-github-pages.sh simplr-liquid-web`.
4. Check GitHub Actions / Pages deployment. The site's URL will appear there.

The script makes a **new PUBLIC repo** under your authenticated user and enables
Pages via the GitHub API. It **refuses to replace an existing repo**.
GitHub's API sometimes takes a moment to recognize a new Pages workflow; if the
workflow-dispatch command fails, open the repo's **Actions** tab and manually
run **Deploy SIMPL_R Liquid Web Edition**. To update an *existing* site, inspect
its repo first so other contents and custom domains are not overwritten.

Alternatively, push this project to a repository on `main` and select
**Settings → Pages → Build and deployment → Source: GitHub Actions**. The included
`.github/workflows/pages.yml` deploys the `web/` folder upon each push to main.
For Cloudflare Pages use the project root with build output directory `web` and
no build command, or upload the contents of the website-only ZIP directly.

## Architecture

```text
Browser DOM / waveform / liquid animations / 8 macros / pads
        |
        +--- web-adapter.js --- local fetch('/...') compatibility shim
        |        |- browser decodeAudioData() for local WAV/MP3
        |        |- 20-second PCM window to worklet
        |        '- 30-second processed WAV export in browser
        v
AudioWorkletNode <-- MessagePort --> audio-worklet.js
                                        |
                                        v
                                   liquid-dsp.wasm
                             LiquidEngine.cpp + bridge.cpp
```

`web-adapter.js` intercepts legacy HTTP endpoints such as `/params`, `/slice`,
`/status`, `/step`, `/grains`, `/upload`, `/cache/seek` and `/output/start`,
so the supplied v0.23.13 UI can run without its local native server.
The WASM C ABI uses small freestanding standard-library compatibility headers.
The runtime uses a 128-frame callback; sequencing and FX render inside the
original C++ engine. `web/build-wasm.sh` rebuilds WASM with LLVM Clang.

## Tests

```bash
./web/build-wasm.sh
node tests/test_web_wasm.mjs
node tests/test_web_worklet.mjs
python tests/test_web_ui.py
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Verified in build environment: 14/14 native tests passed, JS/WebAssembly and
simulated AudioWorklet rendering passed, browser UI (8 macros, 16 pads, COLOR,
PITCH, DIRECTION, MOTION) passed with no page errors. Actual audible desktop or
mobile AudioWorklet startup could **not** be tested here because the sandboxed
browser blocks local navigation. Test listening on a real device before
announcing a public launch.

## Limitations and privacy

- Browser must be enabled by a user click on each fresh page load.
- Desktop Chrome/Edge are the best first test targets; iOS Safari/mobile CPU
  headroom is not yet verified.
- The C++ sample window is up to 20 seconds. Seeking to another window replaces
  active sample contents; large-file decoding may consume a lot of memory.
- Native microphone/live-input capture is not in this Web Edition (related UI
  controls are hidden); **processed-output** recording/WAV download does work.
- Recorded pad-note loops are timed by browser JS, not sample-accurate. The
  16-step sequencer is executed inside WASM using the C++ engine clock.
- Samples, audio effects, output recordings, and WAV encoding remain entirely
  inside each visitor's browser. No external audio service or tracking scripts.
- The source project's bundled reference MP3 is excluded from the public
  repository to avoid publishing unlicensed sample content. Use local import.
