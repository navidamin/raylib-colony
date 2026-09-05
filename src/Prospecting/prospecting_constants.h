#pragma once

#include <algorithm>

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
// 32x32 (playtest request: "double the precision of the layer planes",
// after 8 -> 16 the round before). Metric constants (halo ranges, energy per
// metre, layer thickness) survive unchanged because they were always in
// metres; only per-cell tonnage, the reach rings and the face-support range
// follow the cell size. Ledger: docs/design/prospecting/drill-tuning.md.
constexpr int PROSPECTING_GRID_SIZE = 32;
constexpr int PROSPECTING_MAX_GRID_SIZE = PROSPECTING_GRID_SIZE;

// Side length of the reachable square per tier, centred in the grid.
//
// PROSPECTING no longer uses this -- its reach is ungated (a wall where a
// price should be; see docs/design/prospecting/progression-design.md #1).
// The table and IsSubCellInReach stay because EXCAVATION still reads reach
// with its own tier: hauling distance is a different question from where an
// instrument may look.
constexpr int PROSPECTING_REACH_PER_TIER[] = { 8, 16, 24, 32 };

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
constexpr float SUBCELL_SIZE_M          = 3.125f;  // 100 m cell / 32 sub-cells

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

// Metric layer boundaries, derived -- never restate the thickness table.
constexpr float LayerTopM(int depthIndex)
{
    float top = 0.0f;
    for (int d = 0; d < depthIndex && d < 4; d++)
    {
        top += LAYER_THICKNESS_M[d];
    }
    return top;
}
constexpr float LayerBottomM(int depthIndex)
{
    return LayerTopM(depthIndex) + LAYER_THICKNESS_M[depthIndex];
}
constexpr float FULL_COLUMN_M = LayerBottomM(3);

// A plate is a PLANE at ONE depth. Every cell on it stands for the same z.
//
// It used to be a SLAB: the iso rows (i+j, the axis running into the screen)
// spanned the stratum top to bottom, so moving the pointer across a plane
// walked the depth marker down the rock column. That made the screen axis
// running away from the viewer read as depth, which it is not -- the stack
// is exploded precisely so that depth is the axis BETWEEN plates. Playtest
// verdict: "the z is z. the z should change in between the 4 horizontal
// planes, not within each plane."
//
// Two depths, because a plate answers two different questions.
//
// WHERE THE PLANE IS. The plate is the TOP FACE of its stratum, so its plane
// sits at that stratum's top interface and its rock hangs below it -- which
// is how the borehole strip is now laid out too, each band starting at its
// own plate (ProsDockFrom). The centre was tried first and read wrong: a
// plate drawn mid-band looks like it is floating inside rock that is partly
// above it.
//
// This is what the correspondence cursor reads, and deliberately nothing
// else. Coring is NOT credited here: the line reaches the cell the player
// clicked at its TARGET depth, so crediting the core where the line crosses
// the plane instead would land it a cell or two short of the one they picked
// -- the same class of bug as a collar drawn off its clicked block. Tried
// that way first; the crossing-cell test caught it.
inline float PlatePlaneM(int layer)
{
    return LayerTopM(layer < 0 ? 0 : (layer > 3 ? 3 : layer));
}
// HOW DEEP A HOLE AIMED AT IT GOES. Reaching a plate's plane is not the same
// as sampling the rock under it: a hole that stopped at 68 m would touch
// basalt without ever cutting it, and basalt is what the whole drill campaign
// is tuned against. So aiming at a plate drills INTO that stratum, to its
// centre -- targets 6 / 23 / 51 / 94 m. It must stay strictly inside the
// stratum whatever else changes: the trace draws its end on the plate for
// LayerOfDepthM(endM), so a depth on the boundary would put the end of the
// line on the wrong plate.
inline float PlateTargetM(int layer)
{
    return LAYER_CENTRE_M[layer < 0 ? 0 : (layer > 3 ? 3 : layer)];
}

// Energy to an arbitrary metre depth: full layers above plus the partial one.
inline float DrillEnergyToDepthMetres(float m)
{
    float total = 0.0f;
    for (int d = 0; d < 4; d++)
    {
        float top = LayerTopM(d);
        if (m <= top) break;
        float span = std::min(m - top, LAYER_THICKNESS_M[d]);
        total += span * DRILL_ENERGY_PER_METRE[d];
    }
    return total;
}

