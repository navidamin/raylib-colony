# Prospecting — The Block Model

> Status: **DESIGN** — not built
> Last Updated: 2026-08-25
> Parent: [README.md](README.md)
> Requires: [subsurface/subsurface-model.md](../subsurface/subsurface-model.md)
> Pairs with: [subsurface/module-interplay.md](../subsurface/module-interplay.md)
>
> 2026-08-26 coherence pass: the LAB stage is **retired and shipped so** (a
> recovered core comes out of the ground assayed); GPR is **dropped** (LIBS is
> the one wide instrument); depth and prospecting reach are **ungated**; the
> tier arc is replaced by [progression-design.md](progression-design.md)
> (rigs + techniques). Sections below are edited to match.
>
> **Supersedes** the panel and interaction design in
> [`prospecting-master-design.md`](prospecting-master-design.md) and
> [`ui-layout.md`](ui-layout.md). It does **not** supersede
> [`sampling-mechanics.md`](sampling-mechanics.md),
> [`confidence-system.md`](confidence-system.md) or
> [`depth-sampling-design.md`](depth-sampling-design.md) — those describe the
> instruments, and the instruments survive. What changes is what they are
> pointed at and what they produce.

---

## 1. The Frame

Today prospecting asks *"which of these 64 squares do you want to look at?"*
The player clicks squares until a progress bar fills. Every square is
independent, so there is nothing to reason about — only coverage to grind out.

This design asks a different question:

> **There is an ore body down there with a shape. Where is it, which way does
> it go, and how sure are you?**

That question has structure. It can be reasoned about, guessed at wrongly, and
learned. It also has a natural stopping point — the moment you are sure enough
to commit — which is the decision the whole extraction chain hangs on.

### What the player is looking at

Not a grid of squares. A **block model**: four depth layers of the sub-cell
lattice, drawn as stacked isometric surfaces, all four visible at once. The
lattice stays fixed-resolution (never resized by progression); the code is
8 × 8 today and the **target is 16 × 16** (6.25 m blocks) — decided against
the interactive mock-up, where 8 × 8 read as data resolution rather than
ground. Note the switch invalidates every number measured on the 8 × 8
lattice (`colony_measure_clusters`, the shaft footprint): re-run, re-derive.

```
              ╱▔▔▔▔▔▔▔▔▔▔▔▔╲
   0 m ──── ╱   ▂▄█▄▂        ╲        surface: a low swell, mostly guessed
            ╲▁▁▁▁▁▁▁▁▁▁▁▁▁▁╱
                 ┊   ┊
              ╱▔▔┊▔▔▔┊▔▔▔▔▔╲
  12 m ──── ╱    ▄███▄        ╲       shallow: the shoot is showing
            ╲▁▁▁▁┊▁▁▁┊▁▁▁▁▁▁╱
                 ┊   ┊  ← drill traces, drawn through the layers
              ╱▔▔┊▔▔▔┊▔▔▔▔▔╲
  34 m ──── ╱   ▄█████▄       ╲       mid: the thickest part
            ╲▁▁▁▁▁▁▁▁▁▁▁▁▁▁╱
                     ┊
              ╱▔▔▔▔▔▔┊▔▔▔▔▔╲
  68 m ──── ╱     ▂▄█▄        ╲       deep: one hole reached it, mostly a bet
            ╲▁▁▁▁▁▁▁▁▁▁▁▁▁▁╱
```

Three channels carry three variables, and none of them fight:

| Channel | Variable | Reads as |
|---------|----------|----------|
| **Height** of the surface | grade | how good |
| **Colour** of the surface | class (Measured / Indicated / Inferred) | how sure |
| **Position** in the stack | depth | how hard to reach |

Height for grade is the one that makes it legible at a glance: an ore body
becomes a hill, and a hill has an obvious shape, peak and extent. A shaded grid
of 64 squares does not.

> Prototyped and compared against four alternatives before settling on this:
> grade-as-opacity, pixelation-as-confidence, class-as-outline, and hue-rotation.
> Height-for-grade with colour-for-class was the only pairing where both
> variables stayed readable at once.

---

## 2. The Loop

Two stages, one destination. (An earlier draft kept a third, the LAB — it is
retired, and the retirement has shipped: a recovered core is rock you are
holding, so it comes out of the ground assayed. Analytical precision is a
percent or two; the uncertainty *between* holes is total. Gating the assay
modelled the small uncertainty and made the drill look like it might not tell
you what you had just pulled out.)

