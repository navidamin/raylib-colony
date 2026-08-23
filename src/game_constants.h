#ifndef GAME_CONSTANTS_H
#define GAME_CONSTANTS_H

#include <string>
#include <map>
#include <vector>
#include <iostream>
#include "resource_types.h"

const float SECT_CORE_RADIUS = 50.0f;
const int PLANET_SIZE = 20;  // 20x20 grid of possible sect locations
const float PLANET_WIDTH = PLANET_SIZE * SECT_CORE_RADIUS * 2.0f;  // Total width of planet
const float PLANET_HEIGHT = PLANET_SIZE * SECT_CORE_RADIUS * 2.0f; // Total height of planet

const float TICK_DURATION = 1.0f;
// TICKS_PER_DAY
static const int TICKS_PER_DAY = 20;  // 60 seconds = 1 day

const float INITIAL_UNIT_ENERGY = 1000.0f;
const float INITIAL_UNIT_FOOD = 1000.0f;
const float INITIAL_UNIT_WATER= 1000.0f;
const float INITIAL_UNIT_MANPOWER = 100.0f;
const float INITIAL_UNIT_SCIENCE = 20.0f;

const float DEFAULT_SECT_SHARE = 0.2f;

// Storage capacity constants
const float SECT_BASE_STORAGE = 1000.0f;
const float COLONY_BASE_RESERVES = 5000.0f;
const float STORAGE_SURPLUS_THRESHOLD = 0.8f;  // Push to colony when 80% full
const float STORAGE_DEFICIT_THRESHOLD = 0.1f;  // Pull from colony when below 10%
const float DEFICIT_REQUEST_AMOUNT = 0.3f;     // Request enough to fill to 30%

// Storage upgrade constants (levels 0-3)
const int MAX_STORAGE_LEVEL = 3;
const float STORAGE_LEVEL_MULTIPLIERS[] = {1.0f, 1.5f, 2.0f, 3.0f};

// Sect storage upgrade costs per level (Fe, Si, ENERGY)
const float SECT_UPGRADE_COST_FE[]     = {0.0f, 100.0f, 250.0f, 500.0f};
const float SECT_UPGRADE_COST_SI[]     = {0.0f,  50.0f, 150.0f, 300.0f};
const float SECT_UPGRADE_COST_ENERGY[] = {0.0f, 200.0f, 400.0f, 800.0f};

// Colony reserve upgrade costs per level (Fe, Si, ENERGY, ALLOYS)
const float COLONY_UPGRADE_COST_FE[]     = {0.0f, 500.0f, 1000.0f, 2000.0f};
const float COLONY_UPGRADE_COST_SI[]     = {0.0f, 250.0f,  500.0f, 1000.0f};
const float COLONY_UPGRADE_COST_ENERGY[] = {0.0f, 500.0f, 1000.0f, 2000.0f};

// Manpower constants (constant per sect)
const float SECT_BASE_MANPOWER = 10.0f;           // Fixed manpower available per sect

// Ambient energy constants
const float BASE_AMBIENT_ENERGY = 1.0f;           // Base energy from ambient sources per tick
const float SOLAR_PEAK_MULTIPLIER = 2.0f;         // Multiplier at solar noon
const float SOLAR_MIN_MULTIPLIER = 0.1f;          // Multiplier at night

// Transport constants
const float BASE_TRANSPORT_SPEED = 50.0f;         // Base units per second for transport packets
const float TRANSPORT_PACKET_SIZE = 100.0f;       // Max resource amount per transport packet
const float AUTO_BALANCE_THRESHOLD = 0.3f;        // Difference threshold to trigger auto-balance (30%)
const float MIN_TRANSPORT_INTERVAL = 3.0f;        // Minimum seconds between transport jobs on same road
const int MAX_PACKETS_PER_ROAD = 3;               // Maximum concurrent packets on a road

// Calibration constants
const float CALIBRATION_DRIFT_PER_SCAN = 0.02f;     // Quality loss per scan
const float CALIBRATION_MIN_QUALITY = 0.5f;          // Floor for calibration quality
const float CALIBRATION_DURATION = 30.0f;            // Seconds to calibrate
const float CALIBRATION_CUSTOM_NOISE_BONUS = 0.5f;   // 50% noise reduction for calibrated element

// Survey Progress constants
const float SURVEY_BASE_PROGRESS_T0 = 0.10f;         // T0: +10% base per scan
const float SURVEY_BASE_PROGRESS_T1 = 0.20f;         // T1: +20% base per scan
const float SURVEY_BASE_PROGRESS_T2 = 0.30f;         // T2: +30% base per scan
const float SURVEY_BASE_PROGRESS_T3 = 0.45f;         // T3: +45% base per scan
const float SURVEY_UNSCANNED_EFFICIENCY = 0.35f;      // Extraction efficiency with 0% survey
const float SURVEY_SCANNED_BONUS = 0.65f;             // Max additional efficiency from survey
const float SURVEY_MARKED_SITE_BONUS = 0.15f;         // Additive bonus for marked sites

// Campaign constants
const int CAMPAIGN_QUEUE_CAP_T2 = 10;                // Max queued cells at T2
const float CAMPAIGN_COMPLETION_CONFIDENCE = 0.05f;  // +5% confidence per completed campaign

