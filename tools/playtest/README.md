# Prospecting Playtest

An interactive sandbox that boots straight into the extraction unit's
prospecting module — no menu, no colony placement, no navigation. Use it to
playtest the sweep → sample → lab loop and feel how survey progress and
extraction efficiency respond.

## Play on phone / tablet

The playtest builds for WebAssembly and deploys with the game to GitHub
Pages via `.github/workflows/deploy-web.yml`:

- Game: `https://<owner>.github.io/<repo>/`
- **Playtest: `https://<owner>.github.io/<repo>/playtest/`**

Taps map to clicks; the TIER UP / RESET buttons in the top bar replace the
`T` / `R` keys. The canvas scales to the device screen (landscape
recommended). Trigger the workflow manually (workflow_dispatch) or push to
main to redeploy.

## Run it (desktop)

```bash
cmake -B build
cmake --build build --target colony_playtest -j
./build/src/colony_playtest
```

Needs the same system packages as the game itself (X11 + OpenGL dev
libraries on Linux; see `tools/preview/README.md` for the apt list).

## Controls

| Input | Action |
|---|---|
| Mouse | everything in the panel: tabs, grid cells, frequency bands, depth layers, COLLECT, lab tools |
| `T` | upgrade prospecting tier (0 → 3; grid grows, bands/depths unlock) |
| `R` | reset the run — fresh grid, tier 0 |
| `ESC` | quit |

The left module list and control panel are fully live too — you can build
and activate the other modules, though only prospecting has real gameplay.

## What to playtest

1. **Tier 0 start**: 3x3 grid, one frequency band. Sweep, select cells,
   check the signal readout.
2. **Tier progression** (`T`): grid resolution grows, more bands, deeper
   drilling, more lab tools.
3. **The full loop**: sweep bands → pick high-signal cells → drill samples
   at depths → run lab presets/tools → watch the bottom status bar
   (SWEEP / SAMPLES / TESTING) and marked-site state.
4. **Calibration**: quality drifts with use; recalibrate from the sweep tab.

## Known gameplay gaps (expected, not bugs)

- **Energy costs are display-only** — sweeps/drills/lab runs show an `E`
  cost but nothing is deducted yet.
- **Sample/testing confidence may read 0%** in the bottom bar — the
  aggregation of sample + lab work into `surveyProgress` is a flagged open
  issue (see session notes); sweeping is what moves the number today.
- Frequency bands are one-shot per grid (that's by design); use `R` for a
  fresh grid.

## Headless smoke test

`colony_playtest --shot out.png` renders 40 live frames and exports a
screenshot instead of running interactively (used for CI/container checks):

```bash
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ./build/src/colony_playtest --shot boot.png
```
