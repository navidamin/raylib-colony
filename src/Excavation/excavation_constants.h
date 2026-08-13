#pragma once

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
