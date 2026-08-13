#pragma once

#include "excavation_types.h"

// Excavation tier tables and tuning constants.
//
// Capability lives here as constant tables rather than branching logic, per
// docs/guides/module-architecture.md Part II §3. Engines answer capability
// questions (CanReach, CanWorkDepth); the UI asks rather than deciding.

// ---------------------------------------------------------------------------
// Reach
// ---------------------------------------------------------------------------
// Excavation reads the same fixed 8x8 lattice prospecting builds, and the same
// concentric-ring reach helpers -- but with the EXCAVATION module's tier, not
// prospecting's. That is deliberate and is where the gamble lives: when
// excavation reaches further than prospecting, the outer rings can be dug but
// not surveyed. See docs/design/excavation/excavation-design.md §2.
//
// The ring sizes themselves are prospecting's (PROSPECTING_REACH_PER_TIER),
// so the two modules stay on one lattice with one notion of distance.

// ---------------------------------------------------------------------------
// Confidence derivation
// ---------------------------------------------------------------------------
// Prospecting stores ONE confidence per sub-cell (SubCell::aggregateConfidence),
// not one per depth. Excavation needs per-depth confidence (design Rule 2), so
// it derives a per-depth view from what prospecting already exposes:
//
//   sweep evidence   -- aggregateConfidence, attenuated by depth, and only for
//                       layers the swept frequency band actually penetrated
//   sample evidence  -- mean element confidence of samples taken at that exact
//                       spot AND depth
//
// The two are combined as independent evidence: 1 - (1-a)(1-b).
//
// If prospecting ever stores confidence per depth directly, SiteView::
// GetConfidence is the single place to change.

// Weight applied to sweep-derived confidence relative to sample-derived.
// Sweeps are broad and cheap; samples are local and definitive, so a sweep
// alone should never read as well-known.
constexpr float EXC_SWEEP_CONFIDENCE_WEIGHT = 0.6f;

// A sample taken at a spot+depth is direct evidence, so it carries full weight.
constexpr float EXC_SAMPLE_CONFIDENCE_WEIGHT = 1.0f;

// ---------------------------------------------------------------------------
// Estimates (the gamble)
// ---------------------------------------------------------------------------
// How wrong a completely unsurveyed reading can be, as a fraction of the true
// value. At confidence 0 a spot reads anywhere in truth x [0.4, 1.6]; at
// confidence 1 it reads exactly true. The bias within that band is a stable
// hash of the spot's coordinates, never a per-tick roll -- see
// EstimateEngine, Rule 1.
constexpr float EXC_MAX_ESTIMATE_SPREAD = 0.6f;

// Confidence at which the range is treated as closed and the spot reads as
// known. Slightly below 1.0 so a thoroughly surveyed spot does not sit at
// "almost certain" forever because of floating-point dust.
constexpr float EXC_CERTAIN_CONFIDENCE = 0.95f;

// ---------------------------------------------------------------------------
// Depth
// ---------------------------------------------------------------------------
// Deepest layer index each excavation tier can work. Machines narrow this
// further in Phase 3 -- this is the module-level ceiling.
constexpr int EXC_MAX_DEPTH_PER_TIER[] = { 1, 2, 3, 4 };

// ---------------------------------------------------------------------------
// Machines (Phase 3) -- count only, for now
// ---------------------------------------------------------------------------
// Machines all work the same spot; more machines means faster, same place.
constexpr int EXC_MACHINE_COUNT_PER_TIER[] = { 1, 2, 4, 8 };