// Objective constants
const float OBJECTIVE_THRESHOLD_BONUS = 0.25f;       // +25% extraction for Rich Vein
const float OBJECTIVE_THRESHOLD_DURATION = 5.0f;     // Days for Rich Vein bonus
const float OBJECTIVE_COVERAGE_BONUS = 0.05f;        // +5% permanent confidence
const float OBJECTIVE_GRADIENT_BONUS = 0.15f;        // +15% for gradient discovery
const float OBJECTIVE_GRADIENT_DURATION = 3.0f;      // Days for gradient bonus
const float OBJECTIVE_GRADIENT_THRESHOLD = 0.5f;     // 50% difference for gradient detection

const float DEFAULT_H2ExtractionRate = 0.03;
const float DEFAULT_O2ExtractionRate = 0.03;
const float DEFAULT_CExtractionRate = 0.03;
const float DEFAULT_FeExtractionRate= 0.03;
const float DEFAULT_SiExtractionRate= 0.03;
const float DEFAULT_ResourceFocus= 1;
const float DEFAULT_EnergyConsumption= 1;
const float DEFAULT_WearAndTear= 0.2;
const float DEFAULT_Efficiency= 0.75;
const float DEFAULT_StorageCapacity= 100;
const float DEFAULT_BreakdownChance= 0.02;

/*
enum class ResourceType {
    ENERGY,
    SCIENCE,
    MANPOWER,
    H2,
    O2,
    C,
    Fe,
    Si,
    WATER,
    FOOD
};
*/




const std::map<ResourceType, std::map<ResourceType, float>> EXTRACTION_PRODUCTION_COSTS = {
    {ResourceType::H2, {{ResourceType::ENERGY, 1.0f}}},
    {ResourceType::O2, {{ResourceType::ENERGY, 1.0f}}},
    {ResourceType::C,  {{ResourceType::ENERGY, 1.0f}}},
    {ResourceType::Fe, {{ResourceType::ENERGY, 1.0f}}},
    {ResourceType::Si, {{ResourceType::ENERGY, 1.0f}}}
};

// You can add more for other unit types:
const std::map<ResourceType, std::map<ResourceType, float>> FARMING_PRODUCTION_COSTS = {
    {ResourceType::FOOD, {
        {ResourceType::WATER, 0.5f},
        {ResourceType::ENERGY, 0.2f}
    }},
    {ResourceType::BIOFUEL, {
        {ResourceType::WATER, 0.5f},
        {ResourceType::FOOD, 1.0f}
    }}
};

// ---------------------------------------------------------------------------
// Module build / tier-upgrade costs
//
// PLACEHOLDER TUNING. These exist so the module menu's BUILD and UPGRADE
// controls are operable for every unit type; the numbers are a flat curve, not
// a balanced economy. Each module replaces this with its own table when it is
// designed (see docs/guides/module-architecture.md, Part I §7 "Economy").
//
// Keys are tiers: key 1 doubles as the build cost and the tier 0 -> 1 cost,
// matching Unit::BuildModule and Unit::UpgradeModuleTier.
// ---------------------------------------------------------------------------

// Per-tier multiplier applied to a unit type's base module cost.
const float MODULE_TIER_COST_SCALE[4] = {0.0f, 1.0f, 2.2f, 4.0f};

// Energy draw (kW) a module requires at each tier.
const float MODULE_TIER_ENERGY[4] = {5.0f, 9.0f, 15.0f, 24.0f};

// Base (tier 1) module cost per unit type. Scaled by MODULE_TIER_COST_SCALE.
const std::map<std::string, std::map<ResourceType, float>> MODULE_BASE_COSTS = {
    {"Extraction", {
        {ResourceType::CONSTRUCTION_MATERIALS, 40.0f},
        {ResourceType::MACHINERY, 15.0f}
    }},
    {"Farming", {
        {ResourceType::CONSTRUCTION_MATERIALS, 35.0f},
        {ResourceType::WATER, 20.0f}
    }},
    {"Energy", {
        {ResourceType::CONSTRUCTION_MATERIALS, 30.0f},
        {ResourceType::Si, 25.0f}
    }},
    {"Manufacture", {
        {ResourceType::CONSTRUCTION_MATERIALS, 45.0f},
        {ResourceType::Fe, 30.0f}
    }},
    {"Research", {
        {ResourceType::CONSTRUCTION_MATERIALS, 25.0f},
        {ResourceType::ELECTRONICS, 20.0f}
    }},
    {"Construction", {
        {ResourceType::CONSTRUCTION_MATERIALS, 50.0f},
        {ResourceType::MACHINERY, 20.0f}
    }},
    {"Transport", {
        {ResourceType::CONSTRUCTION_MATERIALS, 35.0f},
        {ResourceType::MACHINERY, 25.0f}
    }},
    {"Communication", {
        {ResourceType::CONSTRUCTION_MATERIALS, 20.0f},
        {ResourceType::ELECTRONICS, 30.0f}
    }},
    {"Core", {
        {ResourceType::CONSTRUCTION_MATERIALS, 60.0f},
        {ResourceType::ELECTRONICS, 25.0f}
    }}
};

#endif // GAME_CONSTANTS_H
