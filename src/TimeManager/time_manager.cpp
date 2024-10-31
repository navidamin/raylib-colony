#include "time_manager.h"


TimeManager::TimeManager()
    : gameTime(0.0f),
      timeScale(1.0f),
      accumulatedTime(0.0f),
      currentTicks(0),
      isPaused(false)
{
    // Initialize containers

}

TimeManager::~TimeManager() {
    // Clear all containers

}

//---------------------------------------------------------------------------------------------
//            Core time functions
//---------------------------------------------------------------------------------------------

void TimeManager::Update(float deltaTime) {
    if (isPaused) return;

    // Scale delta time by time scale
    float scaledDelta = deltaTime * timeScale;
    gameTime += scaledDelta;

    // Accumulate time towards next tick
    accumulatedTime += scaledDelta;

    // Process ticks if enough time has accumulated
    while (accumulatedTime >= TICK_DURATION) {
        std::cout << "Game Time is: " << gameTime << std::endl;
        ProcessTick();
        accumulatedTime -= TICK_DURATION;
    }
}

void TimeManager::Reset() {
    // Reset time-related members
    gameTime = 0.0f;
    timeScale = 1.0f;
    accumulatedTime = 0.0f;
    currentTicks = 0;
    isPaused = false;

    // Clear all tracking containers

}

void TimeManager::Pause() {
    isPaused = true;
}

void TimeManager::Resume() {
    isPaused = false;
}

void TimeManager::SetTimeScale(float scale) {
    timeScale = scale > 0.0f ? scale : 1.0f;
}
/*
//---------------------------------------------------------------------------------------------
//            Production management
//---------------------------------------------------------------------------------------------

void TimeManager::StartProduction(Unit* unit) {
    if (!unit) return;

    ProductionCycle cycle;
    cycle.elapsedTime = 0.0f;
    cycle.cycleTime = unit->GetProductionCycleTime();
    cycle.progress = 0.0f;
    cycle.isActive = true;

    productionCycles[unit] = cycle;
}

void TimeManager::StopProduction(Unit* unit) {
    if (!unit) return;

    auto it = productionCycles.find(unit);
    if (it != productionCycles.end()) {
        it->second.isActive = false;
    }
}

float TimeManager::GetProductionProgress(Unit* unit) const {
    if (!unit) return 0.0f;

    auto it = productionCycles.find(unit);
    return (it != productionCycles.end()) ? it->second.progress : 0.0f;
}

void TimeManager::UpdateProductionCycle(Unit* unit) {
    if (!unit) return;

    auto it = productionCycles.find(unit);
    if (it != productionCycles.end() && it->second.isActive) {
        ProductionCycle& cycle = it->second;
        cycle.elapsedTime += TICK_DURATION * timeScale;
        cycle.progress = cycle.elapsedTime / cycle.cycleTime;

        if (cycle.elapsedTime >= cycle.cycleTime) {
            // Complete production cycle
            cycle.elapsedTime = 0.0f;
            cycle.progress = 0.0f;

            // Process resource production
            UpdateResourceProduction(unit);
        }
    }
}

//---------------------------------------------------------------------------------------------
//            Construction management
//---------------------------------------------------------------------------------------------

void TimeManager::StartConstruction(Unit* unit, float constructionTime) {
    if (!unit) return;

    ConstructionTimer timer;
    timer.elapsedTime = 0.0f;
    timer.totalTime = constructionTime;
    timer.progress = 0.0f;
    timer.isComplete = false;

    constructionTimers[unit] = timer;
}

void TimeManager::UpdateConstruction(Unit* unit) {
    if (!unit) return;

    auto it = constructionTimers.find(unit);
    if (it != constructionTimers.end() && !it->second.isComplete) {
        ConstructionTimer& timer = it->second;
        timer.elapsedTime += TICK_DURATION * timeScale;
        timer.progress = timer.elapsedTime / timer.totalTime;

        if (timer.elapsedTime >= timer.totalTime) {
            timer.progress = 1.0f;
            timer.isComplete = true;
            unit->OnConstructionComplete();
        }
    }
}

float TimeManager::GetConstructionProgress(Unit* unit) const {
    if (!unit) return 0.0f;

    auto it = constructionTimers.find(unit);
    return (it != constructionTimers.end()) ? it->second.progress : 0.0f;
}

bool TimeManager::IsConstructionComplete(Unit* unit) const {
    if (!unit) return false;

    auto it = constructionTimers.find(unit);
    return (it != constructionTimers.end()) ? it->second.isComplete : false;
}


//---------------------------------------------------------------------------------------------
//            Resource rate management
//---------------------------------------------------------------------------------------------

void TimeManager::SetProductionRate(Unit* unit, ResourceType type, float rate) {
    if (!unit) return;

    auto& rates = productionRates[unit];
    auto it = std::find_if(rates.begin(), rates.end(),
        [type](const ProductionRate& pr) { return pr.type == type; });

    if (it != rates.end()) {
        it->ratePerTick = rate;
    } else {
        rates.push_back({type, rate, 0.0f});
    }
}
void TimeManager::UpdateResourceProduction(Unit* unit) {
    if (!unit) return;

    auto ratesIt = productionRates.find(unit);
    if (ratesIt != productionRates.end()) {
        for (auto& rate : ratesIt->second) {
            // Accumulate resources
            accumulatedResources[unit][rate.type] += rate.ratePerTick;
            rate.accumulated += rate.ratePerTick;
        }
    }
}

float TimeManager::GetAccumulatedResources(Unit* unit, ResourceType type) const {
    if (!unit) return 0.0f;

    auto unitIt = accumulatedResources.find(unit);
    if (unitIt != accumulatedResources.end()) {
        auto resourceIt = unitIt->second.find(type);
        if (resourceIt != unitIt->second.end()) {
            return resourceIt->second;
        }
    }
    return 0.0f;
}

void TimeManager::ClearAccumulatedResources(Unit* unit) {
    if (!unit) return;

    auto unitIt = accumulatedResources.find(unit);
    if (unitIt != accumulatedResources.end()) {
        unitIt->second.clear();
    }

    auto ratesIt = productionRates.find(unit);
    if (ratesIt != productionRates.end()) {
        for (auto& rate : ratesIt->second) {
            rate.accumulated = 0.0f;
        }
    }
}

//---------------------------------------------------------------------------------------------
//            Colony management functions definitions
//---------------------------------------------------------------------------------------------
/*
void TimeManager::RegisterColony(Colony* colony) {
    // Only add if colony is valid and not already registered
    if (!colony || IsColonyRegistered(colony)) {
        return;
    }

    colonies.push_back(colony);
}

void TimeManager::UnregisterColony(Colony* colony) {
    if (!colony) return;

    auto it = std::find(colonies.begin(), colonies.end(), colony);
    if (it != colonies.end()) {
        colonies.erase(it);
    }
}

bool TimeManager::IsColonyRegistered(Colony* colony) const {
    return std::find(colonies.begin(), colonies.end(), colony) != colonies.end();
}

*/
//---------------------------------------------------------------------------------------------
//            Getters/Utility functions
//---------------------------------------------------------------------------------------------


