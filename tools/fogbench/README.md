# fogbench — what the fog costs

Times one Holo3D block render in three states: fully known with no fog, fog
over half the block, and fog over all of it. It is headless, with
software GL, 30 frames each after 3 warm-up frames. It forces the GPU to
finish before reading the clock.

```bash
gcc -O2 -std=gnu99 -o build/fogbench tools/fogbench/fogbench.c -Isrc/ui \
    -Ibuild/_deps/raylib-src/src build/src/libcolony_c2d.a \
    build/_deps/raylib-build/raylib/libraylib.a -lGL -lm -lpthread -ldl -lX11
LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe xvfb-run -a \
    -s "-screen 0 1920x1080x24" build/fogbench 1      # supersample 1, the web's
```

llvmpipe milliseconds are not a GPU's, so read the ratios, not the numbers.
First run (console fix plan, step 9):

| | ss 2 | ss 1 |
|---|---|---|
| no fog, all known | 47.4 ms | 28.1 ms |
| fog, half known | 37.0 ms | 15.4 ms |
| fog, nothing known | 31.0 ms | 12.2 ms |

**The fog makes the block cheaper, not dearer.** Fogged rock draws no fill,
no mesh and no bed boundary, only the instrument's wire. The expensive
block is the fully known one: lit gradients, scatter and glow over every
face. That is the reference's renderer, and the port's visual diff holds it.
