# Farming Unit — Design

**Status:** MATURATION (pre-design ideation in progress)
**Git branch:** `claude/farming-unit-design-4rz536`

Entry point for the farming unit's design. This module is currently in
**branch maturation** — raw ideas are being grown into a navigable
concept tree before the standard master-design doc is written. Start
with the tree.

## Documents

| Document | Status | Role |
|---|---|---|
| [`maturation/CONCEPT_TREE.md`](maturation/CONCEPT_TREE.md) | DRAFT | **The reference point.** Eight coded major nodes (A CHARTER · B MEDIA · C REGISTER · D ARRAY · E FORM · F BIOLOGY · G STAKES · H CROPS); navigate by code ("focus B2"). |
| [`maturation/SOURCES.md`](maturation/SOURCES.md) | REFERENCE | Raw inputs: the reMarkable "uniform definition" note, the six constraint programs (PHAROS…VAULT), planter-tool concept images, code state. |
| [`maturation/B2-hydroponics.md`](maturation/B2-hydroponics.md) | DRAFT | **Node B2 deep dive** — technique families, solution chemistry, root-zone oxygen, low-g drainage, light regime, atmosphere, failure latency; units, tier arc, decision texture. |
| [`maturation/B3-media-roster.md`](maturation/B3-media-roster.md) | DRAFT | **Node B3** — thirteen cultivation media on two axes (trophic route × loop closed), the portfolio rule, and the program→portfolio map. |
| [`maturation/media_map.html`](maturation/media_map.html) · [`.png`](maturation/media_map.png) | — | The medium roster as the buffer↔speed frontier. |
| [`maturation/H-crops.md`](maturation/H-crops.md) | DRAFT | **Branch H** — `CropDescriptor`, the computed medium-fit rule, crop roles, the diet ledger, seed stock, pathogens, and a 17-crop preliminary catalog. |
| [`maturation/crop_medium_matrix.html`](maturation/crop_medium_matrix.html) · [`.png`](maturation/crop_medium_matrix.png) | — | Crop × medium fit, and why each refusal happens. |
| [`maturation/BRANCH_MATURATION.md`](maturation/BRANCH_MATURATION.md) | DRAFT | The branch-maturation method itself: node grammar, states, navigation verbs (focus / graft / prune / park / harvest / mix). |
| [`maturation/concept_tree.html`](maturation/concept_tree.html) · [`.png`](maturation/concept_tree.png) | — | The tree as a visual, for opening beside a session. |
| `farming-master-design.md` | — | **Not yet written** — harvested from ◆ nodes when maturation settles. |

## The eight majors at a glance

```
A CHARTER   uniform UnitDescriptor (draws/gates/yields/cycle/medium/couplings)
B MEDIA     13 media, 4 trophic families; buffer<->speed is the spine
C REGISTER  the implicit farming algorithm (six constraint programs)
D ARRAY     spatial plot gameplay — placement, flows, staggered cycles
E FORM      module evolution: stations serving the array + lens model
F BIOLOGY   fungi & microbe additives, nutrients, waste loops
G STAKES    colony coupling: O2, water, power, crew morale, food ledger
H CROPS     the catalog; medium fit is computed from root traits, not authored
```

## Cross-references

- **Code:** `src/Unit/unit.cpp` (`ProcessFarming`,
  `InitializeFarmingModules`), `src/game_constants.h`
  (`FARMING_PRODUCTION_COSTS`), `game_types.toml` (farming module data),
  `resource_types.h` (FOOD, WATER, BIOFUEL).
- **Guides:** `docs/guides/module-architecture.md` (the 13-aspect brief —
  the tree deliberately covers its ground before code),
  `docs/guides/ui-panels.md`, `docs/design/ai-automation/README.md`.
- **Related design:** `docs/design/biomining/README.md` (parked seed
  spun out of node F5), `docs/design/prospecting/` (reference
  implementation of the design method).
- **Roadmap:** farming sits in Phase 3 (Advanced Production) of
  `ROADMAP_OVERALL.md`; this maturation is its Phase-0-style design
  groundwork.
