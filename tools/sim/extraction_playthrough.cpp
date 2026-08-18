// A playthrough of the WHOLE extraction unit, day by day.
//
//   cmake --build build --target colony_playthrough && ./build/src/colony_playthrough
//
// colony_sim drives the excavation engines directly, which is fast but skips
// everything downstream. This drives a real Unit through its real Update(), so
// the whole chain runs:
//
//     sweep -> sample -> lab -> survey progress
//                                     |
//                                     v
//     excavation (spot, machine, pace) -> beneficiation -> unit storage
//
// That matters because the separation chain and storage have never been
// exercised against the composition the rebuilt excavation module produces --
// it is a different shape from the flat per-cell skim it replaced. A pipeline
// that "still runs" is not the same as one that behaves.
//
// The output is a session log rather than a pass/fail: a day-by-day account of
// what a player would see, ending in what actually reached the shelf.

#include "unit.h"
#include "resource_manager.h"
#include "time_manager.h"
#include "prospecting_system.h"
#include "excavation_system.h"
#include "excavation_constants.h"
#include "survey_progress_engine.h"
#include "game_constants.h"

#include <cstdio>
#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <streambuf>
#include <string>
#include <vector>

static const unsigned int SEED = 20260813u;
static const int DAYS = 30;

class Quiet
{
public:
    Quiet() : saved(std::cout.rdbuf()) { std::cout.rdbuf(nullptr); }
    ~Quiet() { std::cout.rdbuf(saved); }
private:
    std::streambuf* saved;
};

static int FindModule(Unit& unit, const std::string& type)
{
    const auto& modules = unit.GetModules();
    for (size_t i = 0; i < modules.size(); i++)
    {
        if (modules[i].moduleType == type) return static_cast<int>(i);
    }
    return -1;
}

// The unit normally draws energy from its sect. There is no sect here, so keep
// a generous but finite supply -- free energy would hide any power problem.
static void TopUpEnergy(std::map<ResourceType, float>& storage)
{
    storage[ResourceType::ENERGY] = 5000.0f;
}

