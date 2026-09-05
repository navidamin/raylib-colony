// Prints real generated game data to the terminal.
//
// Built for the "is this value wrong, or is my assumption wrong?" question.
// Reading the generation code was not enough to find the composition-scale
// bug -- dumping the actual numbers found it immediately (abundances turned
// out to be quantities in the thousands, not 0-1 fractions).
//
// Build & run (see tools/inspect/README.md):
//   cmake --build build --target colony_inspect
//   ./build/src/colony_inspect              # planet cells around the mid-grid
//   ./build/src/colony_inspect 12 7         # a specific parent cell
//
// Prints, for one parent cell: per-depth-layer raw quantities from
// ResourceManager, then the ProspectingGrid sub-cell view (composition
// fractions + absolute quantity) that the prospecting chain actually sees.

#include "resource_manager.h"
#include "prospecting_grid.h"
#include "sample_tray.h"
#include "site_view.h"
#include "estimate_engine.h"
#include "game_constants.h"

#include <cstdio>
#include <cstdlib>

// Keep in sync with PREVIEW_MAP_SEED in tools/preview/preview_main.cpp
static const unsigned int INSPECT_MAP_SEED = 20260813u;

static const char* LayerName(DepthLayer layer)
{
    switch (layer)
    {
        case DepthLayer::SURFACE: return "SURFACE";
        case DepthLayer::SHALLOW: return "SHALLOW";
        case DepthLayer::MID:     return "MID";
        default:                  return "DEEP";
    }
}

static void DumpParentCell(ResourceManager& rm, int gx, int gy)
{
    printf("\n=== ResourceManager raw quantities at parent cell (%d,%d) ===\n", gx, gy);

    const DepthLayer layers[] = {
        DepthLayer::SURFACE, DepthLayer::SHALLOW, DepthLayer::MID, DepthLayer::DEEP
    };

    for (DepthLayer layer : layers)
    {
        auto resources = rm.GetResourcesAtGridLayer(gx, gy, layer);
        float total = 0.0f;
        printf("%-8s ", LayerName(layer));
        for (const auto& [type, quantity] : resources)
        {
            printf("%s=%.1f ", ResourceTypeToString(type), quantity);
            total += quantity;
        }
        printf(" | total=%.1f\n", total);
    }
}

static void DumpProspectingView(ResourceManager& rm, int gx, int gy, int tier)
{
    ProspectingGrid grid(tier, gx, gy, rm);
    int size = grid.GetGridSize();

    printf("\n=== ProspectingGrid view (tier %d, %dx%d sub-cells) ===\n", tier, size, size);
    printf("Composition fractions should sum to ~1.00 per sub-cell.\n\n");

    // Surface layer only, to keep the output readable
    for (int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++)
        {
            auto composition = grid.GetGroundTruth(x, y, DepthLayer::SURFACE);
            float quantity = grid.GetQuantity(x, y, DepthLayer::SURFACE);

            float sum = 0.0f;
            for (const auto& [type, fraction] : composition) sum += fraction;

            printf("  (%d,%d) qty=%8.1f  sum=%.3f  ", x, y, quantity, sum);
            for (const auto& [type, fraction] : composition)
            {
                if (fraction >= 0.05f)
                {
                    printf("%s:%.0f%% ", ResourceTypeToString(type), fraction * 100.0f);
                }
            }
            printf("\n");
        }
    }
}

// Most abundant resource in a parent cell's surface layer.
static ResourceType PickDominantResource(ResourceManager& rm, int gx, int gy)
{
    ProspectingGrid grid(3, gx, gy, rm);
    std::map<ResourceType, float> totals;

    for (int y = 0; y < grid.GetGridSize(); y++)
    {
        for (int x = 0; x < grid.GetGridSize(); x++)
        {
            float quantity = grid.GetQuantity(x, y, DepthLayer::SURFACE);
            for (const auto& [type, fraction] : grid.GetGroundTruth(x, y, DepthLayer::SURFACE))
            {
                totals[type] += quantity * fraction;
            }
        }
    }

    ResourceType best = ResourceType::Fe;
    float bestTotal = -1.0f;
    for (const auto& [type, total] : totals)
    {
        if (total > bestTotal) { bestTotal = total; best = type; }
    }
    return best;
}

