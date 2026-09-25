# DomeForge — lunar dome sprites, roads and base assembly

A self-contained generator for a stylised lunar-base art set: glass domes with hexagonal
cell panels in metal rims, connector sockets with cavity lights, kerbed roads, cratered
regolith, and the full circular base assembly — as 2D sprites, a sprite sheet, a composed
base image, and an interactive ray-marched 3D view. Everything is procedural: no textures,
no images, no external libraries.

Open `dome-forge.html` in any modern browser. That one file contains the whole tool.

```
dome-forge.html          the tool (all three modules inlined; open this)
dome-forge-engine.js     2D sprite renderer (domes, rims, sockets, lights, sheet)
dome-forge-base.js       roads, ground and the base assembly (2D)
dome-forge-3d.js         WebGL ray-marched 3D viewer (single dome or whole base)
examples/render.js       command-line rendering to PNG with node
samples/                 renders produced with the default configuration
```

The three `.js` files are standalone copies of what is inlined in the HTML. Use them if you
want to render from node or embed the generator in your own pipeline.

---

## 1. The tool

### Tabs (top bar)

| Tab | Shows |
|---|---|
| **Unit dome** | one unit dome sprite at the configured size |
| **Central dome** | the central dome sprite (bigger, eight sockets) |
| **Sheet** | central on top, then one row per colour with four lighting variants (A–D) |
| **Base** | ground + roads + nine domes composed into one image |
| **3D** | ray-marched view; switch between one dome and the whole base, drag to orbit, wheel to zoom |

Zoom only affects the on-screen preview. Rendering time is shown at the top right.

### Bottom bar

- **Download PNG** — the current tab. Sprites and the sheet render at the configured size;
  Base always renders at full `Base size` regardless of the preview scale; 3D saves the
  current view at the canvas resolution.
- **Download sheet** — the sheet, from any tab.
- **Download unit + central** — both sprites as separate PNGs.

### Control panel

Only the levers that matter are exposed as sliders. Every other parameter is still live and
can be edited in the **All parameters (JSON)** section at the bottom (Apply / Copy / Reset).
Pasting a JSON config there restores an exact look; the same JSON works with `examples/render.js`.

**Shape** — which dome is being edited (unit or central), sprite size, dome radius and ring
width as fractions of the sprite, socket count and the angle of the first socket.

**Glass** — colour (with preset chips), ambient and diffuse light, key light angle and
height, *Edge falloff* (darkening as the surface turns away from the viewer), bounce light
on the far limb.

**Highlight** — four lighting presets A–D from the original reference sheet; position of the
specular highlight (as a fraction of the radius, +Y up), its tightness and glow, and the size
of the diamond glint.

**Hex cells** — cell size (fraction of the radius), *Curvature* (how fast cells squash
toward the edge; 1 is a true sphere, higher exaggerates the convexity the way the reference
art does), *Tile tilt* (each cell shaded as a flat tilted facet, blended with smooth shading),
line width and shadow, per-cell brightness variation, and the density of whole cells that light
up near the highlight.

**Frame** — metal colour, profile (Plate is the reference look; Tube/knurled, Double tube,
Chamfered and Simple are alternatives), bevel depth, *Lit edges* (bright painted highlight on
bevels facing the light), mottled texture contrast, inked contour strength, and the width of
the narrow stepped lip around the periphery.

**Socket** — the connector loops that merge into the rim: on/off, width, height, bar
thickness, *Pull into rim* (how far the loop sinks into the ring), *Merge fillet* (radius of
the concave blend where loop and rim meet), the point light in each cavity (colour, glow,
size, halo) and optional extra bar lights on the rim beside each socket.

**Roads** — width, kerb width, junction fillet radius, asphalt and kerb colours, kerb bevel,
kerb segment spacing, and the dashed lane line.

**Ground** — regolith colour, density of large and small craters, crater depth.

**Base layout** — show ground+roads+domes or ground+roads only, base size, preview scale,
dome orbit radius, ring-road radius, central and unit sprite sizes, the three dome colours
(central, cardinal N/E/S/W, diagonal), whether the cardinal spokes continue past the ring
road, and socket toggles for unit and central domes.

