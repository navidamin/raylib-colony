#pragma once

// Sub-cell grid sizes per prospecting module tier
constexpr int PROSPECTING_GRID_SIZE[] = { 3, 4, 5, 6 };
constexpr int PROSPECTING_MAX_GRID_SIZE = 6;

// Sample tray base capacities per tier (before objective bonuses)
constexpr int TRAY_BASE_CAPACITY[] = { 4, 8, 12, 16 };
constexpr int TRAY_MAX_CAPACITY = 20;

// Lab bench concurrent processing slots per tier
constexpr int LAB_BENCH_SLOTS[] = { 1, 2, 3, 4 };

// Maximum accessible depth layers per tier
constexpr int MAX_DEPTH_PER_TIER[] = { 1, 2, 3, 4 };

// Drilling energy costs: DRILL_ENERGY_COST[tier][depthLayer]
// -1.0f = layer not accessible at this tier
constexpr float DRILL_ENERGY_COST[4][4] = {
    { 15.0f, -1.0f, -1.0f, -1.0f },
    { 12.0f, 30.0f, -1.0f, -1.0f },
    { 10.0f, 25.0f, 50.0f, -1.0f },
    {  8.0f, 20.0f, 35.0f, 75.0f },
};

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
constexpr float SURVEY_SWEEP_WEIGHT   = 0.20f;
constexpr float SURVEY_SAMPLE_WEIGHT  = 0.50f;
constexpr float SURVEY_TESTING_WEIGHT = 0.30f;

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
