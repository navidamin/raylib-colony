# Excavation Rebuild — The Full Workflow

Status: PLAN
Supersedes the panel half of [`implementation-plan.md`](implementation-plan.md) (its phases 1–4 shipped; its phase 5 "the panel" is what this replaces)
Read first: [`README.md`](README.md), [`excavation-design.md`](excavation-design.md), [`../graphics/dark-plating.md`](../graphics/dark-plating.md) §6.5

---

## 1. What we are building, in one paragraph

Excavation gets **prospecting's anatomy and none of its subject**. The same
three-part panel — block model on the left, a vertical instrument bar down the
middle, a control rail on the right — but the model is shaded by what
excavation cares about, the bar sinks a **shaft** instead of a borehole, the rig
in it is the **diamond rotary drill** rather than the auger, and there is **no
core log**, because excavation does not recover core. It reads what prospecting
learned and it writes back what it took out.

The one-line contract, unchanged from `README.md`: **prospecting produces
knowledge, excavation consumes it.** Nothing in this rebuild reverses that
arrow.

---

## 2. Where things stand today

Two systems live side by side, and only one of them is doing any work.

| | Status | Evidence |
|---|---|---|
| `src/Excavation/` (5 sources, `SiteView`/`EstimateEngine`/`DigEngine`/`AutoPilot`/`ExcavationSystem`) | **LIVE** | in `COLONY_CORE_SOURCES`; `Unit::ProcessExtraction` calls `Dig()` every tick (`unit.cpp:1387`) |
| `DrawExcavationPanel` (~480 lines, `rendermanager.cpp:4664-5146`) | **LIVE** | dispatched at `rendermanager.cpp:2020` |
| `Unit::Excavator` struct + its four accessors | **DEAD as a model, LIVE as a counter** | `GetExcavators`/`MoveExcavator`/`SetExcavatorDepth`/`SetExcavatorRate` have **zero callers**; the vector survives only to be counted for energy (`unit.cpp:1154-1165`) and machine count (`1368-1373`) |

Because `SetExcavatorDepth` is never called, `Excavator::depth` is permanently
`0.0f`, so `ProcessExtraction`'s depth-layer derivation (`unit.cpp:1265-1273`)
always yields `SURFACE` and feeds only a fallback branch that cannot be reached
by a real Extraction unit. **The old model is already a corpse being carried
by the live one.** That is the first thing to bury.

---

## 3. The three things that make excavation not-prospecting

Everything in this plan follows from these. They are also what stops the clone
being a reskin.

### 3.1 A needle vs. a volume

A drill hole is a needle: prospecting's `LineHole` is a slanted line that can be
aimed anywhere, and its record is per-crossing (`RecordCore`). A working face is
a **volume**, and it must connect to the surface — that is the access rule from
`excavation-design.md` §Access, and it is the one thing excavation has that
prospecting cannot. So excavation's bar does not draw a 15 px borehole with a
string in it; it draws a **shaft**: a wide, square-shouldered excavation with
the diamond rotary rig working at its bottom and the ground it has opened
standing empty above.

### 3.2 What comes up is mass, not evidence

Prospecting's bar carries a **core log lane** — 24 sticks graded intact /
partial / lost, because the question is *did the assay survive*. Excavation's
answer to that lane is a **depletion column**: per depth, how much of this
column has already been taken out. Same lane, same depth mapping, opposite
question — not "what did I learn" but "what is left".

This is the user's "no core logs", read as a design constraint rather than a
subtraction: the lane is not deleted, it is re-answered.

### 3.3 Excavation reads a belief it did not form

Prospecting's block model shades by its own estimate. Excavation's block model
shades by **the same estimate**, obtained from the same
`BuildEstimateField(grid, targetResource)` — but overlaid with two things
prospecting has no reason to draw: **excavation's own reach ring** (its tier,
not prospecting's) and **worked-out ground**. That asymmetry is where the
module's gamble lives, per `README.md`: excavation can reach ground prospecting
has seen but not surveyed, and vice versa.

---

## 4. The target panel

