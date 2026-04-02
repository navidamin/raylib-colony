# Confidence Level System

> Status: DRAFT
> Last Updated: 2026-04-02
> Parent: [prospecting-master-design.md](prospecting-master-design.md)

---

## Purpose

Define a coherent confidence metric that spans all prospecting technologies and methods. Confidence represents how certain the player (and the game system) can be about prospecting results.

## Core Decisions (Resolved)

| Question | Decision |
|----------|----------|
| What does confidence measure? | Accuracy/certainty of element readings and deposit knowledge |
| Scale | **Hybrid:** 0-100% tracked internally, displayed as discrete levels |
| Granularity | **Hierarchical** — see below |
| Relationship to surveyProgress | **Confidence modifies survey progress contribution** — higher confidence = more survey progress per analysis result |

### Display Levels

| Internal Range | Display Level | Color |
|---------------|--------------|-------|
| 0-20% | Very Low | Red |
| 21-40% | Low | Orange |
| 41-60% | Moderate | Yellow |
| 61-80% | High | Light Green |
| 81-100% | Certain | Green |

### Hierarchical Granularity

Confidence operates at three nested levels. The player sees progressively more detail as they drill into the data:

```
LEVEL 1: Cell Overview (heat map)
    ┌───────────────────────────────────────────┐
    │  5x5 grid, each cell shows aggregate      │
    │  confidence as color (Very Low → Certain)  │
    │  This is the FIRST VIEW in prospecting     │
    │  menu — evolves as sampling/testing occurs  │
    └───────────────────────────────────────────┘
              │
              │ (click cell)
              ▼
LEVEL 2: Per-Element Breakdown
    ┌───────────────────────────────────────────┐
    │  Fe: ████████░░ High (72%)                │
    │  Si: ██████░░░░ Moderate (58%)            │
    │  H2: ██░░░░░░░░ Low (22%)                 │
    │  Ti: ░░░░░░░░░░ Very Low (8%)             │
    │                                           │
    │  (XRF gave good Fe/Si data but missed H2) │
    └───────────────────────────────────────────┘
              │
              │ (expand depth view)
              ▼
LEVEL 3: Per-Depth-Layer Breakdown
    ┌───────────────────────────────────────────┐
    │  Fe confidence by depth:                  │
    │  Surface:  ████████░░ High (75%)          │
    │  Shallow:  ██████░░░░ Moderate (55%)      │
    │  Mid:      ████░░░░░░ Low (38%)           │
    │  Deep:     ░░░░░░░░░░ Very Low (5%)       │
    │                                           │
    │  (only surface/shallow sampled so far)    │
    └───────────────────────────────────────────┘
```

**Cell-level aggregate** = weighted average of all per-element confidences (weighted by element abundance or equal weight — [?])

## Confidence → Survey Progress Formula

Each analysis result contributes to survey progress, **scaled by confidence**:

```
surveyProgressGain = baseGain × confidenceMultiplier

Where confidenceMultiplier maps from the confidence of that specific measurement:
  Very Low (0-20%):   0.2x multiplier
  Low (21-40%):       0.4x
  Moderate (41-60%):  0.7x
  High (61-80%):      0.9x
  Certain (81-100%):  1.0x
```

This means low-confidence results still contribute (you're not locked out), but investing in better tools/methods pays off significantly.

## Per-Technology Confidence Contributions

| Technology | Confidence Output | Factors |
|-----------|-------------------|---------|
| GPR Sweep | +5-15% per cell (broad, low) | Frequency used, number of sweeps |
| Visual Inspection | +5-10% per element (categories only) | Always low |
| XRF | +30-50% for heavy elements (Fe, Si, Al, Ca, Ti); 0% for light (H, C, O) | Tier quality |
| LIBS | +20-40% for all elements | Tier, noise level |
| Fire Assay | Sets to 100% for ONE chosen element | Destructive |

## Composition Rules (Resolved)

Multiple measurements on the same element compose using **probabilistic composition**:

```
confidence = 1 - (1 - c1)(1 - c2)(1 - c3)...

Where c1, c2, c3 are individual tool contributions (as fractions, e.g., 0.35 for 35%)
```

**Why this formula:**
- Natural ceiling at 100% (can never exceed)
- Rewards tool diversity — each new tool type adds significant value
- Diminishing returns from same-tool re-application
- Intuitive: "probability of NOT missing something decreases with each measurement"

**Example:**
- XRF gives 40% confidence on Fe → conf = 0.40
- LIBS gives 25% on Fe → conf = 1 - (1-0.40)(1-0.25) = 1 - 0.45 = 0.55 (55%)
- Re-sample XRF gives another 40% → conf = 1 - (1-0.55)(1-0.40) = 1 - 0.27 = 0.73 (73%)

## Confidence Decay

**No decay.** Confidence is permanent once established. Geological data doesn't become less valid over time.

## AI/Default Mode Confidence Penalty

AI-produced results carry a **small confidence penalty** representing less careful instrument handling:

| Tier | AI Confidence Penalty |
|------|----------------------|
| T0 | -20% (max penalty) |
| T1 | -15% |
| T2 | -10% |
| T3 | -5% |

The penalty decreases as the AI improves with tier. See [ai-default-mode.md](ai-default-mode.md) for full AI design.

## Per-Tool Contribution Visibility

**Yes** — in the per-element detail view (Level 2), each tool's contribution is shown:
```
Fe: ████████░░ High (72%)
  └─ XRF scan: +40%
  └─ LIBS pulse: +25%
  └─ Re-sample XRF: +18% (diminished)
```

## Open Questions

| Question | Status |
|----------|--------|
| Cell aggregate weighting (by abundance or equal?) | [?] — minor, deferred |
| Is there a QA/QC verification action that boosts confidence? | [?] — possible future mechanic |
