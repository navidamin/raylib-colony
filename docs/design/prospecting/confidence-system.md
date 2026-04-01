# Confidence Level System

> Status: STUB
> Last Updated: 2026-04-01
> Parent: [prospecting-master-design.md](prospecting-master-design.md)

---

## Purpose

Define a coherent confidence metric that spans all prospecting technologies and methods. Confidence represents how certain the player (and the game system) can be about prospecting results.

## Core Questions

| Question | Decision | Notes |
|----------|----------|-------|
| What does confidence measure? | [?] | Options: accuracy of element readings, certainty of deposit existence, or both |
| Scale | [?] | Options: 0-100% continuous, discrete levels (Low/Moderate/High/Certain), letter grades |
| Granularity | [?] | Options: per-cell, per-element, per-sample, per-depth-layer |
| Relationship to surveyProgress | [?] | Is confidence a component that feeds into surveyProgress, or a separate parallel metric? |

## Per-Technology Confidence Contributions

| Technology | Confidence Output | Factors |
|-----------|-------------------|---------|
| GPR Sweep | [?] | Frequency used, number of sweeps, anomaly strength |
| Visual Inspection | [?] | Always low — categories only |
| XRF | [?] | Good for heavy elements, zero for light elements |
| LIBS | [?] | High for all elements, moderate noise at low tier |
| Fire Assay | [?] | Perfect for one element (100% confidence for that element) |

## Composition Rules

How do multiple measurements compose?

| Option | Rule | Pros | Cons |
|--------|------|------|------|
| Additive | conf = sum(contributions) capped at 100% | Simple | No diminishing returns |
| Bayesian | P(correct) = 1 - product(1 - P_i) | Realistic | Complex, hard to display |
| Max | conf = max(all contributions) | Very simple | No incentive for multiple tools |
| Weighted average | conf = weighted_mean(contributions) | Balanced | Weights need tuning |

[?] — Decision needed. Bayesian is most scientifically coherent but may be opaque to player. Additive with diminishing returns (sqrt-based?) may be the pragmatic choice.

## Gameplay Impact

How does confidence affect the game?

| Option | Effect | Design Implication |
|--------|--------|-------------------|
| Gates extraction | Can't extract below X% confidence | Hard gate — frustrating? |
| Affects efficiency | Low confidence = lower extraction multiplier | Soft gate — natural incentive |
| Visual only | Player info, no mechanical effect | Low stakes — boring? |
| Affects survey progress | Confidence modifies how much survey progress each result contributes | Integrated — probably best |

## Display Design

[?] How is confidence shown to the player?

- Per-cell color overlay on grid?
- Numeric percentage next to element readings?
- Confidence bars per element in sample view?
- Aggregate confidence badge per cell (stars? letter grade?)

## Open Questions

- Should confidence decay over time? (instrument drift, environmental change)
- Does the AI/default mode produce systematically lower confidence than manual?
- Can the player see confidence breakdowns (which tool contributed how much)?
- Is there a "verification" action (like QA/QC standards) that boosts confidence?