// The same lattice seen through excavation's eyes: reach comes from the
// EXCAVATION tier (not prospecting's), and the number worth choosing on is
// quantity x composition[target], not total quantity.
static void DumpExcavationView(ResourceManager& rm, int gx, int gy,
                               int excTier, ResourceType target)
{
    ProspectingGrid grid(3, gx, gy, rm);   // full-depth data; tier here only gates prospecting
    SampleTray tray(3);
    SiteView site(excTier);

    printf("\n=== Excavation view of parent cell (%d,%d), EXCAVATION tier %d ===\n",
           gx, gy, excTier);
    printf("lattice %dx%d, reach %dx%d, target %s\n",
           site.GetGridSize(), site.GetGridSize(),
           site.GetReach(), site.GetReach(), ResourceTypeToString(target));

    for (int d = 0; d < 4; d++)
    {
        DepthLayer depth = static_cast<DepthLayer>(d);
        if (!site.CanWorkDepth(depth))
        {
            printf("\n%-8s  -- beyond this tier's depth ceiling\n", LayerName(depth));
            continue;
        }

        printf("\n%-8s  target yield per spot ('.' = out of reach)\n", LayerName(depth));

        float reachBest = 0.0f, reachSum = 0.0f, allBest = 0.0f;
        int reachCount = 0;

        for (int y = 0; y < site.GetGridSize(); y++)
        {
            printf("  ");
            for (int x = 0; x < site.GetGridSize(); x++)
            {
                float yield = site.GetTargetYield(grid, x, y, depth, target);
                if (yield > allBest) allBest = yield;

                if (!site.IsInReach(x, y))
                {
                    printf("    .  ");
                    continue;
                }
                printf("%6.0f ", yield);
                reachSum += yield;
                reachCount++;
                if (yield > reachBest) reachBest = yield;
            }
            printf("\n");
        }

        float mean = reachCount > 0 ? reachSum / reachCount : 0.0f;
        printf("  reachable: %d spots, mean %.0f, best %.0f", reachCount, mean, reachBest);
        if (mean > 0.0f) printf("  (best is +%.0f%% over mean)", 100.0f * (reachBest / mean - 1.0f));
        printf("\n  whole lattice best %.0f", allBest);
        if (allBest > 0.0f)
        {
            printf("  -- reach holds %.0f%% of it", 100.0f * reachBest / allBest);
        }
        printf("\n");

        int bx = -1, by = -1;
        if (site.FindBestReachableSpot(grid, depth, target, bx, by))
        {
            SpotView v = site.Describe(grid, tray, bx, by, depth, target);
            printf("  best reachable spot (%d,%d): quantity %.0f, %s fraction %.3f, "
                   "yield %.0f, confidence %.2f\n",
                   bx, by, v.quantity, ResourceTypeToString(target),
                   v.quantity > 0.0f ? v.targetYield / v.quantity : 0.0f,
                   v.targetYield, v.confidence);

            // What the player would be TOLD about that spot at each level of
            // knowledge -- the gamble, in the numbers they actually read.
            EstimateEngine estimator;
            // The blur is around the CELL AVERAGE, not the spot's own value --
            // that is what stops an unsurveyed rich spot reading rich.
            float cellMean = site.GetCellMeanYield(grid, depth, target);
            printf("    what the player reads: ");
            for (float c = 0.0f; c <= 1.001f; c += 0.25f)
            {
                SpotEstimate e = estimator.EstimateAt(v.targetYield, cellMean, c,
                                                      grid.GetParentGridX(),
                                                      grid.GetParentGridY(),
                                                      bx, by, depth, target);
                printf("[conf %.0f%%: %.0f (%.0f-%.0f)] ",
                       c * 100.0f, e.shown, e.low, e.high);
            }
            printf("\n");
        }
    }
}

int main(int argc, char** argv)
{
    int gx = 5;
    int gy = 5;
    int tier = 3;

    if (argc >= 3)
    {
        gx = atoi(argv[1]);
        gy = atoi(argv[2]);
    }
    if (argc >= 4) tier = atoi(argv[3]);

    // The constructor only allocates; Planet normally calls GenerateResourceMap.
    // Same fixed seed as the preview tool, so these numbers describe the world
    // the preview screenshots are rendering.
    ResourceManager rm(PLANET_SIZE, SECT_CORE_RADIUS * 2.0f);
    rm.GenerateResourceMap(INSPECT_MAP_SEED);

    DumpParentCell(rm, gx, gy);
    DumpProspectingView(rm, gx, gy, tier);

    // Excavation's view at every tier, so the reach rings are visible.
    // Target the cell's most abundant resource -- hardcoding one (Fe) just
    // prints zeros in cells that don't contain it.
    ResourceType target = PickDominantResource(rm, gx, gy);
    for (int excTier = 0; excTier <= 3; excTier++)
    {
        DumpExcavationView(rm, gx, gy, excTier, target);
    }

    return 0;
}
