# Extraction Unit Playtest

An interactive sandbox for the **whole** extraction unit — prospecting,
excavation and beneficiation all running — rather than one module in isolation.

**Play it: `https://navidamin.github.io/raylib-colony/extraction/`**

Pages caches for ten minutes and phone browsers cache harder, so add a query
string when you expect a change: `/extraction/?v=2`.

```bash
# Desktop
cmake --build build --target colony_extraction -j
./build/src/colony_extraction
```

## Controls

Every control is also an on-screen chip, because this is played on a phone.

| Key | Chip | Does |
|-----|------|------|
| — | click a module card | switch between Prospecting / Excavation / Beneficiation |
| `S` | **SURVEY** | instant full survey — sweep, cores, and lab on every sample |
| `P` | **PROS +** | tier up prospecting (widens survey reach and depth) |
| `E` | **DIG +** | tier up excavation (widens dig reach, unlocks machines) |
| `1` `2` `3` | **x1 / x5 / x20** | time acceleration |
| `R` | **RESET** | fresh unit on unsurveyed ground |

## Why time acceleration and instant survey exist

A face takes game days to work out, and a survey needs about a day to repay
what it cost. At 1× neither is visible inside a play session, so the module's
central claim cannot be judged. At ×20 a spot drains in seconds and the pit
visibly advances across the lattice.

**SURVEY** is there so you can A/B the same ground. Note what a spot reads
blind, hit it, and watch the same spot read differently — that difference is
the whole design.

## What is worth looking at

**The readout line, bottom of the excavation panel.** Left is what you have
been *told* is in the selected spot; right is what is actually arriving, and
what share of it is the thing you asked for.

```
SPOT 4,4  Fe 552 (known)          GETTING  1 Fe/day  of  21 moved   (5% useful)
```

Five specific things to judge:

1. **Does an unsurveyed spot read usefully differently from a surveyed one?**
   Press SURVEY and watch the range on the readout close.
2. **Is tier 0 boring?** Reach is 2×2 and the Scoop is the only machine, so
   there is very little to decide for the first stretch. This is a known
   concern, not a bug — see whether it reads as *constrained* or as *empty*.
3. **Does the machine choice matter?** At tier 1 the Bucket Wheel and Bucket
   Drum arrive together as opposites — fast and blunt against slow and choosy.
   Turn AUTO off and try both on the same spot.
4. **Does the useful share respond to the pace slider?** Push pace up and the
   percentage should fall as more waste comes along.
5. **Does the pit advance?** At ×20, worked-out spots should visibly drain and
   the operation should move to a fresh face on its own.

## Diagnosing "the mouse hits the wrong place" on the web

A magenta crosshair marks where the *game* thinks the cursor is, with a
readout of `screen` and `render` size along the bottom. **F9** toggles it.
Load the page with `?debug=1` to add the shell's own view in the top-left
badge: the raw page coordinates, the position relative to the canvas, and
the coordinates emscripten should be handing the game.

If the crosshair sits under the real pointer, input is fine and the problem
is in a panel's hit-testing. If it sits away from it, compare the badge's
`expect=` against the crosshair's `game sees` — the ratio between them names
the layer that is wrong (see `docs/web-deploy-mobile.md` for the three
canvas sizes involved).

## What this sandbox is not

There is no sect, so energy is supplied on a trickle with a cap. That is
deliberate — free energy would hide the power-cap slider doing its job — but it
does mean the energy economy here is not the game's.

Tier upgrades bypass tech and cost, as they do in every harness. The tier
*curve* is testable; the cost of climbing it is not.

## Related

- `tools/playtest/README.md` — the prospecting-only sandbox at `/playtest/`
- `docs/web-deploy-mobile.md` — how these get onto Pages, and the canvas gotchas
- `docs/design/excavation/` — what the module is meant to be
- `colony_sim` and `colony_playthrough` — the same loops without a human
