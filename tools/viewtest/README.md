# View-ladder playtest (`colony_viewtest`)

Walks the game's geographic views — **Orbital → Planet → Colony → Sect** —
using the real `RenderManager`, and overlays the known issues for whichever
view is on screen. The annotations are playtest-only commentary; they exist
in this target alone and never ship in the game.

This is the instrument for judging the *whole descent*, where
`tools/preview` renders a single view in isolation.

## Build and run

```bash
cmake --build build --target colony_viewtest

# interactive
./build/src/colony_viewtest

# headless screenshots -> build/viewtest/vt_{orbital,planet,colony,sect}.png
tools/viewtest/viewtest.sh
```

Headless rendering needs the software-GL wrapper the script already applies:
`LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe xvfb-run -a ...`.

## Controls

| Input | Action |
|-------|--------|
| click / tap, ↓ | descend one view |
| Esc, right-click, ↑ | ascend one view |
| `1` `2` `3` `4` | jump to Orbital / Planet / Colony / Sect |
| `I` | toggle the issue overlay |
| `R` | hop the sect to another grid cell (new terrain) |

On the **orbital** view a click is a *region pick*: it inverts the disc
projection to real lat/lon, re-anchors the playfield there, and descends.
The gold marker shows the 100 km playfield you are about to enter.

## Flags

| Flag | Effect |
|------|--------|
| `--shots PREFIX` | render all four views to `PREFIX_*.png` and exit |
| `--pick LAT,LON` | land the ladder anywhere without clicking |
| `--nodisturb` | generate the ground with the site left untouched |

`--pick` plus `--shots` is how the pipeline gets checked against arbitrary
locations; see the random-site sweeps in `prototypes/planet_visuals/`.

## Web

The deploy workflow publishes this target to `/viewtest/` alongside the game,
so the ladder can be walked on a phone or tablet. Deploys run from `main`;
a feature branch also needs adding to the `github-pages` environment's
allowed branches before its deploy job will run.
