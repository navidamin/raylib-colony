# Sect Walkthrough

Boots straight into the **Sect view** so every unit and every module can be
inspected by hand. No planet view, no colony building, no menu.

```bash
cmake -B build && cmake --build build --target colony_sectwalk
./build/src/colony_sectwalk
```

Headless (renders 40 frames, writes a PNG, exits):

```bash
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a -s "-screen 0 1280x720x24" \
  ./build/src/colony_sectwalk --shot build/sectwalk/boot.png
```

## Why this exists

`tools/preview` renders **one** panel to a PNG — good for iterating on a single
layout, useless for judging the set. This harness is the opposite: it is the
only way to walk all **40 modules across 8 unit types** in sequence and see how
they read as a group.

Use it before merging UI work that touches more than one unit.

## Controls

| Input | Action |
|---|---|
| Click a socket or the **CORE** dome | Open that unit's view |
| Click a module in the left list | Open that module's panel |
| `BACK` · `S` · `ESC` | Return to the sect view |
| `BUILD ALL` · `B` | Build and activate every module on every unit |
| `TIER +` · `+` | Step every built module up one tier |
| `RESET` · `R` | Fresh sect |

`BUILD ALL` matters: sects start with 3 of 5 modules built per unit, so two
panels per unit are in the NOT BUILT state until you press it. Press it, then
`TIER +` three times, to see every module at every tier.

## What it sets up

- A single standalone `Sect` on a **fixed seed** (`20260813`), so the same sect
  appears every run and a visual change is a real regression rather than a
  different map.
- Every unit's stores topped up to 5,000 each tick, so panels show live numbers
  and BUILD/UPGRADE stay affordable while walking the tree. This is a sandbox,
  not an economy.

## Notes

- Selection runs *after* the draw call, because `Sect::DrawInSectView` is what
  writes each unit's screen position and radius.
- The ring is sized to fit: `coreRadius` is capped so the topmost socket clears
  the day counter. Before that cap the top socket was clipped ~34 px off the
  screen and could not be clicked.
- Builds for Web (`PLATFORM=Web`) like the other harnesses, so it can be walked
  on a phone or tablet; taps map to clicks.