**3D view** — dome height (1 = full hemisphere), plate thickness, and whether the light
follows the camera (on: every angle keeps the sprite's lighting; off: the light is pinned to
the world and shading changes as you orbit).

**Output** — pixel size (>1 renders at lower resolution and upscales with hard pixels, for a
pixel-art look; works in every tab including 3D), background on/off and colour, seed.

**Sheet** — the row colours and which lighting variants (letters A–D) to include.

---

## 2. How it is made

All three renderers are built on the same idea: every shape is a signed distance field, and
appearance is computed per pixel from that field. This is why the parts blend into each other
instead of being layered on top of each other.

### Domes (engine)

- **Glass** is a shaded sphere. The hex grid is laid out on the sphere surface (cells keep
  their size on the surface, so on screen they squash progressively toward the limb) with an
  adjustable exaggeration (*Curvature*) and perspective (*Lens*, JSON). Each cell's true
  normal is recovered through the inverse mapping and used for facet shading and for deciding
  which cells light up. Lines are embossed (lighter on the lit side of a cell, darker
  opposite) and thin out toward the edge. On top: a broad specular glow, a diamond glint, limb
  darkening, a bounce light on the far side, a contact shadow from the rim, and a thin bright
  glass lip.
- **Rim** is a height profile revolved around the dome — outer lip, bevel, main band, seam,
  inner band, gap — shaded as metal with a fake studio environment (bright sky above, floor
  bounce below, fresnel at grazing angles), painted highlights on bevels facing the light, a
  mottled patch texture (jittered-grid cell noise with soft borders, at two scales), and an
  inked contour.
- **Sockets** are chamfered-rectangle loops smooth-unioned with the rim, so the outline is one
  piece and the bevel and contour run continuously around the loop with a concave fillet at
  the join. The hollow is cut out with its own inked edge, and a tiny point light with a small
  halo sits in each cavity. Sockets have the same absolute size on every dome (they
  interlock), so they are proportionally smaller on the central dome.
- The sprite is auto-centred so rim and sockets fit, anti-aliased with 2×2 supersampling
  (`ssaa`), and returned as RGBA with the dome centre position (`cx`, `cy`).

### Roads and ground (base)

- **Roads** are one shape: straight segments plus the ring road, smooth-unioned with a fillet
  radius, and the dome rims joined into the same union with their own fillet. The kerb is a
  rounded ridge along that outline: its lit face is the outer side on edges that face the
  light and the inner side on edges that face away, so both kerbs of a street read the same.
  The road is a raised slab (lit slope beside it toward the light, cast shadow on the far
  side), the rims throw a shadow onto the asphalt where a road meets them, and the lane line is
  dashed along the nearest primitive. Colours were sampled from the reference art.
- **Ground** is regolith with mottling, grain, and three layers of jittered-grid craters
  (parabolic bowl with a raised rim), shaded by the same key light with bowl occlusion.
- **Assembly**: central dome at the centre; eight unit domes on an orbit at 45° steps,
  cardinal ones in one colour, diagonal ones in another; spokes centre→dome→ring road;
  optionally the cardinal spokes continue past the ring. Unit domes carry two sockets (one
  toward the centre, one toward the ring road); the central dome carries eight. Dome sprites
  are rendered once per configuration and cached.

### 3D (viewer)

A single WebGL 1 fragment shader ray-marches the scene:

- Domes are instances of one SDF (glass cap, plate with stepped inner band, lip and seam,
  loops merged with the plate, light spheres in the cavities). Nine instances for the base,
  each with its own position, scale, colour and socket orientation; the shader only evaluates
  the nearest instance per sample.
- Roads are the same 2D union extruded into a slab with a rounded kerb tube along the outline;
  the ground is an infinite plane whose craters bend the lighting rather than the geometry
  (intersected analytically so it costs nothing).
- Real cast shadows (soft shadow march against the whole scene), ambient occlusion in the
  seams, the same hex cell, facet and highlight logic as the sprites, and emissive socket
  lights with glow on the surrounding plate.

Performance scales with canvas size. If a machine struggles, set **Pixel size** to 2 or 3.

---

## 3. Rendering from node

```
node examples/render.js unit
node examples/render.js central '{"color":"#2f7fd6"}'
node examples/render.js sheet
node examples/render.js base 0.5              # half-size preview
node examples/render.js base                  # full 1254 px base (~10 s)
```

