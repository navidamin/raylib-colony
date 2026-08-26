#pragma once

// Sub-cell grid sizes per prospecting module tier
// The prospecting grid is a FIXED lattice of sub-cells covering the parent
// planet cell. Sub-cell size never changes; tier extends how far the
// instruments reach from the sect at the centre, as concentric rings:
//
//   T0: core 2x2 (4 cells,   6%)   T2: 6x6 (36 cells,  56%)
//   T1: 4x4     (16 cells,  25%)   T3: 8x8 (64 cells, 100%)
//
// Even sizes nest perfectly (offsets 3/2/1/0), so every tier-up lights up a
// complete ring. Because the grid is never reallocated, survey data and
// collected samples survive a tier upgrade.
constexpr int PROSPECTING_GRID_SIZE = 8;
constexpr int PROSPECTING_MAX_GRID_SIZE = 8;

// Side length of the reachable square per tier, centred in the grid.
//
// PROSPECTING no longer uses this -- its reach is ungated (a wall where a
// price should be; see docs/design/prospecting/progression-design.md #1).
// The table and IsSubCellInReach stay because EXCAVATION still reads reach
// with its own tier: hauling distance is a different question from where an
// instrument may look.
constexpr int PROSPECTING_REACH_PER_TIER[] = { 2, 4, 6, 8 };

// Sample tray base capacities per tier (before objective bonuses)
constexpr int TRAY_BASE_CAPACITY[] = { 4, 8, 12, 16 };
constexpr int TRAY_MAX_CAPACITY = 20;

// Lab bench concurrent processing slots per tier
constexpr int LAB_BENCH_SLOTS[] = { 1, 2, 3, 4 };

// Maximum accessible depth layers per tier.
//
// UNGATED: all four layers are drillable from the first minute, and depth is
// priced per metre instead of being walled (progression-design.md). The table
// shape survives only so its many call sites need no churn; every value is 4.
// Excavation has its own EXC_MAX_DEPTH_PER_TIER and is untouched.
constexpr int MAX_DEPTH_PER_TIER[] = { 4, 4, 4, 4 };

// ---------------------------------------------------------------------------
// Depth geometry and drilling cost (per metre, never discounted)
//
// Layers get thicker with depth -- geologically honest, and it makes deep
// ground worth more in absolute tonnage exactly where it is hardest to know.
// A hole is priced by the metres it passes through; there is no per-hole
// price and no tier discount, which is what keeps "bank energy and drill
// after the upgrade" from ever being correct.
// Placeholder numbers, to be calibrated against dumped data (the repo rule).
// ---------------------------------------------------------------------------
constexpr float LAYER_THICKNESS_M[4]    = { 12.0f, 22.0f, 34.0f, 52.0f };
constexpr float LAYER_CENTRE_M[4]       = {  6.0f, 23.0f, 51.0f, 94.0f };
constexpr float DRILL_ENERGY_PER_METRE[4] = { 1.2f, 1.9f, 2.8f, 4.0f };
constexpr float SUBCELL_SIZE_M          = 12.5f;   // 100 m cell / 8 sub-cells

// Energy for a vertical hole from the surface down THROUGH depth layer d --
// the auger cores everything above its target, so the cost is the whole
// column, not the target layer alone.
constexpr float DrillEnergyToDepth(int depthIndex)
{
    float total = 0.0f;
    for (int d = 0; d <= depthIndex && d < 4; d++)
    {
        total += LAYER_THICKNESS_M[d] * DRILL_ENERGY_PER_METRE[d];
    }
    return total;
}

// ---------------------------------------------------------------------------
// The estimate field (block-model-design.md #3)
//
// Support -- how far one core may honestly speak -- comes from the NEAREST
// core, never a weight sum: distant samples must not out-vote the prior.
// RANGE is the geological continuity distance. At 20 m the lattice falls out
// beautifully: the cored block reads 1.0 (the rock is in your hand), the
// 12.5 m neighbour 0.68 (INDICATED), two cells at 25 m 0.21 (INFERRED), and
// three cells 0.03 (UNCLASSIFIED). Adjacent depth layers step 0.49 / 0.14 /
// 0.01 -- deep ground stays a bet by arithmetic, not by rule.
// ---------------------------------------------------------------------------
constexpr float ESTIMATE_RANGE_M = 20.0f;
constexpr float ESTIMATE_IDW_POWER = 3.0f;

