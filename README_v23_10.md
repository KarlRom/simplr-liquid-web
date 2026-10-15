# SIMPL_R Liquid v0.23.10 — FX-aware SPACE macro

SPACE in Teensy Mode follows FX TYPE:
- HALL/PLATE/ROOM: wet MIX, SIZE/TIME, FEEDBACK.
- SHIMMER: wet MIX, SIZE/TIME, FEEDBACK, SHIMMER AMOUNT (up to 70%), with the warmer tone mapping.
- DELAY/PINGPONG DELAY: wet MIX and FEEDBACK only. FX SYNC continues to select timing subdivision.
- OFF: SPACE does not change effect parameters.
Hover/focus descriptions update when FX TYPE changes. Full Editor's shared FX controls remain available.
Existing K/REC sequence recording, PLAY, SEQ, HOLD and eight macro layout are unchanged.

On Mac: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j && ctest --test-dir build --output-on-failure && ./build/liquid_mac web`