```
  ┌──────────┐        ┌─────────┐        ┌─────────────┐
  │  SWEEP   │ ─────▶ │  DRILL  │ ─────▶ │ BLOCK MODEL │
  └──────────┘        └─────────┘        └─────────────┘
   LIBS, surface       a hole along       grades interpolated
   chemistry only,     a chosen line —    between assays, with
   sets the PRIOR,     core is CERTAIN    confidence falling off
   never classifies    along the trace    with distance from the
        │                                 nearest core
        └────── tells you where drilling is worth it ──────┘
```

| Stage | Was | Is now |
|-------|-----|--------|
| **Sweep** | GPR bands raising per-cell confidence | **LIBS only** — surface chemistry, element by element, blind below the regolith. Sets the prior and is hard-capped below classification: it may make ground look interesting, never make it count. GPR is dropped — it reads structure, not composition, so it had no per-element opinion to lend a grade model (revisit only if a structure-reading role appears once the ground has 3D orientation) |
| **Drill** | a sample at one spot + one depth | **a hole along a line** — collar and target, so azimuth, dip and length are derived. Returns core at intervals down its whole trace, and the core is certain |
| **Model** | *did not exist* | grades interpolated between assays, with confidence from the **nearest** core |

Nothing built is wasted: the sweep engine keeps its job as the prior, the
sampling engine extends from points to lines. The lab engine is orphaned —
retained in the tree, called by nothing, delete when convenient.

### The recurring decision

Every hole is the same four questions, and none has a dominant answer:

1. **Where to collar it** — over the peak you already know, or out in the blank?
2. **Which way to point it** — down the column, or across the interpreted shoot?
3. **How far to push it** — energy and time are per metre, and the deep layers
   are where the tonnage is
4. **Whether to stop** — every hole is tonnage you did not mine yet

That last one is the game. Everything else is in service of it.

---

## 3. The Estimate Field

The player never sees ground truth. They see an **estimate**, and how much to
trust it.

### Grade

Inverse-distance weighting from every assay, pulled toward the prior where
there is no evidence:

```
estimate(p) = lerp( prior(p), Σ wᵢ·gᵢ / Σ wᵢ , support(p) )
              where wᵢ = 1 / dᵢ³
```

### Confidence

Confidence comes from the **nearest** assay, not from the weight sum:

```
support(p) = exp( -(d_nearest / RANGE)² )
```

> **This is the one formula to get right, and it is easy to get wrong.** Using
> the sum of weights instead lets a hundred distant samples out-vote the prior,
> so a barren area surrounded by rich holes reads as rich *and* confident. It
> was measured going the wrong way in the prototype — model error rose from
> 0.232 to 0.270 after adding a third drill hole, because high grades leaked
> across ground nobody had touched. Nearest-sample support fixed it. Any
> reimplementation needs a test that adding a hole never increases error.

`RANGE` is the geological continuity distance — how far one assay can honestly
speak for. Combined with the class thresholds it yields a drill spacing per
class, which is the number a player can act on:

| Class | Support | Drill spacing that achieves it | What the player does |
|-------|---------|-------------------------------|---------------------|
| **Measured** | > 0.80 | ~10 m — tighter than one sub-cell | commit; point the precise machine here |
| **Indicated** | 0.40 – 0.80 | ~17 m | worth digging, not worth a shaft |
| **Inferred** | 0.20 – 0.40 | ~27 m | a bet |
| **Unclassified** | ≤ 0.20 | unsampled | you know only the prior |

Thresholds are the ones already in the code (`CONFIDENCE_THRESHOLD_*`), and
`ResourceClass` already groups them — see
[`implementation-plan.md`](implementation-plan.md). This design changes what
feeds confidence, not how it is banded.

**Measured needs tighter spacing than a sub-cell is wide.** That is deliberate:
you cannot get a block to Measured with one hole through it. You need two, or a
hole plus a mined face. Certainty is expensive, which is what makes Indicated
the honest working state for most of a mine.

---

## 4. Drilling

### A hole

```
struct DrillHole
{
    Vec2  collar;        // where it starts, in parent-cell metres
    float azimuthDeg;    // compass bearing
    float dipDeg;        // 0 = horizontal, 90 = straight down
    float metres;        // how far
    std::vector<Assay> assays;   // one per interval, filled on recovery
};
```

Vertical drilling is the degenerate case: `dip = 90`, azimuth irrelevant,
`metres` = one layer. **That is exactly what the game does today**, which is
what makes this an extension rather than a rewrite.

### Planning one, before paying for it

The single most important piece of UI in the design: **the projected trace is
drawn through the block model as you aim it, before you commit.**

