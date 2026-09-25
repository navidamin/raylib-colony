# DomeForge → Sect view: study and port plan

**Status: STUDY.** Nothing in the game uses DomeForge yet. This document records
what it is, how it maps onto the current sect view, and how to bring it in.

Source: [`prototypes/dome-forge/`](../../../prototypes/dome-forge/), the
user's procedural generator for the lunar-base art set. Its own README is the
full reference. `dome-forge.html` inlines the three `.js` files **line for
line** (verified), so the `.js` files are the canonical source. Port those
once, not the HTML.

![current sect view vs DomeForge base](domeforge-vs-current.png)

*Left: the sect view today (rendered from this branch, Mare Imbrium). Right:
DomeForge's `lunar-base-full.png`.*

---

## 1. What DomeForge is

Three renderers driven by **one plain config object** (88 engine parameters +
49 base parameters, all with defaults):

| File | Renders | How |
|---|---|---|
| `dome-forge-engine.js` (690 lines) | one dome sprite, `unit` or `central`, and the sprite sheet | per-pixel SDF shading into an RGBA buffer, 2×2 SSAA |
| `dome-forge-base.js` (279 lines) | ground, roads, and the full base assembly | the same approach: road union SDF, cratered regolith, domes blitted from a cache |
| `dome-forge-3d.js` (690 lines) | the base or one dome in 3D | one WebGL 1 fragment shader that ray-marches the scene |

The properties that matter for porting:

- **It is not Canvas 2D.** No paths, no gradients, no `shadowBlur`. Every pixel
  is computed from signed-distance fields, and the only Canvas call puts the
  finished buffer on screen. So the c2d kit
  ([`js-graphics-port.md`](../../guides/js-graphics-port.md)) does **not**
  apply. Its *method* does: verbatim source, reference renders and a pixel
  diff gate.
- **Deterministic.** All randomness goes through a seeded integer hash
  (`hashInt`, `Math.imul`-based), so it maps exactly onto `uint32_t`. Given
  the same config, a C port can reproduce the JS almost bit for bit. Use
  `double` where the JS does arithmetic, so float64 rounding doesn't drift.
- **Headless reference renders already exist.**
  `node examples/render.js unit|central|sheet|base [scale] [json]` writes PNGs
  with no browser. Measured here: unit 256 px in 0.4 s, central 560 px in
  1.4 s, base at half size in 3.4 s.
- **Built to be baked.** `renderBase` already caches sprites by
  `(kind, size, colour, sockets, socket angle)`. That's the same pattern as the
  game's current `GetBakedDomeTexture`.
- **The 3D shader is portable.** GLSL ES 1.0 with no textures, no extensions
  and only constant loop bounds. raylib can load it as a fragment shader
  (`#version 100` on web, a small header swap for GL 3.3 on desktop).

## 2. Element by element: today → DomeForge

The layout is the same idea (a central dome, eight unit domes at 45° steps, a
ring road). What's drawn differs:

| Sect view today (`Sect::DrawInSectView`) | DomeForge | Note |
|---|---|---|
| Core: ray-shaded green sphere + hex glass (`DrawDomeSphere`) | `central` sprite: hex cells on a sphere, plate rim, 8 sockets; optional polygon rim (`central.sides`) | DomeForge's glass is much richer: faceted cells, glint, bounce light, contact shadow |
| 8 unit domes with procedural glyph + label (`DrawUnitDomeStation`) | `unit` sprite, 2 sockets (toward centre + toward ring road) | DomeForge has **no icons or labels**. It colours cardinal vs diagonal domes only |
| Connector arms with green conduit + LED sockets (`DrawConnectorArm`, `DrawSocket`) | road spokes centre → dome → ring road, merged into the rims; cavity lights in the socket loops | LED status maps naturally onto the **socket cavity light colour** |
| Ring road with lamp seams (`DrawRingRoad`) | kerbed ring road, filleted into the spokes, dashed lane line | |
| Entry rails at the bottom (`DrawEntryRail`) | none (`spokesBeyond` extends the cardinal spokes instead) | decision needed |
| Background: real-imagery terrain (`DrawSectTerrainBackground`) | its own cratered regolith (`renderGround`) | **keep the real terrain.** `baseView: 'roads'` and `renderRoads` give the road layer without the ground |
| Selection: the selected unit dome turns amber | none | a glass colour per state is one config field (`color`) |
| "Development: N%" drawn over the core | none | keep as overlay text |
| Hit testing via `SetUnitPosInSectView` / radius | positions from `layout()` | keep the game's hit tests and feed them DomeForge's layout |

