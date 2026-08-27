# gfx — the Dark Plating coded-art engine

A standalone renderer for the coded-art style this repo developed while
building the drill rig. **It has no dependency on the game** — no CMake, no
raylib, no headers. Open `assembler.html` in a browser and it runs.

The style itself is documented in
[`docs/design/graphics/dark-plating.md`](../docs/design/graphics/dark-plating.md).
This is its implementation, plus the layer the drill never needed: **volumes**.

```
gfx/
  assembler.html      the stage — hero canvas, controls, and the icon size ladder
  engine/
    core.js           numbers, tone quantization, back-face remap, seeded RNG
    palette.js        §1.2 tokens and the line constants
    material.js       steel() for turned parts; plate LADDERS for flat volumes
    line.js           §2 — the flood pass, cut-outs, edge rules
    iso.js            projection presets, chamfered solids, FACE-LOCAL coords
    parts.js          machine furniture: recess, panel, studs, louvres, rail, lamp
  sprites/
    assembler.js      component 01
  tools/
    shoot.js          headless capture: poses, projections, zoom crops, icon strip
```

## Running it

```bash
# just open it — plain <script> tags, no bundler, no server
xdg-open gfx/assembler.html

# headless renders into build/gfx/  (needs: npm i playwright)
node gfx/tools/shoot.js gfx/assembler.html build/gfx
```

The stage exposes the §11 capture hooks — `__setState(o)`, `__manual(on)`,
`__step(dt)` — so a harness drives it at fixed dt and stills are reproducible
regardless of headless timing.

## What this adds to the drill's technique

Everything in `core`, `palette`, `material` and `line` is the drill's own
craft, lifted out of `drill-rig.html` where it lived as file-scope globals
bound to one canvas. The changes worth naming:

| | Drill | Here |
|---|---|---|
| Binding | module globals `CTX`, `W`, `H` | every helper takes `ctx` and a view |
| Determinism | one global `grainSeed`, reset by hand per pass | `Rng(seed)` closures; a pass owns its stream |
| Banded fills | `steelBands` runs along screen **x** only | `bandsAlong(p0, p1, …)` runs along any axis |
| Materials | one two-point RGB ramp | ramp **plus** registered plate ladders (§4.6) |
| Geometry | screen-space rects and swept cylinders | axonometric volumes with face-local coordinates |
| Scale | px constants, one size | `V.lw` / `V.px` / `V.lod` — a detail budget |

The genuinely new section of the style guide is
[§7 Plated Volumes](../docs/design/graphics/dark-plating.md#7-plated-volumes--the-iso-solid-family):
the projection presets, the chamfered volume, face-local coordinates, the
recess, painter order for an overhanging base, and scale as a material
property.

## Component 01 — The Assembler

Subject: the top-left tile of a factory-asset reference sheet. Drawn in Dark
Plating, so it comes out cooler and harder-edged than the reference — it has to
stand next to the drill rig, not next to the sheet it came from.

Four stacked volumes on one 1×1 world footprint:

```
plinth   dark oversized slab — the machine is bolted down, not floating
frame    amber housing band, with the doorway cut through its south face
hood     chamfered plated steel upper body, where the light lands
cap      dark raised access panel inset on the hood's top face
```

State is `work` and `heat`. Heat is a **field** around the throat, not a global
multiplier — feeding one flat scalar to every material turns the whole machine
tan, which is CG lighting wearing a stepped-tone costume. The status lamp reads
state semantically: cyan idle → amber driven → hot over-driven.

## Adding a component

1. Read §1–§3 (the world), §4 (metal, and §4.6 on ladder vs ramp), and §7 if
   it is a volume.
2. New file in `sprites/`, exporting `draw(ctx, V, st)`. Take the view; do not
   reach for a canvas.
3. Lay every detail out in **face-local coordinates**. If you find yourself
   projecting a point by hand, the primitive you need is missing — add it to
   `parts.js` instead.
4. Gate detail on `V.lod`, and widths through `V.lw`.
5. Render it. Look at the PNG. Render the icon ladder too — that is where a
   sprite's real problems show up.
6. In the same commit, extend `dark-plating.md` if the component needed a
   technique it does not have. If it needed nothing new it should look like it
   belongs; if it doesn't, one of you is wrong.
