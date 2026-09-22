# Founding flow — playtest script

**2026-09-22.** The descent is in the game (plan §8). This is what to
play, where, and what to report. The figures in `figures/b1_*.jpg` show
each rung as the game renders it; `tools/viewtest/viewtest.sh` regenerates
them, flight frames included, in half a minute.

## Where to play

| Build | How | Notes |
|-------|-----|-------|
| Desktop game | `cmake -B build && cmake --build build --target colony_game && ./build/src/colony_game` | the shipping flow |
| Desktop, annotated | `cmake --build build --target colony_viewtest && ./build/src/colony_viewtest` | the same descent through the same `SurveyFlow`, plus the known-issue overlay (`I`) and rung jumps (`1` `3` `4`) |
| Phone / tablet | `https://navidamin.github.io/raylib-colony/` (game), `/viewtest/` (annotated) | only once this branch's Pages deploy passes: the `github-pages` environment must list the branch under *Deployment branches* (Settings → Environments), then re-run the failed deploy. Bust the cache with `?v=N`. |

## Controls

| Where | Input | Does |
|-------|-------|------|
| Menu | `ENTER` | to the globe |
| Globe | move | the region under the cursor lights up and names itself; the card previews its economy |
| Globe | left-drag, wheel | turn, zoom |
| Globe | click | claim the region: the flight down to its 200 km district |
| Globe | `ENTER` | claim what is under the screen centre |
| Globe | click a colony marker | open that colony |
| Globe | `Esc` | menu |
| District | move | the 25 km cursor snaps; the level card reads LOLA elevation and slope under it |
| District | click | descend into the cursor (the dive) |
| Site | move | the 1.5 km cursor is the base's own footprint, judged live, coloured by the verdict |
| Site | click on green | found the colony here: the picture stays, the first sect stands at the click |
| Any rung | `Esc`, right-click, BACK | up one rung, all the way to the globe |
| Colony | `Ctrl`+hover | resource preview of the ground under the cursor |
| Colony | `Ctrl`+click | found a sect there (refused inside another colony's territory, closer than 5 km to a sect, or with its footprint outside the 25 km window) |
| Colony | double-click a sect, `S` | Sect view |
| Colony | `Esc`, `P` | up: the district if you came down the ladder, else the globe turned to face the colony |
| Sect | double-click a unit, `U` | Unit view; `C` or `Esc` back to the colony |

Touch: tap to aim, tap again to claim, descend or found; the strip's BACK
goes up a rung.

## The script

1. **Cold start.** `ENTER` at the menu. The globe comes up on the real
   mosaic; hover names regions; the card lists composition and archetype;
   drag turns, wheel zooms; the strip says what a click does.
2. **Claim Mare Imbrium** (upper left of the near side). The flight turns
   the globe to the pick and zooms in; it lands on a 200 km district of
   the synthesizer's ground. The 25 km cursor snaps; the level card's
   numbers follow it and are real: a crater rim reads steep.
3. **Descend.** The dive. The 25 km site window is the centre of the
   district you were over. The 1.5 km cursor follows the pointer, red on
   crater walls, green on the flat; the card explains the verdict.
4. **Found on green.** The strip says COLONY FOUNDED; the Colony view
   opens on the same ground without a cut; the first sect stands where
   you clicked; the colony's HUD is up.
5. **Walk back up** with `Esc`: district, then globe. The globe faces the
   colony and marks it. Clicking the marker opens the colony directly, no
   second founding. `Esc` from the opened colony returns to the globe,
   turned to face it.
6. **A second colony on the far side.** Turn the globe to Tsiolkovskiy
   (21 S, 129 E, the dark-floored far-side crater) and found there. Two
   markers on the globe; each opens its own colony; the ground differs.
7. **Sects.** In a colony: `Ctrl`+hover previews the ground; `Ctrl`+click
   adds a sect. Try one 3 km from the first (refused), one at the window's
   edge (refused), one 6 km away (built). Double-click into it: the Sect
   view's ground is the same place, worked by the site disturbance.
8. **Polar cap.** Turn to the south pole and hover Shackleton. The strip
   says the cap cannot be claimed and a click does nothing (plan D7).
9. **Refused ground.** At the site rung, park on a crater wall and click:
   nothing happens and the strip says why.

## What to report

- **Phone first:** does the page load at all? The preload now carries the
  32 MB DEM, so device heap is the open question (plan B3). Give the shell
  badge's line (`SHELL v5 cnv=... dpr=N`).
- **Tap feel:** the aim-then-claim two-tap; claims fired by a drag that
  ended on the moon.
- **The flights:** too fast, too slow, any frame where the ground jumps.
- **Registration:** any rung where the ground under the cursor is not the
  next rung's picture.
- **Verdict against the eye:** green on ground that looks steep, red on
  ground that looks flat. Give the lat/lon; `colony_inspect LAT LON`
  dumps the numbers the verdict used.
- **Cards:** readability at phone width (they still use the default
  font; the restyle is an open item).

## Known, not fixed

- Claims inside the polar cap (past 80 deg) are refused until the
  tangent-plane frame exists (plan D7, measured in `figures/b1pole_*.jpg`).
- The cards use the default raylib font; no labels toggle; no ghosted
  card while the pointer is off the moon.
- Device heap with the DEM in the preload is unmeasured.