float TimeManager::TicksToSeconds(int ticks) const {
    return static_cast<float>(ticks) * TICK_DURATION;
}

int TimeManager::SecondsToTicks(float seconds) const {
    return static_cast<int>(seconds / TICK_DURATION);
}



//---------------------------------------------------------------------------------------------
//            Private member functions definitions
//---------------------------------------------------------------------------------------------


/*
void TimeManager::UpdateColonies() {
    for (Colony* colony : colonies) {
        if (!colony) continue;

        //UpdateColonyResources(colony);
        //UpdateColonyConstruction(colony);
    }
}

void TimeManager::UpdateColonyResources(Colony* colony) {
    // Update resource production and consumption for each Unit in the Colony
    for (auto& sect : colony->GetSects()) {
        if (!sect) continue;

        for (auto& unit : sect->GetUnits()) {
            if (!unit || !unit->IsActive()) continue;

            const std::string& unitType = unit->GetUnitType();

            // Process resource production based on Unit type and settings
            if (unitType == "Extraction") {
                unit->ProcessExtraction(TICK_DURATION);
            }
            else if (unitType == "Farming") {
                unit->ProcessFarming(TICK_DURATION);
            }
            else if (unitType == "Energy") {
                unit->ProcessEnergy(TICK_DURATION);
            }
            // TODO: add for other units as they get developed
        }



    }
}


void TimeManager::UpdateColonyConstruction(Colony* colony) {
    for (auto& sect : colony->GetSects()) {
        if (!sect) continue;

        // Update construction progress for units being built
        for (auto& unit : sect->GetUnits()) {
            if (!unit) continue;

            if (unit->IsUnderConstruction()) {
                unit->UpdateConstruction(TICK_DURATION);
            }
        }

        // Update road construction if any are in progress
        sect->UpdateRoadConstruction(TICK_DURATION);
    }
}
*/
void TimeManager::ProcessTick() {
    std::cout << "current ticks is: " << currentTicks << std::endl;
    currentTicks++;
}

void TimeManager::Draw(int screenWidth, int screenHeight) {

    int currentDay = GetCurrentDay();

    // Calculate text properties
    const char* text = TextFormat("Day %d", currentDay);
    int fontSize = 18;
    int textWidth = MeasureText(text, fontSize);

    // Position text at top center of screen with slight padding
    int xPos = (screenWidth - textWidth) / 2;
    int yPos = 10;  // Padding from top

    // Draw background rectangle for better visibility
    Color bgColor = Color{0, 0, 0, 100};  // Semi-transparent black
    int padding = 8;
    DrawRectangle(
        xPos - padding,
        yPos - padding/2,
        textWidth + padding*2,
        fontSize + padding,
        bgColor
    );

    // Draw the text
    DrawText(text, xPos, yPos, fontSize, WHITE);

    // If game is paused, show pause indicator
    if (isPaused) {
        const char* pausedText = "PAUSED";
        int pausedFontSize = 20;
        int pausedWidth = MeasureText(pausedText, pausedFontSize);
        DrawText(
            pausedText,
            (screenWidth - pausedWidth) / 2,
            yPos + fontSize + padding,
            pausedFontSize,
            RED
        );
    }
}

int TimeManager::GetCurrentDay() const {
    return currentTicks / TICKS_PER_DAY;
}