int main()
{
    printf("\n===============================================================\n");
    printf(" EXTRACTION UNIT PLAYTHROUGH -- %d game days, seed %u\n", DAYS, SEED);
    printf("===============================================================\n");

    ResourceManager rm(PLANET_SIZE, SECT_CORE_RADIUS * 2.0f);
    {
        Quiet quiet;
        rm.GenerateResourceMap(SEED);
    }

    TimeManager time;
    std::map<ResourceType, float> storage;
    std::map<ResourceType, float> capacity;
    for (const auto& descriptor : GetResourceDescriptors())
    {
        capacity[descriptor.type] = 100000.0f;
    }
    TopUpEnergy(storage);

    Vector2 position = {SECT_CORE_RADIUS * 2.0f * 5.0f, SECT_CORE_RADIUS * 2.0f * 5.0f};
    std::unique_ptr<Unit> unit;
    {
        Quiet quiet;
        unit = std::make_unique<Unit>("Extraction", position, rm, time, storage, capacity);
    }

    int prospectingIndex = FindModule(*unit, "PROSPECTING");
    int excavationIndex = FindModule(*unit, "EXCAVATION");
    int beneficiationIndex = FindModule(*unit, "BENEFICIATION");

    {
        Quiet quiet;
        unit->ActivateModule(prospectingIndex);
        unit->ActivateModule(excavationIndex);
        unit->ActivateModule(beneficiationIndex);
        unit->Start();
    }

    ProspectingSystem* ps = unit->GetProspectingSystem();
    ExcavationSystem* es = unit->GetExcavationSystem();

    if (!ps || !es)
    {
        printf("\nFAILED: the unit has no prospecting or excavation system.\n");
        return 1;
    }

    printf("\nA fresh extraction unit on parent cell (5,5).\n");
    printf("Prospecting, excavation and beneficiation all active, all tier 0.\n");
    printf("The player surveys a little each morning, upgrades when able, and\n");
    printf("otherwise leaves the excavation module on its default automation.\n\n");

    printf("--- storage before the unit has run a single tick ---\n");
    for (const auto& [type, amount] : unit->GetResourceStorage())
    {
        if (amount > 0.01f) printf("  %-10s %10.1f\n", ResourceTypeToString(type), amount);
    }
    printf("\n");

    printf(" day  tier  survey  reach  machine        spot  target      dug   stored  useful\n");
    printf(" ---  ----  ------  -----  -------------  ----  ------  -------  -------  ------\n");

    float lastStored = 0.0f;
    float totalDug = 0.0f;
    int upgradesDone = 0;

    for (int day = 1; day <= DAYS; day++)
    {
        // --- Morning: a little surveying, the way a player dips in ---
        {
            Quiet quiet;
            ProspectingGrid& grid = ps->GetGrid();

            // One sweep band per day while there are bands left.
            for (int band = 0; band < SWEEP_FREQUENCY_BANDS; band++)
            {
                if (ps->GetSweep().CanSweep(grid, band))
                {
                    ps->GetSweep().ExecuteSweep(grid, band, ps->gameTime);
                    break;
                }
            }

            // A couple of cores, then analyse whatever is in the tray.
            int collected = 0;
            for (int y = 0; y < grid.GetGridSize() && collected < 2; y++)
            {
                for (int x = 0; x < grid.GetGridSize() && collected < 2; x++)
                {
                    if (!grid.IsInReach(x, y)) continue;
                    if (ps->GetTray().IsFull()) break;
                    if (ps->GetSampler().CollectSample(grid, ps->GetTray(), x, y,
                                                       DepthLayer::SURFACE))
                    {
                        collected++;
                    }
                }
            }

            for (Sample& sample : ps->GetTray().GetSamples())
            {
                if (ps->GetLab().CanApplyTool(sample, AnalysisTool::XRF))
                {
                    ps->GetLab().ApplyTool(sample, AnalysisTool::XRF, ps->gameTime);
                }
            }
        }

        // --- Upgrade when the player can, one module per day ---
        if (day % 6 == 0 && upgradesDone < 6)
        {
            Quiet quiet;
            int target = (upgradesDone % 2 == 0) ? prospectingIndex : excavationIndex;
            if (unit->DebugUpgradeModuleTier(target)) upgradesDone++;
        }

        // --- The day itself ---
        float dugToday = 0.0f;
        for (int tick = 0; tick < TICKS_PER_DAY; tick++)
        {
            TopUpEnergy(storage);
            ps->gameTime += 1.0f;
            {
                Quiet quiet;
                unit->Update(1.0f);
            }
            // Accumulate per tick. Sampling only the last tick of the day made
            // the pipeline look like it delivered 24,000% of what was dug.
            dugToday += es->GetLastResult().totalMass;
        }

        // --- Evening: what happened ---
        CellSurveyResult survey = SurveyProgressEngine::Calculate(ps->GetGrid(),
                                                                  ps->GetTray());

        // What reached the shelf, i.e. what survived beneficiation.
        float stored = 0.0f;
        for (const auto& [type, amount] : unit->GetResourceStorage())
        {
            if (type == ResourceType::ENERGY) continue;
            stored += amount;
        }

        const DigResult& last = es->GetLastResult();
        totalDug += dugToday;

        int excTier = unit->GetModules()[excavationIndex].tier;
        const char* machineName = es->GetActiveMachine().displayName;
        const char* targetName = ResourceTypeToString(es->targetResource);
        float useful = last.totalMass > 0.0f ? last.targetMass / last.totalMass : 0.0f;

        printf(" %3d   %d/%d   %5.0f%%  %dx%d   %-13s  %d,%d  %6s  %7.1f  %7.1f  %5.0f%%\n",
               day, excTier, 3, survey.surveyProgress * 100.0f,
               es->GetSite().GetReach(), es->GetSite().GetReach(),
               machineName, es->selectedSpotX, es->selectedSpotY, targetName,
               dugToday, stored - lastStored, useful * 100.0f);

        lastStored = stored;
    }

    // -----------------------------------------------------------------------
    printf("\n--- what reached the shelf ---\n");

    // WATER, FOOD, SCIENCE and MANPOWER are seeded into a new unit's storage;
    // counting them as production made the pipeline look like it created mass.
    static const ResourceType seeded[] = {
        ResourceType::ENERGY, ResourceType::WATER, ResourceType::FOOD,
        ResourceType::SCIENCE, ResourceType::MANPOWER
    };

    float totalStored = 0.0f;
    for (const auto& [type, amount] : unit->GetResourceStorage())
    {
        if (amount <= 0.01f) continue;

        bool isSeeded = false;
        for (ResourceType s : seeded) { if (type == s) isSeeded = true; }

        printf("  %-10s %10.1f%s\n", ResourceTypeToString(type), amount,
               isSeeded ? "   (seeded, not mined)" : "");
        if (!isSeeded) totalStored += amount;
    }

    if (totalStored <= 0.0f)
    {
        printf("  NOTHING. The pipeline ran for %d days and delivered no material.\n", DAYS);
    }

    printf("\n--- the pipeline end to end ---\n");
    printf("  dug out of the ground   %10.1f\n", totalDug);
    printf("  reached storage         %10.1f\n", totalStored);
    if (totalDug > 0.0f)
    {
        printf("  survived beneficiation  %9.0f%%\n", 100.0f * totalStored / totalDug);
    }

    printf("\n--- the ground afterwards ---\n");
    const ProspectingGrid& grid = ps->GetGrid();
    int dugSpots = 0;
    int surveyedSpots = 0;
    for (int y = 0; y < grid.GetGridSize(); y++)
    {
        for (int x = 0; x < grid.GetGridSize(); x++)
        {
            if (grid.GetExcavatedKnowledge(x, y) > 0.0f) dugSpots++;
            if (grid.GetSubCell(x, y).aggregateConfidence > 0.05f) surveyedSpots++;
        }
    }
    printf("  spots surveyed   %d of 64\n", surveyedSpots);
    printf("  spots dug        %d of 64\n", dugSpots);
    printf("  survey progress  %.0f%%\n",
           SurveyProgressEngine::Calculate(grid, ps->GetTray()).surveyProgress * 100.0f);

    printf("\n");
    return 0;
}