// A working face also teaches -- standing in the void you can see the rock
// around it -- but at HALF a core's reach. This number was set by a failure,
// not a guess: with dug spots given the full 20 m halo, colony_sim measured
// blind digging becoming self-mapping enough to beat the surveyor, which is
// the exact "prospecting becomes optional past the first pit" risk
// module-interplay.md #5 warned about. At 10 m a dug spot's neighbour reads
// ~0.21 (barely INFERRED): mining outward still hints, surveying still wins.
constexpr float EXCAVATION_SUPPORT_RANGE_M = 10.0f;


// Sweep energy costs per frequency band (high → low frequency)
constexpr float SWEEP_ENERGY_COST[] = { 30.0f, 60.0f, 100.0f, 150.0f };
constexpr int SWEEP_FREQUENCY_BANDS = 4;

// Depth penetration per frequency band (number of layers revealed)
constexpr int SWEEP_DEPTH_PENETRATION[] = { 1, 2, 3, 4 };

// Confidence display thresholds (upper bound of each tier)
constexpr float CONFIDENCE_THRESHOLD_LOW      = 0.20f;
constexpr float CONFIDENCE_THRESHOLD_MODERATE  = 0.40f;
constexpr float CONFIDENCE_THRESHOLD_HIGH      = 0.60f;
constexpr float CONFIDENCE_THRESHOLD_CERTAIN   = 0.80f;

// Confidence-scaled multipliers for survey progress accumulation
constexpr float CONFIDENCE_SURVEY_MULTIPLIER[] = { 0.2f, 0.4f, 0.7f, 0.9f, 1.0f };

// Survey progress component max contributions
// How the three routes to knowledge divide survey progress.
//
// TESTING is retired. It measured how far a core had been through the lab,
// and the lab is gone: a recovered core is rock you are holding, so it comes
// out of the ground assayed. Its 0.30 folds into sampling rather than being
// deleted, or a fully cored cell would top out at 0.70 forever -- which would
// have silently capped extraction efficiency and looked like a balance issue
// rather than a leftover.
constexpr float SURVEY_SWEEP_WEIGHT   = 0.20f;
constexpr float SURVEY_SAMPLE_WEIGHT  = 0.80f;
constexpr float SURVEY_TESTING_WEIGHT = 0.00f;

// Fraction of sub-cells that must be sampled for full sample coverage
constexpr float SURVEY_SAMPLE_COVERAGE_TARGET = 0.25f;

// Survey progress threshold for marking a site for excavation
constexpr float MARKED_SITE_THRESHOLD = 0.60f;

// AI confidence penalties per tier
constexpr float AI_CONFIDENCE_PENALTY[] = { 0.20f, 0.15f, 0.10f, 0.05f };

// Re-sampling diminishing returns: gain(n) = BASE_GAIN / sqrt(n)
constexpr float RESAMPLE_BASE_GAIN = 0.15f;

// Minimum element abundance to include in cell aggregate confidence
constexpr float CELL_CONFIDENCE_MIN_ABUNDANCE = 0.05f;

// Cross-referencing survey bonus for adjacent cell analysis
constexpr float CROSS_REFERENCE_BONUS = 0.10f;

// Sub-cell resource spatial variation range (multiplier on parent cell value)
constexpr float SUBCELL_VARIATION_MIN = 0.3f;
constexpr float SUBCELL_VARIATION_MAX = 2.0f;

// Per-tool base confidence contributions
constexpr float CONFIDENCE_GPR_MIN          = 0.05f;
constexpr float CONFIDENCE_GPR_MAX          = 0.15f;
constexpr float CONFIDENCE_VISUAL_MIN       = 0.05f;
constexpr float CONFIDENCE_VISUAL_MAX       = 0.10f;
constexpr float CONFIDENCE_XRF_MIN          = 0.30f;
constexpr float CONFIDENCE_XRF_MAX          = 0.50f;
constexpr float CONFIDENCE_LIBS_MIN         = 0.20f;
constexpr float CONFIDENCE_LIBS_MAX         = 0.40f;
constexpr float CONFIDENCE_FIRE_ASSAY       = 1.00f;

// Crystal visual: probability of selecting primary shape family vs random
constexpr float CRYSTAL_PRIMARY_FAMILY_CHANCE = 0.70f;

