# Ground texture at the site level

*Design record, 2026-09-03. Status: prototype exists behind `--chain`;
nothing ships yet.*

The site level of `lunar_map` — the 25 km window where the base is
placed — looks like uniform grey noise over most of the Moon, not like
the photographic ground the game's Sect view draws. This document
identifies why, in full, and designs the fix across the platforms that
matter: **web in current browsers and laptops first, everything else
degrading gracefully by measurement rather than by assumption.**

---

## 1. The decision in one paragraph

Lay the terrain synthesizer's own 25 km level over the site window as a
**texture layer, not a replacement**: the LOLA elevation keeps supplying
form, normals, shadows, the sun and the buildability verdict; the chain
supplies the surface detail that neither data source has. Build it once
per window, at a resolution chosen per device by a timed probe, from a
mosaic the GPU already holds. Leave level 2 alone. Keep the existing
crater synthesis, because on the maria it is better than the chain.

---

## 2. The problem — breadth

Seven causes, each verified against the code on 2026-09-03.

### 2.1 Thirteen real samples

The global DEM (`LDEM_16`) is 16 px/deg ≈ **1.90 km per sample**. Across
the site level's 25 km that is **13.2 measured elevations**, stretched
over the whole screen. Level 2's 200 km gets 105 — coarse, but eight
times denser.

### 2.2 The good data covers 0.18 % of the Moon

Two SLDEM overlays at 512 px/deg (59 m/sample) give **422 samples** across
25 km, and that ground looks genuinely detailed. They are Imbrium
(30.0–35.8 N, 18.6–12.6 W) and Tycho (47.3–39.3 S, 15.4–7.4 W). Together,
equal-area, they are 76 of the Moon's 41 253 deg² — **0.18 %**. "For most
of the map" is exactly right: the player sees real detail in two patches
and the fallback everywhere else.

### 2.3 The WAC imagery is deliberately faded out below 100 km

```cpp
albedoStrength = clamp((worldWidthKm - 20) / 80, 0, 1)   // lunarmap_main.cpp
```

200 km → **1.0**, full photographic albedo. 25 km → **0.06**, effectively
none. The comment explains why: at 25 km only **22 × 20 WAC texels** span
the frame (1.33 km/px), and stretching them paints "huge soft tonal
blobs unrelated to the ground — the single biggest source of the
expressionist wash." The fade is correct. Its consequence is that the
site level has no tonal information at all.

### 2.4 The existing sub-floor synthesis makes geometry only

`LolaDem::Window(..., detailStrength)` already invents relief below the
DEM floor — a fractal regolith spectrum plus a clustered, age-weighted
population of small craters, deterministic per location — and `lunar_map`
runs it at 1.0 by default. It is careful work (degraded dishes,
elliptical rims, playtest-tuned density). But it produces **shaded
relief with no albedo**: on the maria the craters read well; on the
highlands it is featureless grain over three soft blobs.

### 2.5 Two synthesizers that were never composed

| | `lola_dem` sub-floor synthesis | `terrain_synthesis` chain |
|---|---|---|
| Input | LOLA elevation | WAC imagery |
| Invents | height (fractal + craters) | surface (sharpen, relight, grain, boulders) |
| Consumer | `lunar_map` levels 2–3 | the game's Planet / Colony / Sect views |
| Output | a height field, lit by the lunar shader | a lit RGB image |

They answer different questions — *is this ground buildable* versus
*what does it look like* — and were built for different consumers. The
game's Sect view looks photographic because it uses the chain; the site
level looks synthetic because it uses the other one. Nothing joins them.

### 2.6 The chain's scales almost, but not quite, fit the ladder

Chain spans are hard-coded **100 / 25 / 5 km**. The ladder is
3000 / 200 / 25 km. Chain level 1 is exactly the site window — for a
*square* window. But the site window is built as
`spanKm = 25 × sceneAspect` so the terrain covers a landscape screen: on
16:9 that is **44.5 km**, and a 25 km chain covers only the central 56 %.
Level 2 (200 km) has no chain level at all; the chain tops out at 100.

### 2.7 The maria and the highlands want opposite things

Rendered side by side at two locations with no overlay ([figures/site-texture-four-ways.jpg](figures/site-texture-four-ways.jpg),
2026-09-03):

- **Highland (25 S 5 E):** today is featureless grain; the chain has
  landform, light and texture. The chain wins decisively.
