#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "game_constants.h"
#include "resource_types.h"

// Test storage constants and upgrade math (no Sect/Colony instantiation needed)

TEST_CASE("Storage constants are valid", "[storage]")
{
    REQUIRE(SECT_BASE_STORAGE > 0.0f);
    REQUIRE(COLONY_BASE_RESERVES > 0.0f);
    REQUIRE(COLONY_BASE_RESERVES > SECT_BASE_STORAGE);

    REQUIRE(STORAGE_SURPLUS_THRESHOLD > STORAGE_DEFICIT_THRESHOLD);
    REQUIRE(STORAGE_SURPLUS_THRESHOLD > 0.0f);
    REQUIRE(STORAGE_SURPLUS_THRESHOLD <= 1.0f);
    REQUIRE(STORAGE_DEFICIT_THRESHOLD >= 0.0f);
    REQUIRE(STORAGE_DEFICIT_THRESHOLD < 1.0f);
}

TEST_CASE("Storage level multipliers are monotonically increasing", "[storage]")
{
    for (int i = 1; i <= MAX_STORAGE_LEVEL; i++)
    {
        REQUIRE(STORAGE_LEVEL_MULTIPLIERS[i] > STORAGE_LEVEL_MULTIPLIERS[i - 1]);
    }
    REQUIRE(STORAGE_LEVEL_MULTIPLIERS[0] == 1.0f);
}

TEST_CASE("Sect upgrade costs increase per level", "[storage]")
{
    for (int i = 1; i < MAX_STORAGE_LEVEL; i++)
    {
        REQUIRE(SECT_UPGRADE_COST_FE[i + 1] > SECT_UPGRADE_COST_FE[i]);
        REQUIRE(SECT_UPGRADE_COST_SI[i + 1] > SECT_UPGRADE_COST_SI[i]);
        REQUIRE(SECT_UPGRADE_COST_ENERGY[i + 1] > SECT_UPGRADE_COST_ENERGY[i]);
    }
}

TEST_CASE("Colony upgrade costs increase per level", "[storage]")
{
    for (int i = 1; i < MAX_STORAGE_LEVEL; i++)
    {
        REQUIRE(COLONY_UPGRADE_COST_FE[i + 1] > COLONY_UPGRADE_COST_FE[i]);
        REQUIRE(COLONY_UPGRADE_COST_SI[i + 1] > COLONY_UPGRADE_COST_SI[i]);
        REQUIRE(COLONY_UPGRADE_COST_ENERGY[i + 1] > COLONY_UPGRADE_COST_ENERGY[i]);
    }
}

TEST_CASE("Colony costs are higher than sect costs", "[storage]")
{
    for (int i = 1; i <= MAX_STORAGE_LEVEL; i++)
    {
        REQUIRE(COLONY_UPGRADE_COST_FE[i] > SECT_UPGRADE_COST_FE[i]);
        REQUIRE(COLONY_UPGRADE_COST_ENERGY[i] > SECT_UPGRADE_COST_ENERGY[i]);
    }
}

TEST_CASE("Transport constants are valid", "[transport]")
{
    REQUIRE(BASE_TRANSPORT_SPEED > 0.0f);
    REQUIRE(TRANSPORT_PACKET_SIZE > 0.0f);
    REQUIRE(AUTO_BALANCE_THRESHOLD > 0.0f);
    REQUIRE(AUTO_BALANCE_THRESHOLD < 1.0f);
    REQUIRE(MIN_TRANSPORT_INTERVAL > 0.0f);
    REQUIRE(MAX_PACKETS_PER_ROAD > 0);
}

TEST_CASE("Survey constants are valid", "[survey]")
{
    // Base progress should increase with tier
    REQUIRE(SURVEY_BASE_PROGRESS_T1 > SURVEY_BASE_PROGRESS_T0);
    REQUIRE(SURVEY_BASE_PROGRESS_T2 > SURVEY_BASE_PROGRESS_T1);
    REQUIRE(SURVEY_BASE_PROGRESS_T3 > SURVEY_BASE_PROGRESS_T2);

    // Efficiency bounds
    REQUIRE(SURVEY_UNSCANNED_EFFICIENCY > 0.0f);
    REQUIRE(SURVEY_UNSCANNED_EFFICIENCY < 1.0f);
    REQUIRE(SURVEY_UNSCANNED_EFFICIENCY + SURVEY_SCANNED_BONUS <= 1.0f + 0.001f);
}

TEST_CASE("Extraction production costs defined for key resources", "[constants]")
{
    // Fe extraction should cost Energy
    auto feIt = EXTRACTION_PRODUCTION_COSTS.find(ResourceType::Fe);
    REQUIRE(feIt != EXTRACTION_PRODUCTION_COSTS.end());
    auto feEnergy = feIt->second.find(ResourceType::ENERGY);
    REQUIRE(feEnergy != feIt->second.end());
    REQUIRE(feEnergy->second > 0.0f);

    // Food farming should cost Water
    auto foodIt = FARMING_PRODUCTION_COSTS.find(ResourceType::FOOD);
    REQUIRE(foodIt != FARMING_PRODUCTION_COSTS.end());
    auto foodWater = foodIt->second.find(ResourceType::WATER);
    REQUIRE(foodWater != foodIt->second.end());
    REQUIRE(foodWater->second > 0.0f);
}