// Crystal templates per shape family
constexpr int CRYSTAL_TEMPLATES_PER_FAMILY = 5;
constexpr int CRYSTAL_TOTAL_TEMPLATES = 20;

// --- Sweep mechanics ---

// Maximum frequency band accessible per tier (0 = band 0 only)
constexpr int MAX_SWEEP_BAND_PER_TIER[] = { 0, 1, 2, 3 };

// Sweep noise: base noise decreases with tier
constexpr float SWEEP_BASE_NOISE = 0.15f;
constexpr float SWEEP_NOISE_PER_TIER = 0.03f;

// Sweep noise: low frequency adds noise (blurrier deep signals)
constexpr float SWEEP_NOISE_PER_BAND = 0.04f;

// Sweep noise: poor calibration adds noise
constexpr float SWEEP_CALIBRATION_NOISE_WEIGHT = 0.10f;

// Spatial blur: neighbor blending weight per frequency band
constexpr float SWEEP_BLUR_PER_BAND = 0.15f;

// Depth signal attenuation: attenuation = 1 / (1 + depth * factor)
constexpr float SWEEP_DEPTH_ATTENUATION = 0.5f;

// How much each kind of evidence is worth when reconstructing what is known
// about a spot at a given depth. These describe what a sweep and a sample
// TELL you, which is a prospecting fact, so they live here and excavation
// aliases them -- one number, so the two modules cannot drift apart.
//
// Sweeps are broad and cheap; samples are local and definitive, so a sweep
// alone should never read as well-known.
constexpr float PROSPECT_SWEEP_CONFIDENCE_WEIGHT  = 0.6f;

// A sample taken at a spot+depth is direct evidence, so it carries full weight.
constexpr float PROSPECT_SAMPLE_CONFIDENCE_WEIGHT = 1.0f;

// A wide surface sweep may never, on its own, classify ground. You cannot put
// tonnage in a resource statement on the strength of a surface reading -- real
// resource codes forbid it, and it is also the better game: it keeps a
// permanent reason to drill. So sweep-derived confidence is capped just below
// the Inferred threshold. Combined with a core it still rises normally.
constexpr float PROSPECT_SWEEP_CONFIDENCE_CAP = CONFIDENCE_THRESHOLD_LOW - 0.01f;

// ---------------------------------------------------------------------------
// Resource classification
//
// The three named classes are a GROUPING of the CONFIDENCE_THRESHOLD_* bands
// above, never a second set of thresholds -- see GetResourceClass(). That is
// what keeps the coarse reading and the fine one from ever disagreeing.
//
//   Measured      > 0.80    CERTAIN
//   Indicated     > 0.40    MODERATE + HIGH
//   Inferred      > 0.20    LOW
//   Unclassified <= 0.20    VERY_LOW
// ---------------------------------------------------------------------------

// Anomaly detection: cells above mean + threshold * stddev
constexpr float SWEEP_ANOMALY_THRESHOLD = 1.5f;

// --- Sampling mechanics ---

// Lab tool energy costs
constexpr float LAB_TOOL_COST_VISUAL       = 5.0f;
constexpr float LAB_TOOL_COST_XRF          = 25.0f;
constexpr float LAB_TOOL_COST_OPTICAL      = 15.0f;
constexpr float LAB_TOOL_COST_MAGNETIC     = 20.0f;
constexpr float LAB_TOOL_COST_LIBS         = 40.0f;
constexpr float LAB_TOOL_COST_FIRE_ASSAY   = 80.0f;

// Separation energy costs
constexpr float LAB_SEPARATION_COST_MAGNETIC  = 30.0f;
constexpr float LAB_SEPARATION_COST_HEAVY     = 50.0f;
constexpr float LAB_SEPARATION_COST_VOLATILE  = 45.0f;

// Richness normalization: totalAbundance / factor, clamped to 0-1
// Absolute deposit quantity (summed across elements in one sub-cell layer)
// that counts as 100% richness. ResourceManager produces roughly 5,000 for an
// average cell layer and ~13,000 for a cluster core, before sub-cell variation
// (0.3x-2.0x), so 10,000 puts typical samples mid-range and leaves headroom
// for genuinely rich finds.
constexpr float RICHNESS_NORMALIZATION = 10000.0f;