```
+---------------------------------------------+--------+------------------+
|  BLOCK MODEL (~60%)                          | SHAFT  | CONTROL RAIL     |
|                                              | BAR    |                  |
|   4 iso plates, one per depth layer          | 104 px | TARGET  Fe       |
|   shaded by ESTIMATED yield of the target    |        | [resource chips] |
|   worked spots drained, out-of-reach locked  | strata |                  |
|                                              | shaft  | PACE      ----o- |
|   [plate 0  SURFACE   0 m]                   | rig    | POWER CAP -o--- |
|   [plate 1  SHALLOW  12 m]                   | teeth  |                  |
|   [plate 2  MID      34 m]                   |        | MACHINE BAY      |
|   [plate 3  DEEP     68 m]                   | deple- | [6 machine cards]|
|                                              | tion   |                  |
|                                              | lane   | AUTOMATION       |
|                                              |        | OFF BASIC ...    |
|                                              |        |                  |
|                                              |        | READOUT          |
+---------------------------------------------+--------+------------------+
```

Left and centre share **one depth axis**, exactly as prospecting does: the bar's
`bandTop[]` is derived from the plate stack's own `PlateLineY`, never tuned
separately. That derivation is the single most valuable thing to copy from the
prospecting panel and the reason its two halves read as one instrument.

---

## 5. The phases

Each phase leaves the game building, tested, and playable. No phase is allowed
to end with excavation unreachable.

### Phase 0 — the graveyard opens, and the corpse goes in — **DONE** (`f2a0911`)

Retire what the survey proved is already dead. **No behaviour change**, so it is
provable by build + tests alone.

| Part | Lives at | Why it goes |
|------|----------|-------------|
| `Unit::GetExcavators` / `MoveExcavator` / `SetExcavatorDepth` / `SetExcavatorRate` | `unit.h:127-130`, `unit.cpp:1780-1825` | zero callers anywhere in `src/`, `tools/`, `tests/` |
| `ProcessExtraction` depth-layer derivation | `unit.cpp:1265-1273` | reads `excavators[0].depth`, which no code ever sets — always `SURFACE` |
| `ProcessExtraction` legacy fallback skim | `unit.cpp:1402-1426` | unreachable: both systems are constructed together at `unit.cpp:43-57` |
| `extractionRates` map | `unit.cpp:1347-1353` | consumed only by the fallback above |
| `ExcavationSystem::DescribeSelected` | `excavation_system.cpp:55-60` | zero callers |
| `DEFAULT_WearAndTear` / `parameters["WearAndTear"]` | `game_constants.h:96`, `unit.cpp:242` | written once, never read |

**Deliberately NOT in phase 0:** the `Excavator` struct and its vector. They are
still counted for energy and machine count. They die in phase 5, once the
excavation system owns its own machine count.

Exit test: full build clean, `colony_tests` green, one excavation preview PNG
identical to before.

### Phase 1 — the shaft bar exists and can be looked at — **DONE**

New drawing only. The bar renders against the *current* panel so it can be
judged before anything is torn out.

- `ExcDockGeom` + `ExcDockFrom(blockGeom, x, w)` — derived from the plate stack,
  the same discipline as `ProsDockFrom`.
- `ExcDrawShaftDock(...)` — sky, strata bands (reusing `RockTexture`), the
  shaft, the **diamond rotary rig** ported from
  `prototypes/diamond-drill.html` per dark-plating §6.5, spoil at the collar.
- The rig's variant follows **excavation's** tier: compact below T2, heavy duty
  at T2+.
- Deterministic preview hooks so `preview.sh --module excavation` can shoot it.

Exit test: preview PNGs at four shaft depths, looked at, in the commit message.

**What phase 1 actually settled**, beyond the list above:

- `ProsDockGeom` → **`DockGeom`**, `ProsDockFrom` → `DockFromBlock`. The depth
  axis is pure geometry with nothing prospecting-specific in it, and
  excavation is now its second caller, so the name stopped claiming an owner.
- `ExcDockEven` builds the bands **evenly spaced**, which is what the plate
  stack hands over — so phase 3's swap to `DockFromBlock` changes no pixels.