- **Mare (18 N 35 E):** today has convincing small craters; the chain is
  dark mush, because flat dark imagery gives it nothing to amplify. **The
  chain loses.**

So "replace the site level with the chain" fixes the highlands and
breaks the maria. Any solution is a blend, and the crater synthesis
stays.

---

## 3. The problem — depth

### 3.1 The verdict is already insulated — this is the enabling fact

`LolaDem::EvaluateSite` reads elevation straight from the DEM and, in its
own words, *"deliberately bypasses `Window()`'s synthesis"*. **Nothing in
this design can change what the game judges.** Only the picture is in
play. (The picture must still not *contradict* the judgement — see §4.)

### 3.2 Cost scales quadratically and the ladder's top rung is large

```cpp
fullRes = clamp(1.15 × max(screenW, screenH), 512, 2048)
rungs   = { fullRes/4, fullRes/2, fullRes }             // 512 / 1024 / 2048 on 1080p
```

Chain cost per build, measured (§8 has provenance):

| res | real GPU, Chrome | laptop CPU | software GL (llvmpipe) | software GL (SwiftShader, browser) |
|---|---|---|---|---|
| 256 | 18 ms | 65 ms | 32 ms | 168 ms |
| 512 | **22 ms** | 276 ms | 77 ms | 1 162 ms |
| 1024 | **100 ms** | 1 577 ms | 394 ms | 9 869 ms |
| 2048 | **446 ms** | 7 541 ms | 1 803 ms | **44 064 ms** |

Plus a **one-time ≈ 0.9 s** on the first build of a session (mosaic
decode, shader compile, target allocation).

### 3.3 The cost is per-pixel procedural work, not bandwidth

The fused shader evaluates `heightAt()` five times per pixel for the
hillshade gradient and then **marches up to 64 shadow steps**, each one
calling it again — and each call is a 5-octave fbm. That is hundreds of
noise evaluations per pixel. Consequences: the cost does not shrink on
faster memory, it *does* shrink with fewer shadow steps, and a software
rasteriser is catastrophic (44 s).

### 3.4 Grain is tied to resolution, so under-rendering is visible

The chain's grain size is `k = res / 300` so that feature sizes stay
physical. A 512 chain shown at 2048 has **visibly chunkier texture** than
a native 2048 one ([figures/site-texture-res-check.jpg](figures/site-texture-res-check.jpg)). "Render it once at 512 and
upsample" is therefore a real quality trade, acceptable on a phone
(1.6× upsample) and not on a maximised desktop browser (4×).

### 3.5 Web memory is the binding constraint there, not time

- `EnsureWacLoaded()` builds a **128 MB `float`** copy of the 8192 × 4096
  mosaic. `lunar_map` does not pay this today — the globe uploads its own
  8-bit texture. Route A pulls it in. It survived in desktop Chrome
  (§8); a phone is unknown and this is the allocation most likely to
  fail there.
- `terrain_gpu` allocates **nine full-resolution RGBA targets** (six
  scratch + three outputs): 9 MB at 512, 36 MB at 1024, **144 MB at
  2048**.
- WebGL1 guarantees only 2048² textures. Measured: 16384 on this
  laptop's GPU, 8192 under SwiftShader. Phones vary.

### 3.6 Web has no worker threads and no probe

The CPU chain would freeze the page, so `GetTerrainPath()` hard-wires
web to GPU — reasonable when WebGL means a GPU, catastrophic when it
means SwiftShader (locked-down machines, some phones in power saving):
1.2 s at 512, 44 s at 2048, with nothing to catch it. Desktop already
*times* a probe chain and picks a path from the result; web does not.

### 3.7 The chain has its own lighting baked in

Its output carries hillshade and cast shadows. Laid under the DEM
shader's sun, the ground is lit twice — over-dark shadows and a "dirty"
look. Any composite has to remove the chain's low frequencies or export
something unlit.

---

## 4. Invariants

These hold before and after, and the test plan (§7) checks each.

1. **The verdict is measured, never amplified.** `EvaluateSite` keeps
   bypassing synthesis. (Already true; kept.)
2. **Form comes from the DEM.** Slope you can see is slope the verdict
   saw. The chain may add *texture*; it may not move a ridge.
3. **One light.** Whatever the chain contributes is either unlit or
   high-passed. Nothing is shaded twice.