```
   ╱▔▔▔▔▔▔▔▔▔▔▔▔▔▔╲          azimuth  ◀ 124° ▶
  ╱  ▂▄█▄▂     ╲   ╲         dip      ◀  54° ▶
  ╲▁▁▁▁▁╲▁▁▁▁▁▁▁▁▁╱          length   ◀ 115 m ▶
         ╲
   ╱▔▔▔▔▔▔╲▔▔▔▔▔▔▔╲          ── projected ──────────────
  ╱   ▄███▄ ╲      ╲          passes:  3 layers
  ╲▁▁▁▁▁▁▁▁▁╲▁▁▁▁▁╱           blocks:  9
             ╲                cost:    690 E · 5.8 days
   ╱▔▔▔▔▔▔▔▔▔▔╲▔▔▔▔╲          would newly classify:
  ╱   ▄█████▄  ╲    ╲           4 → Indicated
  ╲▁▁▁▁▁▁▁▁▁▁▁▁╲▁▁▁╱            2 → Measured
```

The readout answers *"what will this hole buy me?"* in the same currency as the
resource statement — blocks moved up a class. That turns aiming from a
guess into a comparison, without telling the player what the assays will say.

### Progression

There is no tier. What a hole can be is decided by the **rig** that drills it
and the **techniques** the colony has learned — a stable of instruments and a
short chain of knowledge, specified in
[progression-design.md](progression-design.md). The short form:

| Axis | Owned by | The arc |
|------|----------|---------|
| RECOVERY — what comes back up | rigs | auger → RC chips (fast, ±20%, caps at Indicated) → diamond core (certain) → composite holes |
| AIM — where you may point it | rig × technique | vertical → dip detents → continuous azimuth/dip → daughter holes off a wedge |
| CONTINUITY — how far a core speaks | techniques | isotropic → corroborated pairs → declared strike → per-element anisotropy |

Depth appears nowhere in that table, and neither does reach: **all four
layers and all 64 collars are open from the first minute.** Depth stays
meaningful because it is priced per metre (never discounted), thin by
geometry (cross-layer support ~0.49/0.14/0.01), long to reach obliquely, and
expensive to corroborate — see progression-design.md §5.

### Cost

Per metre, not per hole. Energy per metre rises with depth (harder rock,
more lifting); time per metre likewise.

```
cost = Σ over the trace of  ENERGY_PER_METRE[layer]  ×  metres in that layer
```

So a long angled hole in the shallow layers is cheap per metre and covers
ground laterally; a deep vertical hole is expensive and narrow. Neither
dominates. **All constants to be calibrated against dumped data**, per the
repo's rule — no numbers invented here.

---

## 5. The Panel

```
┌────────────────────────────────────────────────────────────────┐
│  PROSPECTING                                    TIER 2 · 6×6   │
├──────────────────────────────┬─────────────────────────────────┤
│  ⬡ Fe   ⬡ Ti   ⬡ Si   ⬡ H₂O  │  RESOURCE STATEMENT · IRON      │
│  ▲ element switcher          │  ■ Measured    0.23 Mt   36.8%  │
│                              │  ■ Indicated   0.35 Mt   31.3%  │
│      ╱▔▔▔▔▔▔▔▔▔▔╲            │  ■ Inferred    0.45 Mt   24.9%  │
│     ╱  ▂▄█▄▂     ╲     0 m   │  ─────────────────────────────  │
│     ╲▁▁▁▁▁▁▁▁▁▁▁╱            │  Committable   0.58 Mt          │
│      ╱▔▔▔▔▔▔▔▔▔▔╲            │  locked in Inferred  0.45 Mt    │
│     ╱   ▄███▄    ╲    12 m   ├─────────────────────────────────┤
│     ╲▁▁▁▁▁▁▁▁▁▁▁╱            │  PLAN A HOLE                    │
│      ╱▔▔▔▔▔▔▔▔▔▔╲            │  collar  D4                     │
│     ╱  ▄█████▄   ╲    34 m   │  azimuth ◀ 124° ▶               │
│     ╲▁▁▁▁▁▁▁▁▁▁▁╱            │  dip     ◀  54° ▶               │
│      ╱▔▔▔▔▔▔▔▔▔▔╲            │  length  ◀ 115 m ▶              │
│     ╱    ▂▄█▄    ╲    68 m   │  690 E · 5.8 d · 9 blocks       │
│     ╲▁▁▁▁▁▁▁▁▁▁▁╱            │  ┌───────────────────────────┐  │
│                              │  │        DRILL              │  │
│  ■ Measured ■ Indicated      │  └───────────────────────────┘  │
│  ■ Inferred  height = grade  │  LIBS SWEEP  ·  AUTO            │
└──────────────────────────────┴─────────────────────────────────┘
```

**The element switcher** sits above the model. Each icon is sized by that
element's total resource and carries a three-segment ring showing its
Measured / Indicated / Inferred split — a summary in exactly the colours the
model below uses, so a glance at the bar says which elements are drilled out
and which are still guesses.

