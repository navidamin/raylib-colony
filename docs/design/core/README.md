# Core Unit — Design Documents

> **Auto-context rule:** When working on Core/habitat code (`Sect::core`, crew
> and life-support logic, the centre dome in `Sect::DrawInSectView`, or the Core
> module panels in `rendermanager.cpp`), read this README first to load design
> context.

> **Build status:** the Core's five modules exist and are reachable
> (`Unit::InitializeCoreModules`), but no sect constructs a Core unit yet.
> `Sect::CreateInitialUnits` builds eight production units around the dome and
> still assigns Extraction to `Sect::core`; the centre dome is decoration.
> Wiring the Core in means changing that roster and giving the dome its own
> unit — a change the sect view's socket ring has to absorb. Until then this
> document describes the intended design, not the running game.

## Table of Contents

| # | Document | Description | Status |
|---|----------|-------------|--------|
| 1 | [core-master-design.md](core-master-design.md) | Full design against the 13-aspect brief: loop, contract, units, tiers, economy, scale, hero visual, AI hook | DRAFT — designed, not built |

## Design Summary

The Core is the sect's centre dome, and the only unit that is always present.

> **The dome converts life support into labour.**

Everything in the sect's ring runs on crew. The Core makes crew exist, keeps
them alive, and decides what they work on. It is a **cycle**, not a production
chain — the ring feeds the dome consumables, the dome feeds the ring people:

```
   [Life Support] ──▶ habitable volume ──▶ [Crew] ──▶ [Roster] ──▶ labour
          ▲                                                          │
          └──────────── FOOD, WATER, O2, ENERGY ◀── [Ring units] ◀───┘
```

### The five modules

| Module | Owns | Decision it creates |
|---|---|---|
| **Life Support** | O₂/water/CO₂ loop closure | Invest in recycling vs haul raw supply |
| **Roster** | Crew, specialists, assignment | Who works where; where scarce specialists go |
| **Command** | Standing orders, unit priority | What the sect does unattended |
| **Monitoring** | Visibility into sect state | Whether you learn a sect stalled, or find out later |
| **Safety** | Dose, shelter, medical | Shelter and lose output, or push through and take dose |

### Sect structure

```
        ┌─ 1 Core (dome, centre, always present)
Sect ───┤
        └─ 6 sockets in a ring at 60°, chosen from 7 production types
```

Seven production types into six sockets means every sect is missing at least one
thing **by construction**. The transport network is load-bearing by design
rather than by balance tuning.

### Central tension

More crew runs more ring units — but more crew burns more life support, which
costs the very sockets you wanted for production. The sect must decide how much
of itself to spend staying alive. Self-limiting, no tuning knob.

### Scale constraint (read before touching Roster)

Crew split into **complement** (bulk, a count, never individually managed) and
**specialists** (few, named, billets gated by dome tier). Twenty sects at thirty
crew is six hundred people; only the specialists are ever managed by hand, so
the decision count stays bounded as the colony grows. See master design §11.

## Design History

This unit replaced a proposed **Communication** unit, which was cut. The
reasoning is worth keeping:

- With crew on site, an Earth link commands nothing — the crew *are* mission
  control. Earth-side latency and bandwidth gate no decision.
- Within a colony, surface radio is line-of-sight (no ionosphere; the horizon
  from a 2 m antenna is ~2.4 km), so a spread-out colony genuinely needs masts —
  but a mast is built once and then works. Infrastructure, not a mechanic.
- The rule that emerged: **comms only matters where the crew isn't.** That
  leaves expeditionary rovers beyond the horizon, which belongs to Prospecting's
  reach (`PROSPECTING_REACH_PER_TIER`), not to a unit with five modules.
- The one salvageable idea — information as a resource — became the
  **Monitoring** module here.

**Safety was also considered as its own unit and rejected.** Every ring unit
converts inputs into outputs; Safety produces nothing. It is a support function
about the crew, and the crew live in the dome — the storm shelter is physically
part of the habitat. Keeping it here also bounds its scale: solar events are
colony-wide, so it is one event and one decision, not twenty popups.

## Cross-References

### Source Code (current state — Core is not yet implemented)

| File | Relevance |
|------|-----------|
| `src/Sect/sect.h` | `Unit* core` — currently a barely-used pointer; the dome has no meaning yet |
| `src/Sect/sect.cpp` | `CreateInitialUnits` (authoritative unit list); `DrawInSectView` ring layout, hardcoded `i * 45.0f` for 8 units |
| `src/Unit/unit.cpp` | `InitializeStorage` — FOOD, WATER, O2, MANPOWER are initialised and **never consumed by anything** |
| `src/game_enums.h` | `UnitType` — vestigial for instantiation; units are constructed from strings |

### Gap this closes

Nothing in the codebase consumes `FOOD`, `MANPOWER`, or `O2`. Farming produces
food into a void and `MANPOWER` never moves. The Core is the missing sink that
makes the life-support resource set mean anything.

### Related Design Docs

| Document | Relationship |
|----------|--------------|
| [`../ai-automation/README.md`](../ai-automation/README.md) | AI tree pattern; Core's tree automates assignment and shelter decisions |
| [`../prospecting/README.md`](../prospecting/README.md) | Reference implementation for module structure; also the natural home for expeditionary rovers |
| [`../../guides/module-architecture.md`](../../guides/module-architecture.md) | The 13-aspect brief this design works through |
| [`../ui/sprite-manifest.md`](../ui/sprite-manifest.md) | Hero visual asset planning; Core's dome cutaway is not yet listed |

## Open Questions

Five are tracked in the master design. The two that most affect implementation
order:

1. **Crew loss vs degradation** on life-support failure — leaning toward
   recoverable incapacitation, since the project guide prefers missed-opportunity
   penalties over punishment.
2. **Specialist skill levels** on top of specialty — leaning toward starting
   without them, and adding only if assignments feel obvious.