4. **Off by default until measured on device.** The flag stays a flag
   until §7's acceptance criteria are met on the platforms that matter.
5. **Degrade, never stall.** No platform waits more than the budget in
   §5.6 for the layer. If the layer cannot be afforded, the site level
   looks as it does today — which is acceptable, not broken.
6. **Level 2 is untouched** in phase 1. It works; it has no matching
   chain level; the player is not complaining about it.

---

## 5. The design

### 5.1 What the chain contributes — two phases

**Phase 1 — high-passed texture (prototyped, working).** Take chain
level 1, subtract a four-tap local mean in the shader, and multiply the
result into the surface albedo *before* the DEM lights it:

```glsl
float c = TEX(chainMap, cuv).r;
float m = 0.25 * (four neighbours at ±chainTexel);
surface *= 1.0 + (c - m) * chainStrength * inside * 6.0;
```

The high-pass discards exactly the low frequencies where the baked
lighting lives, so invariant 3 holds by construction. It also
self-attenuates on flat imagery — where the chain has little contrast,
`(c − m)` is small — which is why the maria kept their craters in the
prototype without an explicit blend weight. Its limit: only the chain's
*fine* detail comes across; its dramatic relit landforms do not. That
is the right trade at the site level, because those landforms are
albedo relit as if it were topography, and the DEM cannot confirm
them.

**Phase 2 — export height and unlit albedo, not a picture.** Inside
`TextureModulate` (and the fused GPU pass) the chain already has the two
things we actually want, before it shades them: a relief field and an
unlit luminance. Add an output mode that emits **R = sub-floor height
(normalised), G = unlit albedo** and skips the shading and shadow march.
`lunar_map` then:

- adds the height, band-limited to wavelengths below the DEM floor, into
  the window's normal map — so the chain's undulation, grain and
  boulders become *relief* the one lunar shader lights, and the tilt
  view sees them too;
- multiplies the unlit albedo into the surface, replacing the faded WAC
  with something that does not blob because the relief now carries the
  fine detail.

Phase 2 removes the double-lighting problem rather than filtering
around it, costs *less* per build (no shadow march — the single largest
per-pixel term), needs one output target instead of three, and gives one
render path. It is the proper unification of §2.5. Phase 1 ships first
because it exists and is safe; phase 2 is where this should end up.

### 5.2 Where it applies

**Level 3 only.** Level 2 keeps its current render: full WAC albedo,
105 DEM samples, curvature emphasis. If a 200 km chain level is ever
wanted, the chain's top span needs generalising (§5.3 does most of that
work) — but nobody has asked, and §4.6 says leave it.

### 5.3 Coverage: parameterise the crop, do not fade the edge

Today's chain crops each level to a fixed fraction of the one above
(`frac = spans[lvl] / spans[lvl−1]`). Generalise the last step: **crop
level 0 to the window's actual span**, `frac = windowSpanKm / 100`, so a
44.5 km landscape window gets a 44.5 km layer. The prototype's edge fade
(`inside`) becomes unnecessary. This is a small change in
`GenerateChainInternal` — the loop already computes `frac` per level —
exposed as `GenerateTerrainChainForSpan(lat, lon, res, spanKm, …)`. The
game's 100/25/5 callers are unaffected.

### 5.4 Maria and highlands

Phase 1 needs no explicit weight: the high-pass self-attenuates
(§5.1), and the crater synthesis keeps running underneath. Phase 2's
grain is uniform regardless of imagery, so its **height** contribution
should be weighted by local imagery contrast — which `SharpenAdaptive`
already computes as a percentile spread — while the **albedo**
contribution self-attenuates as before. The crater population remains
the dominant texture on the maria in both phases; it is not replaced.

### 5.5 Resolution and scheduling

- **Chain resolution = `min(fullRes, tierCap)`**, where `tierCap` comes
  from §5.6. Not the ladder rung: the chain is built **once per window**,
  not once per sharpening rung, and it does not track the 1.15× top rung
  — display resolution is the honest ceiling for a texture layer.
- **Its own stage.** The ladder already builds draft → mid → full. The
  chain arrives after the draft rung is on screen, never before, so the
  first frame of a new window is as fast as today. On the GPU path it is
  submitted on the frame after the draft; on the CPU path it runs on the
  worker thread the game already has (`TerrainWorker`), which `lunar_map`
  should adopt rather than blocking.
