# Game walk (`colony_viewtest`)

Walks the game: the level ladder as the game runs it — **Globe → District
→ Site** — through the game's own `SiteSelectionController` (`SurveyFlow`),
founds a colony, then walks that colony's **Colony → Sect** views,
using the real `RenderManager`, and overlays the known issues for whichever
view is on screen. The annotations are playtest-only commentary; they exist
in this target alone and never ship in the game.

**Three levels, then two views.** Globe → District (200 km) → Site
(25 km) is the level ladder, the only one there is
(`docs/design/site-selection/README.md`); `lunar_map` runs the same
controller as an instrument. Colony and Sect are where a founded colony
is managed — views, not levels.

This is the instrument for judging the game's views end to end, where
`tools/preview` renders a single view in isolation.

## Build and run

```bash
cmake --build build --target colony_viewtest

# interactive
./build/src/colony_viewtest

# headless screenshots -> build/viewtest/vt_{orbital,district,site,colony,sect,orbital_two}.png
tools/viewtest/viewtest.sh
```

Headless rendering needs the software-GL wrapper the script already applies:
`LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe xvfb-run -a ...`.

## Controls

| Input | Action |
|-------|--------|
| hover / click | the descent's own: the region under the pointer names itself; click claims, descends, founds |
| click / tap, ↓ | colony → sect |
| Esc, right-click, ↑, BACK | up one rung, all the way to the globe |
| `1` `3` `4` | jump to the globe / the colony / the sect |
| `I` | toggle the issue overlay |
| `R` | turn the globe to the next real place |

The descent is the game's: `SurveyFlow` drives the same
`SiteSelectionController` the Engine does, so what the harness walks is
the shipping state machine. Headless, `--shots` scripts the whole ladder
at `--pick` (claim, descend with the cursor aimed `--aim DX,DY` km from
the pick, found), then founds a second colony on the far side and ends on
the globe with both marked.

## Flags

| Flag | Effect |
|------|--------|
| `--shots PREFIX` | script the whole descent and render every rung to `PREFIX_*.png`, then exit |
| `--pick LAT,LON` | the region the scripted descent claims (default Mare Imbrium) |
| `--aim DX,DY` | km east/north of the pick the cursor is aimed at below the globe (default 30,-20) |
| `--nodisturb` | generate the ground with the site left untouched |

`--pick` plus `--shots` is how the pipeline gets checked against arbitrary
locations; see the random-site sweeps in `prototypes/planet_visuals/`.

## Web

The deploy workflow publishes this target to `/viewtest/` alongside the game,
so the ladder can be walked on a phone or tablet. Deploys run from `main`;
a feature branch also needs adding to the `github-pages` environment's
allowed branches before its deploy job will run.
