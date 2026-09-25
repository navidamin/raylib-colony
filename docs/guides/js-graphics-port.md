# Porting JS graphics to raylib — the protocol

**Use this whenever you are handed a JS graphics model** (a `.js` or `.html`
file that draws with Canvas 2D: `getContext('2d')`, `ctx.fill…`,
`ctx.stroke…`) and asked to bring it into the game. It is the procedure that
got Holo3D to 1.15% and ToolRack to 1.83% against their references, written
so the next port doesn't have to relearn the ~15 traps those two hit.

It sits on three things that already exist. Read them, don't re-derive them:

| What | Where | Role |
|---|---|---|
| The contract | [`docs/CANVAS2D_PORT_SPEC.md`](../CANVAS2D_PORT_SPEC.md) | the twelve Canvas features raylib lacks and the one approved way to do each |
| The shim | [`src/ui/c2d.h`](../../src/ui/c2d.h) / `c2d.c` | every one of those twelve, implemented once. Call sites read like the JS |
| The gate | `tools/visdiff/` + `tools/c2dtest/` | JS reference vs port, pixel diff, heatmap; the shim's own acceptance tests |

This guide adds the **order of work**, the **scanner** that starts the
inventory (`tools/jsport/gapscan.py`), the **harness recipe** for a new
module, and the **traps**.

### Getting the system onto another branch

The whole system (shim, tests, diff harness, fonts, ToolRack as the worked
example, this guide, the scanner and the skill) ships as **one commit** at
the tip of branch `claude/c2d-graphics-kit`, built on `main`. To use it on
another branch:

```bash
git fetch origin claude/c2d-graphics-kit
git cherry-pick origin/claude/c2d-graphics-kit
tools/c2dtest/c2dtest.sh                         # shim tests pass
SS=2 tools/visdiff/visdiff_toolrack.sh           # 1.83%: the kit arrived intact
```

Every file in the kit is new except two, and each of those gets one hunk.
On a branch whose copies are too old to match, the conflict is mechanical:
keep the branch's own file and re-add the kit's hunk.

- `src/CMakeLists.txt`: one line at the top,
  `include("${CMAKE_CURRENT_SOURCE_DIR}/ui/c2d_targets.cmake")`.
- `CLAUDE.md`: the "Porting JS graphics (c2d)" section under the opening
  line.

Tested: clean onto `farming-unit-design`, `graphics-engine-extend` and
`lunarmap-wiring-site-selection`. The hunks need adding by hand on
`develop` and `section-visual-redesign`, which predate the current
`src/CMakeLists.txt`.

The Holo3D port, the dashboard chrome and the survey console that assembles
them are **not** in the kit. They live on
`claude/excavation-module-design-jhp3v1`, where the system was built, and
so do the commits the traps below cite.

> **The one rule.** A port is a 1:1 translation of draw calls, never a
> redesign. If the JS draws a 4-stop gradient on a concave polygon, so does
> the port. A flat fill where the reference has a gradient is a port failure,
> not a simplification. Every earlier failure traces back to skipping this.

### Is this the right protocol?

Yes if the source is **procedural Canvas 2D**. If it's WebGL, three.js, SVG
or DOM/CSS layout, c2d doesn't cover it. Stop and ask the user how to
proceed; don't force it through the shim.

---

## The protocol

Nine steps. Each one produces something and has a gate. Don't start a step
until the previous one's gate holds.

### 0. Intake — land the source, verbatim

- Put the file in **`js/`** exactly as given. **Never edit a reference.**
  If you think it has a bug, the port reproduces the bug and flags it (see
  trap 3).
- **Find the canonical source.** If one file is an extract of another, port
  it once: `holo3d.js` is byte-identical to `dashboard.html` 965–1290, and
  an inventory that counts both double-counts every feature. Check with
  `diff <(sed -n 'A,Bp' big.html) small.js`.
- **Record line ranges** per module when a file holds several
  (`dashboard.html` holds four).