Any config key can be overridden with the JSON argument, including whole sub-objects such as
`"unit": {"size": 512, ...}`. Copy the JSON from the tool's panel to reproduce a look exactly.

Programmatic use:

```js
const DomeForge     = require('./dome-forge-engine.js');
const DomeForgeBase = require('./dome-forge-base.js');   // requires the engine

const cfg = Object.assign({}, DomeForge.DEFAULTS, DomeForgeBase.DEFAULTS, { color: '#d9a21b' });

const sprite = DomeForge.render(cfg, 'unit');          // {width, height, data: RGBA, cx, cy}
const sheet  = DomeForge.renderSheet(cfg);
const base   = DomeForgeBase.renderBase(cfg, DomeForge, { scale: 1 });
const roads  = DomeForgeBase.renderRoads(cfg, W, H, prims, scale);   // road layer only
const ground = DomeForgeBase.renderGround(cfg, W, H, scale);

// in a browser
DomeForge.toCanvas(sprite, canvasElement, cfg.pixelSize);
```

The 3D viewer needs a WebGL context:

```js
const v = DomeForge3D.create(gl);
v.setConfig(cfg, 'unit');                    // or 'central'
v.setConfig(cfg, 'base', DomeForgeBase);     // whole assembly
v.setCamera({ yaw: -25, pitch: 55, dist: 3.9 * v.state.boundR, fov: 30 });
v.render(width, height);
```

`data` is a `Uint8ClampedArray` in row-major RGBA, straight alpha; sprites are transparent
outside the dome when `bgOn` is false (the base and sheet always set this for their sprites).

---

## 4. Units and sizes

- Sprite geometry (`domeRadius`, `ringWidth`) is a fraction of the sprite size, so a 128 px
  and a 1024 px sprite have the same proportions.
- Socket, kerb and lane dimensions are pixels at a 256 px unit sprite and scale with the
  unit sprite size, so sockets match across domes.
- Base layout values (`orbit`, `ringRoadR`, `roadW`, ...) are pixels at a 1254 px base and
  scale with `baseSize`.
- In 3D the unit dome radius is 1 world unit; everything else is converted from the same
  config.

---

## 5. Config reference (defaults)

The engine has 88 parameters and the base 49. The important ones are described under
"Control panel" above; the rest are visible with their defaults in the JSON panel. A few
worth knowing that have no slider:

| Key | What |
|---|---|
| `hexLens` | perspective in the 2D hex mapping (0 orthographic … 1 extreme) |
| `hexRot` | rotates the hex grid |
| `specWhite`, `glintAspect`, `glintStrength` | highlight colour blend, glint shape and strength |
| `edgeShadow`, `edgeShadowW`, `edgeLine` | rim contact shadow on the glass and the bright glass lip |
| `frameProfile` `'classic'` + `segments`, `segDepth` | knurled outer tube |
| `grainStyle` `'mottled' \| 'speckle' \| 'brushed' \| 'smooth'`, `grainScale` | metal texture |
| `socketCorner`, `socketCorners` `'chamfer' \| 'round'`, `hollowDark` | loop corner shape and cavity shading |
| `rimLights`, `lightAngle`, `lightRadial`, `lightLen`, `lightW` | extra bar lights on the rim |
| `central.sides`, `central.corner`, `central.socketsAtCorners` | polygonal central rim (0 sides = circle) |
| `bankW`, `bankLight`, `bankShadow`, `domeShadowW`, `domeShadow` | raised-road embankment and rim shadow on asphalt |
| `craterRim`, `groundMottle`, `groundGrain` | ground detail |
| `offsetY`, `roadOuterW`, `domeRoads`, `socketsToCentre` | base layout details |
| `fov3d`, `shadow3d`, `autoRotate`, `roadHeight3d` | 3D view |
| `ssaa`, `levels`, `bgNoise` | anti-aliasing, colour quantisation, background grain |

---

## 6. Notes

- The browser tool has no dependencies and works from a `file://` URL.
- 3D needs WebGL 1; if it is unavailable the tab says so and the 2D tabs are unaffected.
- All randomness is seeded (`seed`); the same config always produces the same image.
- Full-size base export blocks the UI for about ten seconds; the preview uses `previewScale`.