inline int LayerOfDepthM(float m)
{
    for (int d = 0; d < 3; d++)
    {
        if (m < LayerBottomM(d)) return d;
    }
    return 3;
}

// ---------------------------------------------------------------------------
// Line holes (the prescribed line -- the drill-dock port).
// The string advances in metres of hole per second of game time, by the
// stratum being cut: soft ground runs quick, basalt crawls. Depth is priced
// in time as well as energy, and the player watches it happen.
// ---------------------------------------------------------------------------
// Rates are chosen against the DOCK, not just the ground: the strip draws
// every stratum at (near) equal band height while thicknesses run 12..52 m,
// so equal m/s would make the bit visually slam to a crawl at each seam.
// These give band traverse times of ~6/8/10/14 s at full spindle -- the
// on-screen speed steps down gently (x0.7-0.8 per seam) instead of x0.3.
// Hard rock's real cost is heat (pecks), not a wall of slowness.
constexpr float DRILL_ADVANCE_MPS[4] = { 2.0f, 2.75f, 3.4f, 3.7f };

// Heat (Dark Plating section 4.5, the redline prototype's model, tool-side
// only): heat climbs with the hardness of the rock being cut and bleeds at a
// flat rate. At DRILL_HEAT_MAX the string auto-pecks -- it dwells off the
// face until DRILL_HEAT_RESUME -- so hard ground costs TIME, never the run.
// Broken equipment stays the lesser penalty (drilling-procedure.md Rule 1).
constexpr float DRILL_HEAT_GAIN   = 0.50f;   // x rpm x (0.35 + hard * 0.75) per second
constexpr float DRILL_HEAT_BLEED  = 0.15f;   // while cutting
constexpr float DRILL_HEAT_COOL   = 0.25f;   // while dwelling or idle
constexpr float DRILL_HEAT_MAX    = 1.0f;
constexpr float DRILL_HEAT_RESUME = 0.45f;

// Stratum hardness at each depth layer, matching the strata the borehole
// dock draws (regolith, megaregolith, fractured, intact basalt).
constexpr float LAYER_HARDNESS[4] = { 0.25f, 0.55f, 0.45f, 0.95f };

// The spindle (redline's clicking, in the game): the string always turns at
// an idle crawl, and CLICKING THE BOREHOLE kicks it. Advance scales with
// rpm, and so does heat -- driving hard through basalt cooks the bit into
// its auto-peck, which is the whole hands-on tension. AUTO (never clicking)
// still finishes every hole, just slowly: hands-on is ceiling, not floor.
// Tuned by campaign (docs/design/prospecting/drill-tuning.md): idle is a
// bare crawl -- the clicks ARE the drill. Steady clicking at f/s holds
// roughly IDLE + KICK x f x TAU: 2/s cruises at ~0.44, 4/s at ~0.73, and
// only a furious ~6/s pins the cap. At idle the bit never outruns its own
// heat bleed, so an untouched hole finishes cold -- just very slowly.
constexpr float DRILL_RPM_IDLE = 0.15f;
constexpr float DRILL_RPM_MAX  = 1.0f;
constexpr float DRILL_RPM_KICK = 0.12f;
constexpr float DRILL_RPM_TAU  = 1.2f;       // seconds, decay back to idle

// A drill-side value (advance rate, hardness) read at depth m, blended
// linearly across DRILL_BLEND_M of each stratum seam. Without this the cut
// changes as a step the instant the bit crosses a border -- rate cliff plus
// a heat spike that dwelled right on the seam, which read as a synthetic
// full stop at every plate boundary.
constexpr float DRILL_BLEND_M = 6.0f;
inline float DrillBlendAtM(float m, const float v[4])
{
    int L = LayerOfDepthM(m);
    float half = DRILL_BLEND_M * 0.5f;
    float top = LayerTopM(L);
    if (L > 0 && m < top + half)
    {
        float t = 0.5f + (m - top) / DRILL_BLEND_M;      // 0.5 .. 1 past the seam
        return v[L - 1] + (v[L] - v[L - 1]) * t;
    }
    float bot = LayerBottomM(L);
    if (L < 3 && m > bot - half)
    {
        float t = (m - (bot - half)) / DRILL_BLEND_M;    // 0 .. 0.5 into the seam
        return v[L] + (v[L + 1] - v[L]) * t;
    }
    return v[L];
}
inline float DrillAdvanceAtM(float m)  { return DrillBlendAtM(m, DRILL_ADVANCE_MPS); }
inline float DrillHardnessAtM(float m) { return DrillBlendAtM(m, LAYER_HARDNESS); }