- **Scope by reachability.** List the entry points the page actually calls.
  ToolRack ships two racks and the dashboard only ever calls variant B, so
  variant A was left unported and the scope decision was written into the
  inventory. It wasn't treated as a skipped row.
- **Read the design space** off the canvas size or the module's own scene
  constant (`ToolRack.SCENE = 1536×1024`). This is the port's coordinate
  system, fixed forever.

**Gate:** you can name the source file, its line range, its entry points and
its design size.

### 1. Scan — enumerate every gap site mechanically

```bash
python3 tools/jsport/gapscan.py js/<file> --lines A-B --md
```

It lists every Canvas feature site with resets excluded, finds glow
**helpers** by itself (grepping `shadowBlur` alone missed the whole tool
rack once), maps each feature to its spec section and shim function, and
marks each one `COVERED`, `APPROX`, `FLATTEN` or **`MISSING`**. It exits 2 if
anything is missing. It also lists **determinism hazards** (`Math.random`,
clocks, web fonts) that the diff harness must pin.

The counts are a floor, not a ceiling: an indirection it can't see (a
helper spread over several lines, a style built at runtime) won't show. Read
the source anyway.

**Gate:** the scan runs, and you have read every `MISSING` and hazard line.

### 2. Inventory — account for every row before writing code

Write `docs/design/<module>/<name>-inventory.md` (precedents:
[`toolrack-inventory.md`](../design/prospecting/toolrack-inventory.md),
`holo3d-inventory.md` on the excavation branch). Start
from the scanner's skeleton:

| JS line | Canvas call | Spec § | Shim function | Status | Notes |

- Every scanned row stays. Fill the Notes cell for each one.
- Every **scope decision** is written down with its reason. Nothing is left
  out silently.
- Every **`MISSING`** row names the shim addition that will cover it.
- Anything the port will knowingly render differently is a **known
  deviation**, recorded here and in the port's header comment (precedent:
  `dash_chrome.h`, sans-serif strings drawn mono).

**Show the inventory to the user and wait for approval** unless they have
said to run straight through. The inventory is what makes a port faithful:
the failure is never that the glow couldn't be written, it's that nobody
noticed a glow was there.

**Gate:** zero unaccounted rows. The user has seen it.

### 3. Shim first — close every `MISSING` in c2d, with a test

For each `MISSING` row, extend `c2d.h`/`c2d.c` **before** the port uses it:

- The header is a contract. **Adding** entry points is how it grew; every
  ToolRack gap landed this way (`c2d_rpoly_pts`, nested clips,
  `c2d_gradient_linear_x`, `c2d_text_gradient`,
  `c2d_dashed_polyline_phase`). **Never** add one that lets a call site
  bypass the alpha stack, the transform or the glow path.
- Every addition gets a `tools/c2dtest/c2dtest.c` assertion **measured
  against Canvas's actual behaviour**, not against your expectation of it
  (trap 7 was invisible to a symmetric test).
- Document the new function in the header the way the others are
  documented: which Canvas call it replaces, and why raylib's obvious
  equivalent is wrong.
- If the gap is new to the spec, add a subsection to
  `CANVAS2D_PORT_SPEC.md` §2 and a row to the scanner's `FEATURES` table,
  so the next scan reports it `COVERED`.

Then prove you broke nothing:

```bash
tools/c2dtest/c2dtest.sh                    # all pass
for d in tools/visdiff/visdiff*.sh; do SS=2 "$d"; done   # every port unchanged
```

The existing ports' diffs should be **identical** either side of a shim
change. 9fe7e11 checked pixel counts to the digit.

**Gate:** c2dtest green, every existing visdiff unchanged, no `MISSING` rows
left.

### 4. Harness before port — make the reference render first

Build the diff for the new module **before** the port, so the first render
of the port is already measured. Copy the ToolRack set, which is the
cleanest template:

| New file | Copy from | Change |
|---|---|---|
| `tools/visdiff/ref_<name>.html` | `ref_toolrack.html` | the `<script src>`, canvas size, the fixed state and the entry-point call |
| `tools/visdiff/shoot_<name>.js` | `shoot_toolrack.js` | the page filename and the viewport |
| `tools/visdiff/<name>_main.c` | `toolrack_main.c` | the include, the surface size, the fixed state, the draw call |
| `tools/visdiff/visdiff_<name>.sh` | `visdiff_toolrack.sh` | target name, args, default `TAG` |
| CMake target `<name>_visdiff` | `toolrack_visdiff`, next to `colony_c2d` | the source file |

The reference page's rules are non-negotiable, and each one exists because
its absence once invalidated a run:

- **Local `@font-face`** against the exact TTFs in `src/assets/fonts/`, and
  set `window.__fontError` if any face fails. A blocked web font falls back
  **silently**, and the diff then measures one typeface against another
  (trap 2).
- **Fixed state.** Pin `time`, rotation, selection and anything that pulses
  on `sin(t)`. Force `dpr = 1`.
- **Nondeterminism off or seeded on both sides.** `makeGrain` uses
  `Math.random()`, so grain is off in the rack diff and checked separately.
- **`window.__ready = true`** after the draw. The shooter waits on it.

Screenshot the reference alone and **look at it** before going on.

**Gate:** `ref_<name>.png` exists and looks like the JS in a browser.

### 5. Port — translate, don't reinterpret

Write `src/ui/<name>.c` + `<name>.h`, and add the `.c` to the
`colony_c2d` library. That's `src/ui/c2d_targets.cmake` where the kit was
cherry-picked, and `src/CMakeLists.txt` on the branch the system was built
on. `grep -rn "add_library(colony_c2d" src` finds it.

- **C99**, `extern "C"` guards in the header, project naming conventions.
  The header comment cites the source file and line range, links the
  inventory, lists any known deviations, and shows a 3-line usage example
  (see `holo3d.h`, `toolrack.h`).
- **All drawing through `c2d.h`.** No raylib draw call in a UI module.
- **Coordinates verbatim.** `bx + bw - 12 * s` stays exactly that. Nothing
  is re-derived from window size. Hit tests take **design-space** points.
- **Keep the structure recognisable.** One C function per JS function, same
  names in TitleCase, same order, `/* dashboard.html:NNN */` at
  non-obvious sites. Someone holding both files should be able to follow
  along.
- **Port the code, not the intent** (trap 3). Where you believe the JS is
  wrong, match it, flag it in the inventory, and make changing it a
  one-liner.
- Keep the JS's performance paths (`state.fast` skips glow during drag
  rotate) and its caches (static chrome → `c2d_cache_*`).
- **Preallocate.** The JS reallocates point arrays per frame; C gets
  fixed-size scratch buffers sized from the model's known ceiling.
- Choose the glow form per site. Use **`c2d_glow_*`** for an isolated shape.
  Use **`c2d_shadow_begin/_end`** wherever several shapes share one
  `glowOn`, or the path is dashed (trap 8).

**Gate:** it builds and draws something.

### 6. Diff gate — under 2%, read the heatmap before tuning

```bash
SS=2 tools/visdiff/visdiff_<name>.sh    # prints % and writes heat_<tag>.png
```

`SS=2` is what the console ships with, and the gate is measured in the same
configuration. **Target: under 2%.** Above it, open the heatmap and match it
against the signatures below **before** you change any code:

| Heatmap shows | Cause | Spec |
|---|---|---|
| large soft regions around strokes | a glow site dropped, or glow the wrong shape (stroke where the JS glows a fill) | 2.3 |
| halos solid between dashes / beaded dashes | per-shape glow on a shared or dashed glow: use the layer blur | 2.3 |
| hard banding inside a filled shape | gradient became flat per triangle, or a triangle fan on a concave polygon | 2.1 / 2.2 |
| wedge artifacts across a fill | triangle fan instead of ear clipping | 2.1 |
| uniform vertical offset on all text | baseline not converted from alphabetic | 2.9 |
| every string ~24% too small / narrow | raylib size treated as the em square (fixed in c2d; don't bypass `c2d_text`) | 2.9 |
| overlapping shapes darken | per-shape alpha where the JS composites a group once | 2.6 |
| flat rectangles where the reference glows softly | a radial gradient dropped | 2.10 |
| a closed curve where the reference is open | partial `ellipse` arc drawn full-sweep | 2.11 |
| one side of a closed outline missing | open-path glow on a closed shape: `c2d_glow_polygon` | 2.3 |
| every dash exactly in the reference's gaps | `lineDashOffset` sign | 2.7 |
| content leaking outside a panel | a clip dropped or approximated with a scissor rect | 2.4 |
| a diff that doesn't move whatever you change | **stale binary.** The build failed; read its output | — |

Measure the diff after **every** fix and keep a running log in the commit
message (ToolRack went 8.11 → 3.04 → 1.83, and each step says what fixed
it).

Also render the port with `fast = true` and at a second fixed state
(different rotation / selection / level) so the gate doesn't pass at one
cherry-picked frame only.

**Gate:** under 2% at `SS=2`, and every inventory row is either ported or
listed as a known deviation.

### 7. Wire into the game — reachable, not just diffable

A module the diff harness can see but the player can't reach is not
finished (see [`feature-completeness.md`](feature-completeness.md)).

- Render into a `C2DSurface` at design size and put it on screen with
  **`c2d_present_into(surface, rect)`** for a region, or `c2d_present` for
  the whole window. Presenting against the window and then scissoring
  crops the design space instead of fitting it. That cut the top off the
  rack the first time.
- Input: convert with **`c2d_to_design`** before any hit test. Take
  screen points at the module's API and convert inside. `SurveyDash_*` in
  `src/ui/survey_dash.c` and its caller in `rendermanager.cpp` are the
  working pattern, on the branch that has the console.
- Obey the display-scale rules in CLAUDE.md (no raw
  `GetScreenWidth`/`BeginTextureMode` etc. in game code).
- **Web memory.** Every offscreen target is multiplied by the supersample
  factor squared. Run supersample 1 under `__EMSCRIPTEN__` (the survey
  console's `DASH_SS` does). At 2, seven design-sized targets came to
  126 MB and the browser refused them. A new module that adds design-sized
  layers must check what it adds to the tab's budget.
- Render it in context with `tools/preview/preview.sh` and **look at the
  PNG**. Then build every target.

**Gate:** a player can reach it, and you've looked at it in the game, not
just in the harness.

### 8. Record — leave the next port better off

- Add a row to the **port registry** below.
- Mark the inventory's status and the final diff figure.
- If a new trap cost you time, add it to the traps list with its commit.
- Commit message: the diff numbers before and after, and each defect found
  with its cause. The existing port commits (`0f36ae8`, `9fe7e11`,
  `c24cd7b`, `16bf24b`) are the model.

---

## Traps

Each one cost a real session. They're all fixed in the shim or the harness
now. The list is here so nobody reintroduces one by going around them. The
cited commits are on `claude/excavation-module-design-jhp3v1`.

1. **The em square** (c24cd7b). Canvas `font: 24px` sets the *em* to 24px;
   raylib's size sets ascent−descent. For JetBrains Mono that's a factor of
   1.32, so every string was 24% small. First misdiagnosed as a hinting and
   gamma problem, and a gamma sweep moved the diff by 0.02pp. `c2d_text`
   reads `unitsPerEm` from the font and corrects it. Never draw text around
   it.
2. **Silent font fallback** (0f36ae8). The proxy blocks Google Fonts, and a
   blocked web font doesn't fail. Chromium drew the reference in DejaVu,
   and it looked like a port bug. Hence local `@font-face` plus
   `__fontError` in every reference page.
3. **The code, not the intent** (0f36ae8). `path()` begins with
   `beginPath()`, which discards the preceding `rect(-1e5…)`, so an
   even-odd clip that *looks* like "everywhere except the hull" is the bare
   hull. The reference draws what the code says.
4. **Glow behind helpers** (spec 2.3). `glowOn(ctx, c, blur)` is a state
   toggle. 15 of the dashboard's 26 glow sites are helper calls. The
   scanner finds helpers, and grep doesn't.
5. **Resets are not features.** `shadowBlur = 0`, `setLineDash([])`,
   `globalAlpha = 1` inflate the count and hide what was skipped.
6. **Open vs closed strokes** (c24cd7b). `ctx.stroke()` on a closed path
   strokes the closing segment. An open-path glow dropped the whole left
   edge of a rounded rect built corner-first → `c2d_glow_polygon`.
7. **`lineDashOffset` sign** (c24cd7b). Canvas subtracts it. Wrong sign puts
   every dash precisely in the reference's gaps, which a symmetric unit
   test can't see.
8. **Glow is Gaussian and source-over, and shared glows blur once**
   (0f36ae8, c24cd7b, 16bf24b). Canvas shadows fall off with
   σ = blur/2, overlap as 1−(1−a)(1−b), and blur everything under one
   shadow setting *once*. Additive stacking read 0.81 between two dashes
   where the reference reads 0.16.
9. **`BLEND_ADDITIVE` squares alpha** (16bf24b). `(SRC_ALPHA, ONE)` gives
   dst.a += src.a², so six blur passes took a solid shadow to 6e-5.
10. **`BeginTextureMode` doesn't nest** (16bf24b). Running a blur while a
    layer was still bound silently dropped everything drawn in it.
11. **Bilinear minification undersamples** (16bf24b). Halve in steps. A
    single 1/7 downsample loses thin strokes.
12. **Stale binary** (c24cd7b). A build failure piped to `/dev/null` meant
    four "different" experiments measured the same binary. The harness
    scripts now fail loudly with exit 4. Keep it that way in new scripts.
13. **A plain clip that never clipped** (9fe7e11). Multiplying layer alpha
    by a mask emits no fragments outside the mask, so nothing was erased.
    Every new c2d operation gets a test that checks the *outside*.
14. **Render target id 0 on the web** (8b25606). A failed
    `LoadRenderTexture` returns id 0, and binding it draws onto the page at
    the wrong scale with no error. c2d refuses to bind id 0; keep new
    offscreen paths behind the same guard.
15. **`EndTextureMode` resets the matrix mode** (2876bf3). It leaves rlgl
    pointing at the modelview while a caller's transform is pushed, so the
    transform compounds every frame. `c2d_unbind` re-seats it; don't bind
    targets outside c2d.
16. **JS numbers are float64** (port-audit). `hash()` multiplies past 2^53.
    uint32 matches it to ~6e-8, which is fine. What matters is that no
    sample straddles a threshold the JS tests against, so test that.

---

## Port registry

"Where" is the branch that carries the port: **kit** means it's in the
`claude/c2d-graphics-kit` commit (and so on every branch that picked it),
**excavation** means `claude/excavation-module-design-jhp3v1` only.

| Module | Source | C port | Inventory | Harness | Diff (SS=2) | Where |
|---|---|---|---|---|---|---|
| ToolRack (variant B) | `js/dashboard.html` 21–712 (= `js/toolrack.js`) | `src/ui/toolrack.c` | [toolrack-inventory](../design/prospecting/toolrack-inventory.md) | `visdiff_toolrack.sh` | 1.83% | kit |
| Holo3D | `js/dashboard.html` 965–1290 (= `js/holo3d.js`) | `src/ui/holo3d.c` | `docs/design/prospecting/holo3d-inventory.md` | `visdiff.sh` | 1.15% | excavation |
| Dashboard chrome (subset) | `js/dashboard.html` 1290–1670 | `src/ui/dash_chrome.c` | — (header lists scope + deviation) | none | not gated | excavation |
| HoloBlock | `js/dashboard.html` 717–964 | not ported | — | — | scan flags 1 `MISSING`: diagonal gradient, line 800 | — |
| layers-block | `js/layers-block.html` (1600×1300) | not ported | — | — | — | — |

On the excavation branch, `survey_dash.c` assembles the ported modules into
the console using the dashboard's own `DASH.layout`. `drill_sim.c` and
`dash_knowledge.c` there are logic ports (no drawing), which this protocol
doesn't cover.
