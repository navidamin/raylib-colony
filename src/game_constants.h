#ifndef GAME_CONSTANTS_H
#define GAME_CONSTANTS_H

#include <string>
#include <map>
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

const float DEFAULT_H2ExtractionRate = 1.0;
const float DEFAULT_O2ExtractionRate = 1.0;
const float DEFAULT_CExtractionRate = 1.0;
const float DEFAULT_FeExtractionRate= 1.0;
const float DEFAULT_SiExtractionRate= 1.0;
const float DEFAULT_ResourceFocus= 1;
const float DEFAULT_EnergyConsumption= 1;
const float DEFAULT_WearAndTear= 0.2;
const float DEFAULT_Efficiency= 0.8;
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


#endif // GAME_CONSTANTS_H
