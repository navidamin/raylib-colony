# darkplate

A standalone renderer for a coded-art style: dark ground, heavy near-black
outlines, stepped tone, and machinery built from plated volumes.

**This directory is self-contained and is not part of the surrounding
project.** No CMake, no engine, no build step, no shared headers, and nothing
outside `darkplate/` is read or written. It happens to live in this repository;
it does not belong to it. Open `assembler.html` in a browser and it runs.

The style is documented in [`STYLE.md`](STYLE.md) — also self-contained.

```
darkplate/
  STYLE.md            the style guide: the world, the line, tone, materials,
                      plated volumes, capture. Read before drawing anything.
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
xdg-open darkplate/assembler.html

# headless renders into build/darkplate/   (needs: npm i playwright)
node darkplate/tools/shoot.js darkplate/assembler.html build/darkplate
```

The stage exposes the capture hooks — `__setState(o)`, `__manual(on)`,
`__step(dt)` — so a harness drives it at fixed dt and stills are reproducible
regardless of headless timing.

## The engine

| Layer | What it owns |
|---|---|
| `core` | tone quantization, the back-face range remap, `Rng(seed)` closures |
| `palette` | the tokens, and the outline constants |
| `material` | `steel()` for turned parts, registered plate **ladders** for flat ones |
| `line` | the flood pass, even-odd cut-outs, edge rules |
| `iso` | projection presets, chamfered volumes, face-local coordinates, `fit()` |
| `parts` | recess, panel, studs, louvres, rail, lamp |

Three things carry most of the weight:

- **Face-local coordinates.** `V.on(face, box)` returns a mapper whose `(u,v)`
  are world units from the face's top-left *as it appears on screen*. Lay out
  an arch, a vent or a bolt pattern like a UI rectangle and it lands correctly
  on any face. Without it every greeble is hand-projected, which is how iso art
  turns into magic numbers nobody can move. `m.at()` is its depth-compensated
  twin, for interiors.
- **Ladders.** A two-point RGB lerp cannot hold saturation, so an amber housing
  shaded that way goes muddy in its midtones. A ladder is an ordered list of
  plate tones; a shade picks a rung and nothing interpolates. Quantization
  stops being a step applied to the data and becomes the data. (STYLE §4.2)
- **Scale as a material property.** `V.lw` / `V.px` / `V.lod`. Line weight and
  stud size are px constants tuned at hero size; at 32 px they *are* the
  sprite, so a component drops greebles rather than rendering them into mud.
  (STYLE §5.6)

## Component 01 — The Assembler

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

The press inside the throat is this component's own invention, not part of the
reference subject; it is gated behind the fine detail threshold.

## Adding a component

1. Read [`STYLE.md`](STYLE.md) §1–§3 (the world), §4 (materials, and §4.2 on
   ladder vs ramp), and §5 if it is a volume.
2. New file in `sprites/`, exporting `draw(ctx, V, st)`. Take the view; do not
   reach for a canvas.
3. Lay every detail out in **face-local coordinates**. If you find yourself
   projecting a point by hand, the primitive you need is missing — add it to
   `parts.js` instead.
4. Gate detail on `V.lod`, and widths through `V.lw`.
5. Render it. Look at the PNG. Render the icon ladder too — that is where a
   sprite's real problems show up.
6. In the same commit, extend `STYLE.md` if the component needed a technique it
   does not have (§7). If it needed nothing new it should look like it belongs;
   if it doesn't, one of you is wrong.
