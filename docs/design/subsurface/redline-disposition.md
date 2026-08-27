# Redline — Disposition of the Drilling Prototype

> **Auto-context rule:** Read this before changing
> `prototypes/redline.html`, before wiring any hands-on drilling mode into
> either module, and before deciding what a drill action costs when it goes
> badly.

`prototypes/redline.html` was built as a playable toy from `prototypes/drill-rig.html`,
ahead of any decision about where it belonged. This document decides that, and
records the one place the toy contradicted the design of record.

**Status: DECISION.** Supersedes nothing; constrains the prototype and the two
modules' hands-on modes.

---

## 1. What the Prototype Proved

Worth keeping, all of it:

- The **side-view cross-section** specified in
  [drilling-procedure.md](drilling-procedure.md) §6 works. Strata, string,
  turning flights, heat as colour on metal, cracks, cuttings rising, camera
  following the bit.
- **Click-to-spike-RPM with decay back to a crawl** is a good control. It is
  an impulse, not a setting, so the player is never "holding a slider" — they
  are working the machine.
- The ground it plays on is **already the real column**: `REGOLITH 0-12`,
  `MEGAREGOLITH 12-34`, `FRACTURED 34-68` (icy), `INTACT BASALT 68-120`, which
  is `LAYER_THICKNESS_M {12,22,34,52}` summing to 120 m. Nothing was invented
  to make the toy work.
- Hardness driving **both** cut rate and heat gain produces the intended
  tension without any extra rule: the ground that most needs driving is the
  ground that most punishes it.

## 2. Where It Was Wrong

[drilling-procedure.md](drilling-procedure.md) §1, Rule 1:

> **Bad technique costs knowledge or product, never just parts.** In a
> knowledge game the sting must land on the knowledge side: a gap in the
> column, a cooked assay, a diluted load. Broken equipment is the *lesser*
> penalty.

The prototype's **only** failure state was `"Bit failed"` — the run ends, the
score is the depth reached. That is the demoted penalty promoted to being the
whole game, and it is the one failure identical across both modules, so it is
also the maximally-shared, minimally-differentiated version of drilling.

Two smaller symptoms of the same thing:

- The `FRACTURED` layer carried `icy: true` and heat never touched it. The
  failure §3 calls *"the most on-theme failure this game can have"* was
  decoration.
- The score was **depth**. Neither module wants depth for its own sake.
  Prospecting wants a logged column; excavation wants tonnes at grade.

## 3. The Decision

**Redline becomes prospecting's hands-on drilling mode.** Excavation gets the
same view and the same physics later, with different landings, per
[drilling-procedure.md](drilling-procedure.md) §4 and the build order in §5.

Prospecting first, for a reason that is structural rather than a preference:

| | Prospecting | Excavation |
|---|---|---|
| Has a discrete, player-initiated action | `SamplingEngine::CollectSample` — atomic, returns | none: the panel assigns state, `ExcavationSystem::Dig` ticks per frame |
| An episode is | one hole | one *stint on a face* — not a hole |

A minigame is a bounded episode. Prospecting already has the boundary;
excavation does not, and its continuous model was measured and defended, not
assumed (`EXC_MIN_TAPER` exists because a spot that never exhausted was a bug;
AUTO works one face until spent because a margin-based re-pick was measured to
scratch the whole lattice and exhaust nothing). Excavation's port must be a
stint, or it will fight the model it already has.

## 4. The Failure Ladder

Three tiers, in order of how much they hurt. This is the operative change.

### Tier 1 — Cooked evidence (costs knowledge, and silently)

Heat above `COOK_THRESHOLD` while cutting an **ice-bearing** interval
sublimates the water out of the core. The interval's assay comes back
**falsely low**, and *the player is not told at the time*.

The silence is the mechanic. A visible "you ruined this" message makes it a
slap; an assay you have no reason to distrust makes it a **wrong belief you
act on** — you write off good ground because of your own impatience, and find
out when a later hole disagrees or when you mine it and it is not there.

### Tier 2 — Lost interval (costs knowledge, visibly)

Contact pressure has a **band that moves with the strata**. In band, the core
comes up intact. Out of band — over-pressure especially, and worst in
fractured ground — the barrel comes up part empty and that interval is a **gap
in the column**: no assay exactly where you wanted one.

This is what makes clicking a *modulation* rather than a spam: over-driving
still advances fast, which is the temptation, but it costs the thing you came
for.

### Tier 3 — Bit wear and tripping (costs time — the lesser penalty)

Bits dull with metres cut, faster in hard rock, much faster when driven hot.
A dull bit does not end the run. It forces a **trip**: pull the string rod by
rod and run it back, **time proportional to current depth**.

That converts depth into the recurring push-your-luck the design already
names:

> *"bit at 60% at 40 m — trip now cheap, or push to target and risk tripping
> from 90 m?"*

Voluntary tripping must therefore be available, and cheaper the shallower you
are. This is also what gives the **wireline** technique its meaning later:
core retrieved *without* pulling the string.

### The run ends when

The player pulls the string, or reaches target depth. **Not** on a broken bit.

### The score is

The **logged column** — metres logged, mean recovery, intervals lost, and
assay integrity — not the depth reached. Depth is an input to the score, not
the score.

## 5. What This Does Not Decide

Named so they are not mistaken for settled:

- **Where a per-hole outcome lands.** `surveyProgress = 0.20 x sweep + 0.80 x
  sample`, and the sample term is coverage of cells with non-empty `sampleIds`
  times mean richness. There is **no recovery term and no assay-quality term**.
  Until one exists, a minigame result has nowhere to go. This is the single
  blocker most likely to void the feature.
- **How long a hole takes.** Three mutually exclusive answers are on file:
  flat-and-short (`prospecting/depth-sampling-design.md`), priced per metre
  with a "5.8 days" planning readout (`prospecting/block-model-design.md` §4),
  and a real-time sub-minute session (this prototype, which ignores game time
  entirely). Dwells, trips and downtime are meaningless until this is settled.
- **Whether bad excavation technique can corrupt data**, not just lose
  product. §4 says a digging machine's telemetry feeds the same confidence
  writeback as the empty-out, which implies it should — but §4 grants
  excavation only yield loss.
- **Who owns the drill view code.** §6 calls it "shared by both modules'
  hands-on mode" and names no `RenderManager` method and no directory.
- **Whether sublimation applies to water only** or to volatiles generally. No
  `ResourceType` is named in any heat discussion.
- **Whether energy is charged before or after** a failable action. Today
  `ConsumeResource(ENERGY, drillCost)` runs unconditionally before
  `CollectSample`, so a failable minigame would charge first and fail after.

Two further cautions:

- The 8x8 -> 16x16 lattice migration invalidates every measured footprint
  number. A minigame tuned against 12.5 m sub-cells is tuned against geometry
  already slated to change.
- No real-world target number exists for how much a redline should cost an
  assay. Published volatile-loss figures could not be verified against primary
  sources; treat every constant here as playtest-derived, not sourced.

## 6. Cross-References

| Where | What |
|-------|------|
| [drilling-procedure.md](drilling-procedure.md) | the physical contract this implements; Rule 1, §3 prospecting frame, §5 build order |
| [module-interplay.md](module-interplay.md) | why the consequence must differ by module |
| `prototypes/redline.html` | the playable prototype |
| `prototypes/drill-rig.html` | the rig renderer it was built from |
| `src/Prospecting/sampling_engine.cpp` | `CollectSample` — the discrete action an episode would wrap |
| `src/Prospecting/survey_progress_engine.cpp` | where a recovery term would have to land |
