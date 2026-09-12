# Prompts for Claude Code

Two prompts. Run phase 1 once. Run phase 2 for every module, forever.

---

## Phase 1 — build the shim (once)

> Read `docs/CANVAS2D_PORT_SPEC.md` and `src/ui/c2d.h`.
>
> Implement `src/ui/c2d.c` against that header. The header is a contract:
> do not change its signatures, do not add functions that let call sites
> bypass the alpha stack or the glow path.
>
> Work in this order and do not start a step before the previous one has
> a passing test:
>
> 1. Surface + present + `c2d_to_design`. Test: a 1536×1024 surface with
>    a marker rect at each corner blits correctly letterboxed at window
>    sizes 800×600, 1920×1080 and 1024×1366, and `c2d_to_design` round
>    trips each corner to within 1px. **The RenderTexture is Y-flipped —
>    source height must be negative.**
> 2. Alpha stack + `c2d_tint`. Test: nested pushes multiply
>    (0.5 then 0.6 gives 0.3), pop restores exactly.
> 3. `c2d_fill_poly` via ear clipping. Test it against a deliberately
>    concave polygon: a 20-point sawtooth strip like the layer walls.
>    Assert no triangle has a vertex outside the input polygon. A
>    triangle fan will fail this — that is the point of the test.
> 4. `c2d_fill_poly_gradient`. Per-vertex colour from
>    `c2d_gradient_at(v.y)`, emitted through `rlBegin(RL_TRIANGLES)` with
>    `rlColor4ub`. Test: a tall quad with 4 stops shows smooth vertical
>    interpolation with no per-triangle banding.
> 5. `c2d_polyline` with round joins, then `c2d_glow_stroke`. Follow the
>    pass structure in spec 2.3 exactly — two additive halo passes, then
>    a normal-blend core pass.
> 6. Text. This is the one most likely to be subtly wrong. Load the three
>    weights as separate fonts at 64px with bilinear filtering. Implement
>    `c2d_ascender` from the font's actual metrics, not an estimate.
>    Test: draw the same string with `C2D_BASELINE_ALPHABETIC` at y=100
>    in raylib and in a headless Canvas at y=100, and assert the glyph
>    bounding boxes match within 1px vertically. Do not skip this test.
> 7. `c2d_fill_radial` — rlgl triangle fan, inner RING at `r_in` (not a
>    single centre vertex; all six JS sites have `r_in > 0`), outer rim
>    at `r_out`, 48+ segments. Test: the rack slot light at
>    `(cx, cy, 6, 50)` shows smooth falloff with no visible polygon rim.
> 8. `c2d_ellipse_pts` — must honour partial arcs (`a0..a1`) and
>    rotation. Two of the five JS sites are partial; a full-sweep
>    implementation silently closes them. Test one partial arc.
> 9. Everything else in header order.
>
> `c2d_hash` must match the JS `hash()` bit for bit. Use `uint32_t`
> throughout and mirror the `>>>` unsigned shifts. Write a test that
> checks 20 known (a,b,c) triples against values you compute from the JS.
>
> Do not port any game module in this session. Shim only.

---

## Phase 2 — port one module (repeat per module)

Fill the four bracketed slots.

