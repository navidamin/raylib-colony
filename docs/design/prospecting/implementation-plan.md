# Prospecting — Implementation Plan: Resource Classification

> Status: **C1–C4 BUILT** (2026-08-24) · panel and playtest landed
> Last Updated: 2026-08-24
> Parent: [README.md](README.md)
> Companion: [excavation-design.md §3](../excavation/excavation-design.md#the-three-classes-and-one-colour-key)

**Scope note.** This is *not* a plan for the prospecting module as a whole — that module is
built, and its design documents are marked IMPLEMENTED. This plan covers one addition:
presenting `aggregateConfidence` as three named classes on a colour key shared with
excavation. It is deliberately narrow and can be built and shipped on its own.

---

## 1. What This Is

`SubCell::aggregateConfidence` is a float. The player does not act on a float — they act on
*"can I commit to this?"* Three named bands answer that question:

| Class | Confidence | Grouping of existing `ConfidenceLevel` |
|-------|-----------|---------------------------------------|
| **Measured** | > 0.80 | `CERTAIN` |
| **Indicated** | 0.40 – 0.80 | `MODERATE` + `HIGH` |
| **Inferred** | 0.20 – 0.40 | `LOW` |
| **Unclassified** | ≤ 0.20 | `VERY_LOW` |

The classes are a **grouping of the bands already in the code**, not a new banding. That is
the whole trick: `GetConfidenceLevel()` (`prospecting_types.cpp:15`) already reads the same
field off `CONFIDENCE_THRESHOLD_LOW/MODERATE/HIGH/CERTAIN`
(`prospecting_constants.h:47-50`). Deriving from those means no new constants, and no way
for the fine reading and the coarse one to contradict each other.

**No new state. No gameplay change in prospecting.** This is presentation, plus one small
API so excavation can ask the same question.

---

## 2. Why It Belongs In Prospecting

Both modules classify one field. If excavation owned the thresholds, the prospecting panel
and the excavation panel could disagree about the same sub-cell — the exact drift the
existing `GetConfidenceLevel()` was written to prevent. Prospecting owns the confidence, so
prospecting owns its classification, and excavation reads it.

The dependency runs the same way it already does for the write-back (excavation Phase 4):
**excavation calls prospecting, never the reverse.**

---

## 3. The Colour Key, and Why It Is Worth Doing

The same three colours carry the same meaning at three different scales:

| Surface | What the colour does |
|---------|---------------------|
| **Resource icon** in the element switcher | A three-segment ring: how much of this element is Measured / Indicated / Inferred. Readable at 40 px |
| **Depth map** (four stacked layers) | Per-spot class, so the shape of what you know is visible at a glance |
| **Excavation grid** | Per-spot class, so target selection and survey planning use one key |

Because the ring is a summary of exactly the field the map shows in detail, the player can
glance at the icon bar and know which elements are drilled out and which are guesses without
opening anything. **That only works because colour is spent on class rather than element.**
Colour identifying the element would make the ring and the map mean different things, and
the summary would be worthless. The element identifies itself through the icon, the tinted
rock wall, the panel heading and the shape of the relief — none of which cost a per-cell
channel.

Prototyped and compared against three alternatives (element colour + class outline; element
hue + class lightness band; class rotates the element's hue) before settling on this one.

---

## 4. Build Order

Four phases. Nothing here is blocking for anyone else, and each phase is independently
verifiable.

### Phase C1 — The type and the classifier ✅ DONE

- `enum class ResourceClass { UNCLASSIFIED, INFERRED, INDICATED, MEASURED }` in
  `prospecting_types.h`, beside `ConfidenceLevel`
- `ResourceClass GetResourceClass(float confidence)` in `prospecting_types.cpp`, written as
  a `switch` over `GetConfidenceLevel()` — **not** as a second set of `if` thresholds, so it
  cannot drift
- `const char* ResourceClassName(ResourceClass)` for the panel readouts

**Verify:** unit test that walks confidence 0.00 → 1.00 in 0.01 steps and asserts the class
never disagrees with the `ConfidenceLevel` it is grouped from. That test is the whole point
of deriving rather than re-thresholding.

### Phase C2 — The theme token ✅ DONE

- `EXT_CLASS_INFERRED = {124, 143, 214, 255}` in `rendermanager.cpp`, beside the other
  `EXT_*` colours
- `Color ExtClassColor(ResourceClass)` mapping the four classes to
  `EXT_ACCENT_GREEN` / `EXT_ACCENT_GOLD` / `EXT_CLASS_INFERRED` / `EXT_DIM_TEXT`

`EXT_ACCENT_VIOLET` {170,110,255} **cannot** be reused: it is within a few units of
`EXT_HEADER_COLOR` {168,130,255}, so section headings and Inferred ground would read as the
same thing. The muted violet is deliberate — Inferred is the class the eye should settle on
least.

### Phase C3 — The prospecting panel ✅ DONE

- Grid cells carry their class colour
- The confidence readout names the class alongside the existing level text
- A per-depth class strip, so *"surveyed at the surface, guessing below"* is visible without
  clicking through the depth tabs

**Verify with `tools/preview`.** Render every state — all four classes, all four depths, a
dug spot, and a spot dug but never surveyed — and **look at the PNGs**. Per
`docs/dev-workflow.md`, never claim a visual result without rendering it.

### Phase C4 — The per-element roll-up ✅ DONE (roll-up + playtest; icon ring still to draw)

The ring on the resource icon needs a per-element tonnage split by class:

```
for each element:
    for each reachable sub-cell, for each accessible depth:
        class  = GetResourceClass(confidence(cell, depth))
        amount = GetQuantity(cell, depth) × GetGroundTruth(cell, depth)[element]
        total[class] += amount
```

Note the `quantity × composition` product — this is the units trap named in
`docs/guides/module-architecture.md` Part II §2 and in the excavation plan §2. Fractions
alone or quantity alone are both wrong.

- `struct ClassSplit { float measured, indicated, inferred, unclassified; }`
- `ClassSplit ProspectingGrid::GetClassSplit(ResourceType, int tier) const`
- Cache it and recompute on survey/dig, not per frame — it is a full lattice sweep

**Verify with `colony_inspect`**: dump the split for a real seed and check the four figures
sum to the lattice total. If a number looks wrong, dump the data before theorising.

---

## 5. Sequencing

```
C1 ─▶ C2 ─▶ C3
 └──▶ C4 ─────▶ (excavation grid + icon ring)
```

- **C1 unblocks excavation.** Once `GetResourceClass` exists, the excavation panel can use
  it; C2–C4 are not on that critical path.
- **C4 is independent of C3** — one is a roll-up, the other is per-cell rendering.

---

## 6. Testing

| Instrument | Use for |
|-----------|---------|
| Unit test | C1 — class never contradicts `ConfidenceLevel`, across the full range |
| `tools/preview` | C3 — every class, every depth, dug and undug states |
| `tools/inspect` | C4 — real per-element splits against a real seed |
| `colony_sim` | C4 — assert the split moves the right way as a simulated player surveys |

One ordering worth asserting in code: **surveying never moves a spot to a lower class.**
Confidence is monotonic today; a regression that broke it would be invisible in a screenshot
and obvious in a test.

---

## 7. Risks

| Risk | Mitigation |
|------|-----------|
| A second set of thresholds drifts from `ConfidenceLevel` | C1 derives by `switch`, and the test asserts it. Never re-threshold |
| Colour-blind readers cannot separate green from amber | Class is also named in text on every readout; the panel never relies on colour alone |
| The roll-up runs per frame | Cache in `ProspectingGrid`, invalidate on survey and on `RecordExcavation` |
| Inferred violet collides with `EXT_HEADER_COLOR` | New token, deliberately desaturated (C2) |

---

## 8. What Building It Changed

Three things the plan did not anticipate, all resolved in the code.

**Per-depth confidence had the wrong owner.** The plan assumed prospecting
already had the field it needed to classify. It did not — `SiteView::GetConfidence`
in *excavation* was computing it, from inputs that are all prospecting state:
the sub-cell, its sweep band, the sample tray. The depth physics already lived
in prospecting; only the two evidence weights sat in excavation. Those moved to
`prospecting_constants.h` with `EXC_*` aliases kept, the arithmetic moved
verbatim to `GetDepthConfidence()`, and `SiteView` now delegates. One
implementation, so the drift this plan exists to prevent cannot happen on that
axis either.

**The two-channel question answered itself, loudly.** The plan flagged that the
grid already shades by yield and asked for both to be rendered before deciding.
Rendering them showed a direct collision: the sweep-heat ramp ran navy → cyan →
**green** → magenta, so green meant *both* "strong signal" and "Measured", and
cyan is already the selection accent. Resolved the way the plan said to — class
wins. Signal is now a single-hue plum luminance ramp (intensity); class is hue,
on a ring drawn over it. Two variables, two channels.

**A dug spot read as a contradiction.** With the class shown, `MEASURED` sat
directly above `Confidence: Very Low`. Both were true — the class counts
digging, that number measures what the *instruments* found — but together they
read as a bug. Relabelled `Instruments:`, which is what it always was.

---

## 9. Open

- ✅ **Worked stays a separate flag.** It is orthogonal — a worked spot has a class *and* is
  emptied — and `workedFraction` already carries it. `ResourceClass` stays four values.
- `[?]` The **icon ring** — C4's roll-up drawn as a three-segment ring on a resource icon —
  is not built. `GetClassSplit()` and the playtest's stacked bar prove the data; the ring
  needs an element switcher, which the panel does not have yet
- `[?]` `ClassSplit` is recomputed on demand, not cached. It is a full lattice sweep, so it
  wants invalidate-on-survey caching before it goes anywhere that runs per frame — the
  playtest gets away with it at 8×8 × 4 depths
- `[?]` Does the depth map roll up to a **single class per column** anywhere, and if so by
  what rule — worst layer, or the shallowest unworked one? Only matters if a summary view
  appears above the four-layer map