- Excavation's strip is **120 px against prospecting's 104**, and the extra
  width is spent on rock. A shaft has to leave strata either side of it or the
  depth context is lost.
- The rig's `ExcDrawJoint` came out byte-identical to `ProsDrawJoint`, so the
  copy was dropped and the original is called.
- `--depth 0..3` added to the preview tool, for the same reason prospecting
  needed `previewHoverLayer`: headless has no pointer to click a depth with.

**Deferred, deliberately:** `ProsSteel` / `ProsBandedSlice` / `PROS_OUT` /
`PROS_*_TONES` / `ProsRnd` are Dark Plating's **shared material layer** (style
guide §4–§5), not prospecting's property — the prefix is historical. Excavation
calls them rather than duplicating six functions. Renaming them out of the
`Pros` namespace belongs in **phase 3**, when the panel is recomposed and the
diff is already in this file; doing it in phase 1 would have widened an
additive change into a 60-site rename in a file other sessions are editing.

### Phase 2 — the block model comes to excavation — **DONE**

- Build the four `BlockCell` layers from **one** `BuildEstimateField(grid,
  target)`. The scalar `GetEstimatedYield` path is O(N⁴) and was measured at
  55 ms/frame at 16×16; at 32×32 it is 16× worse. This is a documented trap.
- Reuse `ProsDrawBlockLayer` unchanged — it already takes every input as a
  parameter and holds no member state.
- Overlay excavation's own reach ring (`IsSubCellInReach` with the *excavation*
  tier) and worked-out drain (`SubCell::workedFraction`).
- ~~Rename the four cell-marker helpers out of the `Pros` prefix.~~ They did
  not need renaming, they needed **burying** — every one was called only by the
  flat lattice, so they died with it. See
  [`../graveyard/excavation-flat-lattice.md`](../graveyard/excavation-flat-lattice.md).

**What phase 2 actually settled:**

- **The relief reads what is LEFT**, not what was there: grade is drained by
  `workedFraction` before the plates are built. This is §3.2's idea arriving
  early — the depletion *lane* is not needed, because depletion is already the
  shape of the ground.
- **Worked-out ground needs no marker.** Digging sets confidence to 1.0 at that
  spot and depth, so a worked cell is MEASURED with no relief while barren
  unsurveyed ground is UNCLASSIFIED with no relief. The class colour carries it.
  The first attempt drew an amber diamond per dug cell and it swamped the
  plates — at 32×32 a tile is ~3 px and **no per-cell overlay can read**.
- **The depth row is gone: the plate is the depth.** One click sets spot and
  layer together, which removes a two-control sync hazard rather than just
  saving space.
- **The reach ring is dashed**, because the active-plate rim is already a solid
  amber square and two solid amber squares on one plate read as one shape with
  a mistake in it.
- A **locked depth is a whole plate** held at 0.42 light with `LOCKED` on its
  label, not a per-cell glyph. A depth is locked; a cell is not.
- `ExcavationSystem` gained `plateLight[4]` / `UpdatePlateLight` /
  `previewHoverLayer`, mirroring prospecting — persistent presentation state on
  the facade, per CLAUDE.md, because the renderer is rebuilt each frame and
  could only ever snap.

**Fixed here, introduced in phase 1:** the dock-width change hit the *first*
`dockW = 104.0f` in the file, which was **prospecting's**. Prospecting's strip
had been silently widened to 120 and excavation's left at 104 — the exact
opposite of what was intended and what the phase 1 commit message claimed.
Prospecting is back to 104 and excavation is 120.

### Phase 3 — the panel is recomposed, and the flat grid is buried

Block model + shaft bar + control rail, in the prospecting layout. The old flat
N×N lattice drawing (`rendermanager.cpp:4712-4862`) goes to the graveyard; the
control rail (`4863-5145`) is **kept and restyled**, not rewritten — it already
carries target/pace/power/machines/automation/readout and those are all still
the right controls.

### Phase 4 — sinking becomes an action

Today `selectedDepth` is a radio button gated by tier. That is the placeholder
the access design was always going to replace.

