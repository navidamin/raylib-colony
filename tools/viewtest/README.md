# View-ladder playtest (`colony_viewtest`)

Walks the game's geographic views — **Orbital → District → Colony (or the
site rung) → Sect** — through the game's own survey descent (`SurveyFlow`),
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
| `--sect` | interactive: found at `--pick` first, then start in the Sect view (for hover and dome testing) |

On the web, `?sect` does the same as `--sect`. The DomeForge sect-view branch
deploys this target to `/sectview/`, so `/sectview/?sect` opens straight in it.

`--pick` plus `--shots` is how the pipeline gets checked against arbitrary
locations; see the random-site sweeps in `prototypes/planet_visuals/`.

## Web

The deploy workflow publishes this target to `/viewtest/` alongside the game,
so the ladder can be walked on a phone or tablet. Deploys run from `main`;
a feature branch also needs adding to the `github-pages` environment's
allowed branches before its deploy job will run.
