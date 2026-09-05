# Prospecting Playtest

An interactive sandbox that boots straight into the extraction unit's
prospecting module — no menu, no colony placement, no navigation. Use it to
playtest the sweep → sample → lab loop and feel how survey progress and
extraction efficiency respond.

## Play on phone / tablet

The playtest builds for WebAssembly and deploys with the game to GitHub
Pages via `.github/workflows/deploy-web.yml`:

- Game: `https://navidamin.github.io/raylib-colony/`
- **Playtest: `https://navidamin.github.io/raylib-colony/playtest/`**

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
| `T` | upgrade prospecting tier (0 → 3; reach ring widens, bands/depths unlock) |
| `D` | dig the selected spot at the selected depth — a sandbox shortcut for what excavation will do properly |
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

## Energy

Every prospecting action costs energy from the unit's storage: sweeps
30-150 E by band, drills 8-75 E by tier/depth, lab tools 5-80 E, and
presets the sum of their pipeline. Buttons grey out when you cannot
afford them, and the ENERGY segment in the bottom bar turns gold below
300 E and red below 100 E.

In the real game a unit is fed by its sect/colony. The sandbox has
neither, so it trickles **30 E/second up to a 1500 E cap** — enough to
keep playing, not enough to ignore. Prioritising which samples deserve a
full lab workup is the intended tension.

## Known gameplay gaps (expected, not bugs)

- Frequency bands are one-shot per grid (that's by design); use `R` for a
  fresh grid.
- Objectives (Phase 8) and AI/default mode (Phase 7) are not implemented.

## Headless smoke test

`colony_playtest --shot out.png` renders 40 live frames and exports a
screenshot instead of running interactively (used for CI/container checks):

```bash
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ./build/src/colony_playtest --shot boot.png
```

## Troubleshooting

- **Deploy run failed with "socket hang up" / 503 in Setup Emscripten** —
  transient GitHub Actions infrastructure; re-run the workflow (or push any
  change under `src/` or `tools/playtest/`).
- **Phone still shows an old/cropped page** — GitHub Pages caches HTML for
  10 minutes and mobile browsers cache hard; append a query string
  (`/playtest/?v=3`) to force a fresh fetch.


## The resource statement

The panel at the bottom left is the point of the sandbox. For each element it
shows total tonnage as a bar, split by how well that tonnage is known:

| Colour | Class | What it means |
|--------|-------|---------------|
| green | Measured | commit to it |
| gold | Indicated | worth digging |
| violet | Inferred | a bet |
| grey | Unclassified | you know only the cell average |

The percentage on the right is Measured + Indicated — the share you could
actually commit to. That number is what surveying is *for*, and watching it
move is the fastest way to feel whether a sweep was worth its energy. Bars are
scaled by tonnage, so a small deposit cannot look like a big one just because
it happens to be well surveyed.

`DIG SPOT` (or `D`) digs the selected cell at the selected depth. Digging is
direct observation, so the spot flips straight to Measured — the quickest way
to see the difference between knowing and guessing without driving the whole
sweep → sample → lab chain. It is **per depth**: digging the surface says
nothing about what lies under it, which is what keeps the deep layers a bet
long after the surface is mapped.