- **Kept across rungs.** The three sharpening rungs share one chain
  texture; only the DEM height map rebuilds.
- **Cached with the window.** The built-window cache (`SameWindowKey`)
  gains the chain texture as part of the entry, so backing out and in
  again is free, as it is for the DEM.

### 5.6 Platform tiers — measured, not named

One rule everywhere: **time a 256 px chain on the first build, then set
the cap from the result.** This is the probe `terrain_gpu` already runs
on desktop, extended to every platform including web, replacing the
"browser ⇒ GPU ⇒ fine" assumption that §3.6 shows is wrong.

| Tier | Detected by | Cap | Budget per build | Who lands here |
|---|---|---|---|---|
| **A — real GPU** | probe ≤ 25 ms at 256 | 1024 | ≤ 100 ms | laptops with a GPU; current browsers on a GPU |
| **B — CPU only** | GPU probe fails or > 25 ms, threads available | 512 (CPU path) | ≈ 300 ms, off-thread | desktop with software GL (this WSL box) |
| **C — GPU but slow** | probe 25–150 ms at 256 | 512 | ≤ 1.2 s, staged | weak mobile GPUs |
| **D — no viable path** | probe > 150 ms at 256, no threads | **layer disabled** | 0 | software-GL browsers (SwiftShader) |

The budgets are set so that no tier ever produces a stall the player
notices as one: 100 ms on a one-off window build reads as the ladder
sharpening; 1.2 s staged behind a draft rung is tolerable on a phone;
44 s is not a tier, it is a bug, and tier D exists so it cannot happen.
The probe result is logged (`CHAINBENCH:`-style) so a report from a
device tells us which tier it took and why.

The 256 px probe costs 18 ms on tier A and 168 ms on tier D — cheap
enough to run every session, and the one-time 0.9 s is paid anyway if
the layer is used at all.

### 5.7 Memory on web

Three changes, in order of leverage:

1. **Do not build the 128 MB float mosaic.** On the GPU path
   `GetTerrainMacroCrop` needs a ~75 × 75 crop of the mosaic at native
   resolution, plus a percentile sort of that crop. The globe already
   holds the mosaic as an 8-bit GPU texture (2048 wide on web, 8192 on
   desktop). Render the crop from that texture in a tiny pass, read back
   75² bytes (22 KB), sharpen on the CPU as now. **Zero CPU-side
   mosaic.** On desktop the float copy can stay for the CPU path, or
   become `uint8` (32 MB) — either is fine; on web it must go.
2. **Cap targets by tier** (§5.6): 9 MB at 512, 36 MB at 1024. 2048 is
   never allocated on web.
3. **Query the real limits.** `MAX_TEXTURE_SIZE` and, where exposed,
   memory hints — set the cap from the device, not from the word "web".

### 5.8 What the game gets for free

Phase 2's height/albedo export and §5.3's span parameter are changes to
the shared chain, so the game's Sect view can use the same unlit
output if it ever wants to light the ground with its own sun. Not a
goal here; worth knowing it falls out.

---

## 6. What does not change

- `EvaluateSite` and every number on the level card.
- Level 2's render.
- The DEM window build, the mesh, the tilt view, the sun controls.
- The crater synthesis (`detailStrength`), which stays at 1.0.
- The two SLDEM overlays keep their real detail; the layer is additive
  on top and, being high-passed, adds little where the DEM already has
  fine relief.

---

## 7. Delivery plan and acceptance

**Phase 0 — already done.** `--chain` / `--chain-strength` prototype
(high-pass in shader, GPU or CPU path, edge fade). `?chainbench=1` on
web. Rendered at two locations; ladder still walks with the flag off;
162/162 tests.

**Phase 1 — ship the texture layer.**
1. Span-parameterised crop (§5.3); remove the edge fade.
2. Build once per window as its own stage, cached with the window (§5.5).
3. The probe and tier caps on every platform (§5.6), with the tier
   logged.
4. Web: drop the float mosaic; crop from the globe's texture (§5.7.1).
5. Flip the default on for tiers A–C.