> Icons are **not** faded by confidence. Amount and confidence correlate, so
> size-plus-fade would make a poorly-drilled element small *and* faint at once
> — it would vanish exactly when it most needs noticing. Size carries
> magnitude, the ring carries quality, and neither can hide the other.

**Colour stays on class, not element.** The element is named by the icon, the
panel heading, the tinted rock wall, and the shape of its relief — none of
which cost a per-cell channel. Class geometry is identical for every element
(one core is assayed for all of them), so repeating it on the surface would buy
nothing, while the class colours are the ones the player acts on.

> Also settled by rendering it: the old sweep-heat ramp ran navy → cyan →
> **green** → magenta, so green meant both "strong signal" and "Measured". Two
> variables, one channel. Signal is now a single-hue intensity ramp; class is
> hue. Already fixed in the current panel.

---

## 6. Multi-Scale Control

Every stage keeps its AUTO mode, per the module's existing principle.

| Mode | Behaviour | Cost |
|------|-----------|------|
| **AUTO** | drills a standard vertical grid pattern, widest-gap-first | works unattended; never sites an angled hole, so it plateaus at Indicated |
| **HINTED** | proposes the next hole; the player accepts or re-aims | small efficiency penalty vs. hand-siting |
| **HANDS-ON** | full control of collar, azimuth, dip, length | best results, most attention |

The ceiling is the honest part: AUTO can reach Indicated over the whole
lattice and will never reach Measured on a dipping shoot, because it cannot
aim.
A player who ignores prospecting still plays a complete game — just never a
confident one.

---

## 7. What Carries Over, and What Is Retired

| Existing | Fate |
|----------|------|
| `SweepEngine` | **Kept, single-mode.** LIBS only; its output is the prior, hard-capped below classification. The band arrays die with GPR |
| `SamplingEngine` | **Extended.** A vertical one-layer sample is a hole with dip 90 and length one layer |
| `LabEngine` | **Retired.** Cores come out assayed; the engine is orphaned in the tree, delete when convenient |
| Sample tray, crystal visuals | **Kept as specimens only.** Assay records move onto `SubCell` permanently — today evicting a core deletes the ground it classified, which makes the tray a cap on knowledge |
| `SurveyProgressEngine` | **Kept.** Still the single "how surveyed is this cell" number the extraction pipeline multiplies by |
| `ResourceClass`, `GetDepthConfidence`, `GetClassSplit` | **Kept.** Built already; the block model is their first real consumer |
| Flat per-depth-tab 2D grid | **Retired**, replaced by the four-layer model |
| Per-cell independent confidence | **Retired**, replaced by the interpolated field |
| `SUBCELL_*` cluster generation per layer | **Retired**, see [subsurface-model.md](../subsurface/subsurface-model.md) §4 |
| Every `*_PER_TIER` constant | **Retired**, disposition table in [progression-design.md](progression-design.md) §6 |

---

## 8. Open Questions

**Needs deciding before building**

- `[?]` Can a hole be collared **outside** the reach ring if it is aimed back
  inside? Physically no; it would also break the reach ladder. Leaning no.
- ✅ ~~Does the lab assay per interval or per hole?~~ Moot — the lab is
  retired and a recovered core is assayed on recovery, per interval, along
  the whole trace.
- `[?]` **One calibration conflict to resolve deliberately, not discover:**
  §3 says Measured needs ~10 m spacing and "you cannot get a block to
  Measured with one hole through it" — but RANGE = 20 m makes the hole's own
  block Measured (correct: the rock is in your hand) and its neighbours
  Indicated at ~0.68. The ~10 m rule therefore applies to **neighbours**, not
  the cut block. Write that down before tuning, and add the regression test
  §3 already demands: adding a hole must never increase model error
  (isotropic kernel only — a wrongly-declared anisotropy raising error is the
  point of `StructuralInterpretation`).
- `[?]` Is `RANGE` a **constant**, or a property of the ground the player can
  learn? A per-cell continuity distance would be a beautiful hidden variable —
  and a cruel one, because it silently changes what a hole is worth.
- `[?]` How is the projected trace **aimed** on a touch screen? Two sliders is
  safe; dragging the trace end in the isometric view is better and much harder.

**Can wait**

- `[?]` Should a hole be re-enterable — deepened later for less than a new one?
- `[?]` Downhole survey at T3: a separate consumable step, or automatic?
- `[?]` Does a failed hole (nothing but background) return anything besides the
  negative? Real exploration values a good miss; the model already does, since
  a barren assay lowers the estimate around it.

**Blocked on other documents**

- `[?]` Void blocks — see [module-interplay.md](../subsurface/module-interplay.md) §3
- `[?]` Whether mining raises confidence in *neighbouring* blocks, and by how
  much — same document, §3