// The bit: wear, fracture, and the trip (redline-disposition.md section 4,
// Tier 3 -- broken equipment is the LESSER penalty: it buys a TRIP, never an
// ending). Wear runs 0 -> 1 through two channels:
//
//   abrasion -- metres cut, scaled by hardness ("bits dull with metres cut,
//   faster in hard rock"):        dW = adv_m * hard * BIT_WEAR_PER_M
//
//   thermal fatigue -- TIME AT TEMPERATURE, quadratic above an onset. This
//   is what fractures a bit driven too hot for too long, and it accrues
//   whether or not the bit is advancing -- hot is hot:
//       dW/dt = BIT_FATIGUE_RATE * ((heat - ONSET) / (1 - ONSET))^2
//
// At wear 1.0 the bit FRACTURES and the string trips: out rod by rod and
// back, BIT_TRIP_BASE_S + depth * BIT_TRIP_S_PER_M seconds, nothing
// advancing, the bit cooling in the open; it returns fresh (wear 0). Depth
// pricing the trip is the push-your-luck the disposition doc names: driving
// hot at 80 m is a gamble, at 20 m an errand. Campaign numbers in
// docs/design/prospecting/drill-tuning.md.
constexpr float BIT_WEAR_PER_M    = 0.008f;
constexpr float BIT_FATIGUE_ONSET = 0.60f;
constexpr float BIT_FATIGUE_RATE  = 0.055f;
// Retuned when a plate became one depth: the deepest hole went from ~79 m to
// 94 m (PlateDepthM), and at 0.30 s/m a trip cost 31 s while driving hard
// only saved ~12. Pushing the redline was then STRICTLY DOMINATED -- a trap,
// not a gamble -- which is the inversion an earlier round had already fixed
// once at the shallower depth. Repricing the trip restores the bargain:
// hard driving still fractures, and still wins on the clock.
constexpr float BIT_TRIP_BASE_S   = 3.0f;
constexpr float BIT_TRIP_S_PER_M  = 0.12f;

// Finishing a hole hoists the string back out of it. Same winch as a trip,
// but one direction only and with nothing to re-seat on the face, so it is a
// fraction of the trip's cost -- and unlike a trip it is a BEAT, not a price:
// the hole is already paid for and its knowledge already landed. Long enough
// to watch the rods come up (~5 s at 79 m), short enough that the next hole
// is never waiting on the animation.
// ---------------------------------------------------------------------------
// Block model focus
// ---------------------------------------------------------------------------
// Four plates of data at once is more than anyone reads at once. The SURFACE
// plate is pinned lit -- it is the one holes are collared on, and the one
// that answers "where am I" -- while the three below it rest dim and rise to
// full when the pointer is on them. Dim means RECEDE, not hide: at these
// values class colour is still legible, it just stops competing.
//
// The rest values keep a slight gradient with depth so the stack still reads
// as depth when nothing is hovered (further away is dimmer, which is the one
// cue an exploded iso stack has left).
constexpr float PLATE_LIGHT_FULL    = 1.0f;
constexpr float PLATE_REST_LIGHT[4] = { 1.0f, 0.50f, 0.44f, 0.38f };
// Time constant of the rise and fall. An exponential needs about 3 tau to
// arrive, so this is ~0.14 s to full: long enough to read as a light coming
// up rather than a state flipping, short enough to still feel like the
// pointer did it (past ~0.15 s a hover response starts to feel laggy).
constexpr float PLATE_LIGHT_TAU_S   = 0.045f;

constexpr float DRILL_PULL_BASE_S  = 1.2f;
constexpr float DRILL_PULL_S_PER_M = 0.045f;
inline float DrillPullSeconds(float depthM)
{
    return DRILL_PULL_BASE_S + depthM * DRILL_PULL_S_PER_M;
}