*Accept when:* on a real-GPU laptop the site window's build time grows
by ≤ 100 ms and the highland/mare pair reads as in [figures/site-texture-route-a.jpg](figures/site-texture-route-a.jpg); in
Chrome on this laptop the probe lands tier A and a window costs ≤ 100 ms;
under `--use-angle=swiftshader` the probe lands tier D and the layer is
off with no stall; on this WSL box the probe lands tier B and the layer
arrives off-thread within 300 ms of the draft; `--siteshot --chain`
renders without double-lighting artefacts (compare against the phase 0
reference frames); 162/162 tests; the far-side regions, the globe and
the verdict card are unchanged pixel-for-pixel with the flag off.

**Phase 2 — unlit export.**
1. Height + unlit-albedo output mode in `TextureModulate` and the fused
   pass, skipping the shadow march.
2. `lunar_map` adds height below the floor into the normal map and
   multiplies the albedo; delete the in-shader high-pass.
3. Contrast-weighted height on the maria (§5.4).

*Accept when:* the tilt view shows the chain's relief as relief; per-build
cost is *lower* than phase 1 at equal resolution; the mare pair still
keeps its craters; nothing in §6 has moved.

**Phase 3 — mobile.** Measure on a real phone: probe tier, memory,
whether §5.7 holds. Not before phase 1 lands; not estimated here,
because §8 shows how wrong an estimate for the web was.

---

## 8. Measurements — provenance

All 2026-09-03, on the LOLA branch (`claude/lunar-elevation-lola-dem-1dcdtj`),
this laptop (WSL2, no GPU passthrough to Linux; Windows Chrome has the
real GPU).

- **Sample counts and coverage:** computed from `LDEM_16` (16 px/deg),
  the two `sldem_*.json` overlays, and `KM_DEG = π·1737.4/180`.
- **Chain, laptop CPU and llvmpipe GPU:** `terrain_probe --res N --path
  both`, which forces completion with a read-back.
- **Window build:** differenced whole-run wall times of
  `lunar_map --pick −25,5 --span 25 --demres {256,2048} --out`;
  6.8–7.2 s vs 3.3–4.2 s, so **≈ 3 s at 2048, ± a lot** — startup
  swamps everything smaller, which is why no per-rung number is quoted.
- **Chain in a browser:** `lunar_map.html?chainbench=1` in headless
  Chrome, default GL (ANGLE on the real GPU) and `--use-angle=swiftshader`.
  Warm pass, res 256/512/1024/2048. **Three clocks lied before one told
  the truth**, and this is written down so nobody repeats it:
  1. raylib's `GetTime()` is GLFW's clock, which emscripten advances only
     per frame; the bench runs to completion before the first frame and
     read 0.0 ms for everything.
  2. `emscripten_get_now()` would work, except headless Chrome needs
     `--virtual-time-budget` to load a 48 MB page reliably, and virtual
     time **fakes `performance.now()`** — that is its purpose. Also 0.0.
  3. GL is asynchronous; timing the call measures submission only.
  The numbers come from bracketing each build with `CHAINMARK` log lines
  and differencing **the browser's own log timestamps**, which are real
  wall clock under virtual time. A 1 × 1 `rlReadScreenPixels` after each
  build forces completion.
- **My estimate before measuring was "tens of milliseconds at 2048".
  It is 446 ms.** The error was calling the fused shader "one texture
  fetch per pixel"; §3.3 is what it actually does. Recorded so the next
  estimate is checked against this one.

---

## 9. Open questions and risks

- **Is phase 1's improvement enough?** It adds texture, not landform.
  If the highlands still read as too soft, phase 2 is the answer, not a
  larger `chainStrength` — pushing the high-pass harder brings the
  double-lighting back.
- **Phone memory is unmeasured.** §5.7.1 is designed to make the
  question moot; until it is built and run on a device, treat mobile as
  unknown.
- **The probe is a single sample.** A device under thermal throttling
  or a browser sharing the GPU with a video call may probe well and
  build slowly. Mitigation: re-probe if a build exceeds 3× its budget,
  and drop a tier.
- **Determinism across paths.** The GPU chain hashes its noise; the CPU
  chain draws it from xorshift. Same statistics, different pixels — so
  tier A and tier B show slightly different grain at the same site.
  Acceptable for a texture layer; it would not be for anything the
  verdict touched, which is why the verdict touches none of it.
- **Span-parameterised crops change the chain's contract.** The game's
  callers pass 100/25/5 and must keep getting exactly that. Covered by
  `terrain_probe`'s existing CPU-vs-GPU comparison; add a fixed-seed
  image diff for the 25 km level before and after §5.3.