Layout constants for comparison. DomeForge is in px at a 1254 px base; the
game is in fractions of screen height `h`:

| | DomeForge | as a fraction of the base | game today |
|---|---|---|---|
| unit orbit radius | 340 | 0.271 | 0.325 h |
| ring road radius | 485 | 0.387 | 0.443 h |
| central sprite size | 475 | 0.379 | core radius 0.15 h |
| unit sprite size | 236 | 0.188 | unit radius 0.085 h |
| vertical offset | −42 | | −0.04 h |

## 3. Port approach

**Recommended: a C port of the engine and the road layer, baked to textures.**

1. `src/DomeForge/` (C or C++, following the module-architecture guide):
   `domeforge_config` (DEFAULTS as a struct), `domeforge_engine` (render
   unit/central → RGBA), `domeforge_base` (layout + roads; ground optional).
   Translate function by function with the same names, the same order and the
   same arithmetic, in `double`.
2. Bake at sect load, keyed like `renderBase`'s cache. Sizes follow the display
   scale (1×–3×), so a 3× screen bakes about a 650 px central sprite. Budget
   the bake and cache it; never render per frame.
3. `Sect::DrawInSectView` draws the baked road layer and dome textures over the
   real terrain, then the game overlays (development %, labels, selection,
   hover).

Alternatives, and why they aren't first:

- **Shader port of the 2D engine.** Live lighting and animated socket lights
  for free, but it's harder to diff and costs GPU every frame on phones. A good
  second step once the CPU port is the reference.
- **The 3D viewer as a raylib shader.** Nearly a direct lift. Worth doing for a
  tilted or hero view of the sect, but it's a new camera mode, not a
  replacement for the top-down view.
- **Pre-rendered PNGs from node.** The quickest route, but it freezes colours,
  states and sizes into assets, and the game needs those to vary at runtime.
  Fallback only.

## 4. Verification

The same shape of gate as the c2d kit, with a tighter target because the
arithmetic is shared rather than approximated:

- **Reference:** `node prototypes/dome-forge/examples/render.js <kind> '<json>'`
  at a fixed config.
- **Port:** a small driver that renders the same kind and config through the C
  engine to PNG.
- **Diff:** `diff.py`, from the c2d kit's `tools/visdiff/` or copied. Target
  **under 1%** differing pixels. Expect near zero, because the math is
  identical, not approximated.
- **In context:** `tools/preview/preview.sh --view sect`, looked at every time.

## 5. Open questions (for the user, before building)

1. **Unit colours.** DomeForge colours by position (cardinal green, diagonal
   grey). Should the game colour by unit type, by status (active/idle), by
   selection, or keep the reference look?
2. **Unit identity.** DomeForge domes carry no icon or label. Keep the game's
   glyph and label on or under the glass, or move identity into hover and
   tooltip?
3. **Ground.** Keep the real-imagery terrain under the roads (recommended), or
   use DomeForge's regolith?
4. **Entry rails.** Keep them, replace them with `spokesBeyond` roads, or drop
   them?
5. **Central rim.** Circular (the base sample) or polygonal (the sprite sheet's
   octagon, `central.sides = 8`)?
6. **3D.** Is the ray-marched view wanted in the game, and where?

## 6. Stages

| # | Stage | Done when |
|---|---|---|
| S1 | Reference renders pinned: a script produces unit/central/roads PNGs at fixed configs | PNGs committed as the diff baseline |
| S2 | C engine: `unit` and `central` sprites | diff under 1% for both, 2 sockets and 8 sockets |
| S3 | C road layer + layout | roads-only render diffs under 1% |
| S4 | Sect view draws the baked set over the real terrain; hit tests from the layout | preview renders looked at at 1× and 2× |
| S5 | Game state on the art: status → socket lights, selection → glass colour, overlays per the answers to §5 | every state rendered and looked at |
| S6 | Retire the old dome drawing, with its graveyard record | this branch has no `docs/design/graveyard/`; bring the rule's directory over from `main` first |
