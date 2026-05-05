# Depth Sampling Design

> Status: DRAFT
> Last Updated: 2026-04-02
> Parent: [prospecting-master-design.md](prospecting-master-design.md)

---

## Purpose

Define how resources are distributed across depth layers, how tier progression gates depth access, and how drilling costs work.

## Decisions (Resolved)

| Question | Decision |
|----------|----------|
| Distribution model | **Geologically realistic** — follows lunar geology profiles |
| Drilling cost scaling | **Stepped with tier discounts** — fixed costs per layer, tiers reduce cost |
| Tray capacity | **4 / 8 / 12 / 16** per tier (T0/T1/T2/T3) |
| Re-sampling | **Yes** — improves confidence with diminishing returns |

## Layer Structure (Lunar Geology Model)

| Layer | Index | Tier | Geological Basis | Characteristic Resources |
|-------|-------|------|-----------------|------------------------|
| **Regolith** | 0 | T0 | Fine-grained surface soil, impact-gardened, solar-wind implanted | Fe, Si, Al, Ca (bulk), trace solar-wind H, He |
| **Megaregolith** | 1 | T1 | Coarse fragmented rock, partially sintered, impact ejecta | Ti (ilmenite concentrations), higher metal grades, some trapped volatiles |
| **Fractured Bedrock** | 2 | T2 | Cracked but coherent rock, hydrothermal alteration zones | H2O (ice in fractures), mineral veins, concentrated ores |
| **Intact Bedrock** | 3 | T3 | Solid unweathered rock, deep geological formations | He-3 (deep-implanted), rare minerals, pristine composition data |

### Resource Distribution Rationale

```
         Surface ─── most accessible, most "processed" by impacts/radiation
           │         Common elements well-mixed. Low concentration of volatiles.
           │         Good for: bulk construction materials (Fe, Si, Al)
           ▼
         Megaregolith ─── less disturbed, larger fragments
           │         Mineral grains preserved (ilmenite = FeTiO3).
           │         Good for: Ti extraction, industrial minerals
           ▼
         Fractured Bedrock ─── protected from surface processes
           │         Fractures trap migrating volatiles (water ice in PSRs).
           │         Good for: H2O, concentrated ore bodies
           ▼
         Intact Bedrock ─── pristine, untouched
                     Highest-value, hardest-to-reach deposits.
                     Good for: He-3, rare elements, definitive geological data
```

### Why This Creates Good Gameplay

- **Surface:** Easy access, common materials. Default/auto mode stops here
- **Megaregolith:** Industrial upgrade path. Player who wants Ti must invest in T1 drilling
- **Fractured Bedrock:** Water is critical for colony survival. Finding it at depth is a strategic discovery
- **Intact Bedrock:** End-game materials (He-3 = high profit). Expensive to reach but very rewarding

## Drilling Costs (Stepped with Tier Discounts)

Base costs per layer, reduced by tier. Calibrated against energy unit production rate (15.0f/cycle):

| Layer | T0 Cost | T1 Cost | T2 Cost | T3 Cost |
|-------|---------|---------|---------|---------|
| Regolith (0) | 15 energy | 12 energy | 10 energy | 8 energy |
| Megaregolith (1) | -- | 30 energy | 25 energy | 20 energy |
| Fractured Bedrock (2) | -- | -- | 50 energy | 35 energy |
| Intact Bedrock (3) | -- | -- | -- | 75 energy |

`--` = not accessible at that tier

**Cost rationale:**
- Surface sample (T0) = 1 energy unit cycle — trivially cheap, encourages exploration
- Megaregolith (T1) = 2 cycles — noticeable but routine
- Fractured Bedrock (T2) = ~3.3 cycles — meaningful investment for water access
- Intact Bedrock (T3) = 5 cycles — serious commitment for He-3/rare minerals
- Full 16-sample tray at T3 across mixed depths ≈ 40-60 energy cycles total

### Drill Time

**Flat short time for all depths.** Drilling does not take longer for deeper layers — the cost difference is purely energy. This keeps the gameplay loop snappy and avoids idle waiting.

[?] Should drilling also cost Manpower? — deferred

## Tray Capacity

| Tier | Capacity | Rationale |
|------|----------|-----------|
| T0 | 4 samples | Tight. Player must prioritize. ~1 cell fully surveyed at surface |
| T1 | 8 samples | Room for 2-3 cells. Starting to build spatial picture |
| T2 | 12 samples | Comfortable multi-cell surveys. Depth sampling becomes practical |
| T3 | 16 samples | Full survey campaigns. Deep + broad coverage possible |

**Discard:** Samples can be discarded at any time for free to free tray slots.

[?] Is there a "sample archive" for completed/analyzed samples separate from active tray?

## Re-Sampling Mechanics

Re-sampling the same cell+depth is allowed. Each re-sample:
- Produces a new sample in the tray (costs a slot)
- Adds confidence to that cell+depth, but with **diminishing returns**

**Diminishing returns formula:**
```
confidenceGain(n) = baseGain / sqrt(n)

Where n = number of times this cell+depth has been sampled
  1st sample: baseGain × 1.0
  2nd sample: baseGain × 0.71
  3rd sample: baseGain × 0.58
  4th sample: baseGain × 0.50
```

This encourages breadth (sample new cells) but allows focused re-sampling when the player really needs high confidence on a specific location.

## Default Auto-Mode Depth Priority

When in default/auto mode, the system samples in this order:
1. Regolith (surface) — always first
2. Megaregolith (shallow) — if T1+ available
3. Intact Bedrock (deep) — skips fractured bedrock
4. Fractured Bedrock (mid) — last priority

**Why skip fractured bedrock by default?** The AI doesn't know to look for water in fractures — it follows the "easy access" heuristic. A player who understands the geology will prioritize fractured bedrock when water is needed, getting better results than the auto-mode.

[?] Is this skip-pattern the right design? Alternative: AI just goes 0→1→2→3 but with lower sample quality.

## Open Questions

| Question | Status |
|----------|--------|
| Time cost for deeper drilling? | **Resolved:** Flat short time for all depths |
| Manpower cost for drilling? | [?] — deferred |
| Sample discard mechanic? | **Resolved:** Free discard anytime |
| Sample archive vs active tray? | [?] — deferred |
| Exact energy values vs production rates | **Resolved:** 15/30/50/75 base, calibrated to 15.0f/cycle |
| Geological formations spanning layers (veins, faults)? | [?] — future feature |
| How depth interacts with stratigraphic column (7E)? | Direct — column slots map 1:1 to depth layers |
