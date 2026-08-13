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

    return 0;
}