- `ShaftState { NONE, SINKING, AT_DEPTH }` on `ExcavationSystem`, shaped like
  `LineHoleState`.
- Sinking costs energy and time, and opens the **3×3 block** the design
  measured as the right footprint (`excavation-design.md` §Access: 14% of the
  lattice, 30.8% of a well-sited field's yield, 2.2× concentration).
- Depth becomes something you *paid for*, not something a tier handed you.

This is the phase that turns the panel from a viewer into a decision, and it is
the first one that changes the balance. It should land alone.

### Phase 5 — the old excavator counter dies

`Unit::Excavator`, `std::vector<Excavator> excavators`, and the three spawn
sites (`unit.cpp:452-460`, `858-885`, `1027-1054`) go to the graveyard. Machine
count moves to `EXC_MACHINE_COUNT_PER_TIER` on the excavation system, which is
where the table already lives. Energy cost (`unit.cpp:1154-1165`) reads that
instead.

Note the duplication to remove with it: `DebugUpgradeModuleTier`
(`unit.cpp:1027-1054`) is an exact copy of the `UpgradeModuleTier` block.

### Phase 6 — balance, previews, tests

- Preview states for excavation mirroring prospecting's (`--state` covering
  idle / sinking / at-depth / worked-out).
- Move the real excavation invariants from `tools/test/excavation_test.cpp`
  (748 lines, 17 checks, **not run by ctest**) into `tests/` so CI actually
  runs them. This is the biggest quality gap the survey found.
- Balance pass against dumped data, per `implementation-plan.md` phase 7.

---

## 6. Traps, collected up front

Each of these has already cost this codebase real time. They are listed here so
the rebuild does not re-buy them.

| Trap | Where it bit | The rule |
|------|-------------|----------|
| **Quantity × fraction** | "the most expensive bug this codebase has had" (`module-architecture.md` Part II §2) | `GetQuantity` is absolute, `GetGroundTruth` is fractions. Neither is a yield. Use `GetSubCellYield` |
| **O(N⁴) estimate scan** | 55 ms/frame at 16×16; the lattice is now 32 | Build one `EstimateField`, never per-cell `GetEstimatedYield` in a loop |
| **Stale lattice literals** | a bare `8` survived the 8→16 migration and made every spot with x or y ≥ 8 read as exhausted | Never write the grid size as a literal. `tests/test_dig_site.cpp` exists solely to catch this |
| **Two draw paths** | textured batch vs `DrawTriangle`: 27 ms vs 216 ms | Keep the batched path; the fallback is not a simplification |
| **Hover before draw** | plate brightness depends on the pick, so picking after drawing lights the wrong layer for a frame | Pick first, then draw |
| **Stale comments** | `excavation_constants.h:15` and `site_view.h:26` both still say "fixed 8×8"; the lattice is 32 | Fix them in the phase that touches the file |
| **Engine-implemented ≠ reachable** | `docs/guides/feature-completeness.md` | Every phase ends with a rendered PNG or a failing-then-passing test, not a claim |

---

## 7. What this plan does not decide

Flagged rather than guessed, to be settled when the phase arrives:

1. **Does the shaft replace the machine bay or sit beside it?** The design says
   access (a shaft) and production (machines at a spot) are different purchases,
   which argues for both. Phase 4 has to show they do not compete for the same
   screen space.
2. **Does the depletion lane show the selected column, or the whole 3×3 a shaft
   opened?** The column is simpler; the block is what the player actually bought.
3. **Does prospecting's `previewHoverLayer` pattern get generalised?** It is the
   only way hover states are screenshottable headlessly, and excavation will
   want it too.

---

## 8. Order of work, and why this order

The phases are ordered so that **the thing most likely to be wrong is looked at
earliest and cheapest**. The art (phase 1) is judged from a PNG before any
gameplay is disturbed; the model (phase 2) is proven against real generated data
before the panel is recomposed (phase 3); and the two phases that change what a
player actually does — sinking (4) and machine count (5) — land last, alone,
where a balance regression is attributable.

Phase 0 comes first for a different reason: it is the only phase whose
correctness is provable by the compiler.