> Port `[js/holo3d.js]` to raylib as `[src/ui/holo3d.c]` + header.
>
> Read first, in this order:
> - `docs/CANVAS2D_PORT_SPEC.md` — the porting contract
> - `src/ui/c2d.h` — the shim you must build on
> - `[js/dashboard.html lines 965-1290]` — the reference implementation
> - `[src/ui/toolrack.c]` — an already-ported module, for house style
>
> `dashboard.html` is the canonical source. `holo3d.js` is byte-identical
> to lines 965–1290 of it — the same module extracted, not a second
> implementation. Do not port both and do not diff them for guidance.
>
> **This is a translation, not a redesign.** The JS is procedural Canvas
> 2D; every pixel comes from a draw call. Reproduce every draw call.
> Do not simplify, do not "adapt for raylib," do not substitute a flat
> fill for a gradient or a plain line for a glow. If something seems
> hard, it is in the spec's list of twelve gaps with an approved
> implementation — use that.
>
> Design space is `[1536×1024]`. Every coordinate in the JS ports over
> verbatim. Do not re-derive layout from window size.
>
> Before you write any code, produce an inventory as a markdown table and
> show it to me:
>
> | JS line | Canvas call | Spec § | Shim function | Notes |
>
> One row per `createLinearGradient`, `createRadialGradient`,
> `shadowBlur`, `glowOn`, `globalAlpha`, `clip`, `setLineDash`,
> `ellipse`, `measureText`, `textAlign`, `textBaseline`, `drawImage`,
> and offscreen-buffer site.
>
> Two grep traps, both of which will make your inventory look complete
> when it isn't:
>
> - **Glow is mostly behind a helper.** `ToolRack` calls
>   `glowOn(ctx, color, blur)` / `glowOff(ctx)`. Grepping `shadowBlur`
>   alone finds 11 of the 27 glow sites and misses the entire rack.
>   Grep both.
> - **Exclude resets.** `shadowBlur = 0`, `setLineDash([])` and
>   `globalAlpha = 1` are teardown, not features. There are 16, 10 and 9
>   of them respectively. Counting them inflates the inventory and hides
>   what you skipped.
>
> Check your totals against the verified table in spec §3.5. For this
> module the expected counts are in its column. If you come back short,
> grep again — do not proceed.
>
> Then implement. Then run the visual diff harness
> (`[tools/visdiff.sh holo3d]`) and report the differing-pixel
> percentage. Target is under 2%. If you are above it, read the heatmap
> before changing code:
> - large soft regions → a `shadowBlur` site was dropped
> - hard banding inside a bed → gradient became a flat fill, or the fan
>   bug in 2.1
> - uniform vertical text offset → baseline not converted from
>   alphabetic
> - beds darkening where they overlap → ghost compositing done per-bed
>   instead of per-group (spec 2.6)
> - rack slots reading as flat rectangles → a radial gradient was
>   dropped (spec 2.10)
> - a closed shape where the reference has an open curve → a partial
>   `ellipse` arc was drawn full-sweep (spec 2.11)
>
> Do not tell me it is done until the diff passes and the inventory table
> is fully accounted for. If a row cannot be ported, say so explicitly
> rather than silently approximating it.

---

## A note on the inventory step

The inventory table is the part that actually fixes your problem. The
failure mode you are hitting is not that Claude Code can't write the
glow — it's that it never registers that a glow was there. Forcing an
explicit enumeration before any code is written converts a perception
problem into a checklist problem.

Ask for it every time. Read it before approving.

---

## Model and effort

Claude Code separates two dials: the **model** is the fixed weights —
how much it knows. The **effort level** is how much work it does per
turn: how long it thinks, how many files it reads, whether it re-checks
its work. Raise effort when it got something wrong by skipping a step;
change model when it had everything it needed and still got it wrong.

| Work | Model | Effort |
|---|---|---|
| Phase 1 — shim (`c2d.c`) | Opus 5 | xhigh |
| Phase 2 — first two module ports | Opus 5 | high |
| Phase 2 — later ports, once the pattern is set | Sonnet 5 | high (its default) |
| Diff-failure fixes, mechanical edits | Sonnet 5 | medium |
| Boilerplate, headers, build files | Haiku 4.5 | — |

Reasoning:

- The shim is the only genuinely novel work — ear clipping, gradient
  interpolation through `rlgl`, stencil clipping, the RenderTexture Y
  flip, font metrics. It's also the piece everything else inherits from,
  so a subtle bug there degrades every module you ever port. This is
  where to spend.
- Once `c2d.h` exists and two modules are done, porting is pattern
  matching against an existing example. Sonnet 5 defaults to high effort
  in Claude Code and handles this well.
- Anthropic's guidance for Opus 5 and Fable 5.1 is to start at the
  default (high) and adjust from your own results rather than reaching
  for the top of the scale by reflex. Move up to xhigh when you have
  evidence a task needs it — the shim is that case.
- You can type `ultrathink` anywhere in a prompt for deeper reasoning on
  a single turn without changing the session's effort setting. Useful
  for the inventory step specifically.
- Set effort with `/effort` or `--effort`; the current level shows in the
  session header next to the model name.

Sources: [Model configuration](https://code.claude.com/docs/en/model-config),
[Choosing the right model](https://platform.claude.com/docs/en/about-claude/models/choosing-a-model),
[Choosing a Claude model and effort level in Claude Code](https://claude.com/blog/claude-model-and-effort-level-in-claude-code).

---

## Put this in CLAUDE.md

So you stop repeating it:

```md
## UI porting

UI modules are ported from procedural Canvas 2D JS in `js/`. This is a
1:1 API translation, never a redesign.

- Read `docs/CANVAS2D_PORT_SPEC.md` before touching anything in `src/ui/`.
- All drawing goes through `src/ui/c2d.h`. Do not call raylib draw
  functions directly from a UI module.
- Design space is fixed per module. Never re-derive layout from window
  size. Never hit-test in screen space.
- Before porting, produce the gap inventory table required by the spec.
- A port is not done until the visual diff is under 2%.
```
