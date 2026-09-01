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
| [`maturation/CONCEPT_TREE.md`](maturation/CONCEPT_TREE.md) | DRAFT | **The reference point.** Seven coded major nodes (A CHARTER · B MEDIA · C REGISTER · D ARRAY · E FORM · F BIOLOGY · G STAKES); navigate by code ("focus B2"). |
| [`maturation/SOURCES.md`](maturation/SOURCES.md) | REFERENCE | Raw inputs: the reMarkable "uniform definition" note, the six constraint programs (PHAROS…VAULT), planter-tool concept images, code state. |
| [`maturation/BRANCH_MATURATION.md`](maturation/BRANCH_MATURATION.md) | DRAFT | The branch-maturation method itself: node grammar, states, navigation verbs (focus / graft / prune / park / harvest / mix). |
| [`maturation/concept_tree.html`](maturation/concept_tree.html) · [`.png`](maturation/concept_tree.png) | — | The tree as a visual, for opening beside a session. |
| `farming-master-design.md` | — | **Not yet written** — harvested from ◆ nodes when maturation settles. |

## The seven majors at a glance

```
A CHARTER   uniform UnitDescriptor (draws/gates/yields/cycle/medium/couplings)
B MEDIA     cultivation substrates: SOIL / HYDRO / AERO / MYCO / VAT
C REGISTER  the implicit farming algorithm (six constraint programs)
D ARRAY     spatial plot gameplay — placement, flows, staggered cycles
E FORM      module evolution: stations serving the array + lens model
F BIOLOGY   fungi & microbe additives, nutrients, waste loops
G STAKES    colony coupling: O2, water, power, crew morale, food ledger
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