// The core log's fine intervals (redline's log, finer than the strata).
// Each 5 m stick is graded by the thermal DOSE it was cut under -- mean
// squared excess above the fatigue onset, per metre cut, so a stick that
// merely brushed the redline once and one that rode it read the same
// sustained level (worst-instant grading made the auto-peck sawtooth
// alternate LOST/PARTIAL stick by stick, which read as noise):
//   dose <  PROS_LOG_SMOKE_DOSE -> INTACT   core in the barrel
//   dose >= PROS_LOG_SMOKE_DOSE -> PARTIAL  smoked, half the story
//   the bit fractured in it    -> LOST     rubble where the core was
// The grade is a record, not yet a survey term (redline-disposition.md
// section 5 names that blocker); the lane tells the truth ahead of it.
// Sticks are counted PER STRATUM, not per fixed metre. A fixed 5 m stick was
// drawn through the borehole strip's mapping, where every stratum gets a
// near-equal band while holding 12/22/34/52 m -- so a stick's pixel height
// swung 4x with depth, and a lane drawn on its own linear scale instead ran
// up to ~84 px BEHIND the bit mid-column (measured). Equal sticks per band
// satisfies both: uniform on screen AND level with the string, because the
// strip is linear within a band. The metre-length of a stick then varies by
// unit (2.0 / 3.7 / 5.7 / 8.7 m), which is how a real log is cut anyway --
// the sample interval belongs to the unit, not to the tape.
constexpr float PROS_LOG_SMOKE_DOSE = 0.0005f;  // in effect: any hot metre smokes the stick
constexpr int   PROS_LOG_PER_LAYER = 6;
constexpr int   PROS_LOG_INTERVALS = 4 * PROS_LOG_PER_LAYER;

// Metre span of one stick, and the stick a depth falls in. Layer boundaries
// are stick boundaries by construction, so no stick ever straddles a seam.
inline float ProsLogTopM(int iv)
{
    int L = std::clamp(iv / PROS_LOG_PER_LAYER, 0, 3);
    int k = iv - L * PROS_LOG_PER_LAYER;
    return LayerTopM(L) + LAYER_THICKNESS_M[L] * static_cast<float>(k)
                          / static_cast<float>(PROS_LOG_PER_LAYER);
}
inline float ProsLogBottomM(int iv)
{
    int L = std::clamp(iv / PROS_LOG_PER_LAYER, 0, 3);
    return ProsLogTopM(iv) + LAYER_THICKNESS_M[L]
                             / static_cast<float>(PROS_LOG_PER_LAYER);
}
inline int ProsLogIndexOfDepth(float m)
{
    int L = LayerOfDepthM(m);
    float t = (m - LayerTopM(L)) / LAYER_THICKNESS_M[L];
    int k = static_cast<int>(t * static_cast<float>(PROS_LOG_PER_LAYER));
    k = std::clamp(k, 0, PROS_LOG_PER_LAYER - 1);
    return L * PROS_LOG_PER_LAYER + k;
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
// Scales with the face: a working spot is one sub-cell across, so when the
// lattice refined 12.5 -> 6.25 m this halved with it. At 5 m the immediate
// neighbour of a dug spot reads support 0.21 (Inferred) -- the same ladder
// the 8x8 lattice had -- and colony_sim's survey-beats-blind claim holds.
// At the old 10 m every neighbour of every dug spot went Indicated and
// blind digging self-mapped its way past the surveyor. Halved again with the
// 6.25 -> 3.125 m lattice, same rule, re-measured by colony_sim.
constexpr float EXCAVATION_SUPPORT_RANGE_M = 2.0f;


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
// A fraction of the LATTICE, so it must scale with the lattice: the physical
// ground a hole speaks for is metric (its halo), and holding this at 0.25
// after the 16x16 refinement would have quietly demanded 4x the holes for
// the same progress -- which is exactly what flipped colony_sim's
// "surveying beats digging blind" claim until this was scaled.
constexpr float SURVEY_SAMPLE_COVERAGE_TARGET =
    0.25f * (8.0f * 8.0f) / (PROSPECTING_GRID_SIZE * PROSPECTING_GRID_SIZE);

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