// ---------------------------------------------------------------------------
// The machine table
// ---------------------------------------------------------------------------
// Capability lives here rather than in branching logic. Every machine differs
// on every axis, so none is a strict upgrade on another.
//
// The pair that matters most is BUCKET_WHEEL and BUCKET_DRUM: both arrive at
// tier 1, and they are opposites. The wheel is fast, blunt and indiscriminate;
// the drum is slow, precise and choosy. Which one is right depends entirely on
// whether the ground was surveyed -- a precise machine collects the payoff of a
// survey, while a blunt one averages your chosen spot with its neighbours and
// throws that payoff away. On unsurveyed ground the wheel is genuinely the
// better tool, because covering ground beats aiming when you cannot aim.
//
// Science behind the stats is in docs/design/excavation/excavation-mechanics.md
// Part 1: counter-rotating drums cancel their own reaction force (precise, very
// low wear, moderate pace); bucket wheels trade force for volume; percussion
// buys depth in hard ground at a steep energy and wear price; augers are
// surgical but slow; pneumatic ignores hardness and eats consumables.
constexpr Machine EXC_MACHINES[] = {
//   id                      name              depth  prec  pace  power  wear  select  tier  tech
    { MachineId::SCOOP,        "Scoop",           1,  0.55f, 0.7f, 0.5f, 0.6f, 0.15f,  0, ""                   },
    { MachineId::BUCKET_WHEEL, "Bucket Wheel",    2,  0.45f, 1.8f, 1.0f, 1.5f, 0.10f,  1, "MechanizedDrilling" },
    { MachineId::BUCKET_DRUM,  "Bucket Drum",     2,  0.90f, 1.0f, 1.0f, 0.4f, 0.60f,  1, "MechanizedDrilling" },
    { MachineId::PERCUSSIVE,   "Hammer",          4,  0.70f, 1.2f, 1.8f, 2.0f, 0.40f,  2, "HeavyEquipment"     },
    { MachineId::AUGER,        "Auger",           3,  0.98f, 0.6f, 0.7f, 1.0f, 0.85f,  2, "HeavyEquipment"     },
    { MachineId::PNEUMATIC,    "Blower",          1,  0.88f, 1.0f, 1.6f, 0.3f, 0.70f,  3, "AutonomousFleet"    },
};

constexpr int EXC_MACHINE_TABLE_SIZE =
    static_cast<int>(sizeof(EXC_MACHINES) / sizeof(EXC_MACHINES[0]));

// ---------------------------------------------------------------------------
// Digging
// ---------------------------------------------------------------------------
// Mass moved per tick by one machine at pace 1.0, before every other modifier.
// Calibrated against dumped data: a spot layer holds roughly 800-4,500 units at
// cell (5,5) and up to ~44,000 in rich cells (colony_inspect), and a spot
// should last on the order of a game day at tier 0.
constexpr float EXC_BASE_DIG_MASS = 12.0f;

// Fraction of a spot's total content removed per unit of mass taken. Spot
// content varies by orders of magnitude between cells, so depletion is tracked
// as a fraction of that spot rather than an absolute.
//
// Tuned so one spot lasts on the order of two to three game days at full pace
// (TICKS_PER_DAY is 20), which is long enough to feel like working a face and
// short enough that the worked-out area visibly spreads across the lattice.
constexpr float EXC_DEPLETION_PER_MASS = 0.001f;

// A nearly-empty spot yields less, but never nothing -- without this floor the
// taper is asymptotic and a spot never actually runs out.
constexpr float EXC_MIN_TAPER = 0.2f;

// How much of the surrounding waste a fully selective machine leaves in the
// ground. Never all of it -- some waste always comes along.
constexpr float EXC_MAX_WASTE_REJECTION = 0.8f;

// Pushing the pace costs selectivity: there is no time to be choosy. At full
// pace a machine loses this fraction of its selectivity.
constexpr float EXC_PACE_SELECTIVITY_PENALTY = 0.7f;

// Power drawn per unit of pace, on top of the machine's floor.
constexpr float EXC_POWER_PER_PACE = 1.2f;

// Hardness by depth: deeper ground is denser and more cohesive, so the same
// machine moves less of it. From the geotechnics in excavation-mechanics.md
// Part 1 section 8 -- bulk density climbs 1.30 to 1.92 g/cm3 over the first
// metre and cohesion rises with it.
constexpr float EXC_DEPTH_HARDNESS[] = { 1.0f, 0.85f, 0.7f, 0.5f };
