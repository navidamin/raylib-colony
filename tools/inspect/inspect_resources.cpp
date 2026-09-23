// Prints real generated game data to the terminal.
//
// Built for the "is this value wrong, or is my assumption wrong?" question.
// Reading the generation code was not enough to find the composition-scale
// bug -- dumping the actual numbers found it immediately (abundances turned
// out to be quantities in the thousands, not 0-1 fractions).
//
// Build & run (see tools/inspect/README.md):
//   cmake --build build --target colony_inspect
//   ./build/src/colony_inspect                    # Mare Imbrium
//   ./build/src/colony_inspect -43.3 -11.4        # Tycho
//   ./build/src/colony_inspect 32.8 -15.6 2       # Imbrium, tier-2 lattice
//   ./build/src/colony_inspect --pick 32.8,-15.6
//
// Prints, for one place on the Moon: who the region is and what the
// orbital survey says of it, the per-depth-layer raw quantities from
// ResourceManager, then the ProspectingGrid sub-cell view (composition
// fractions + absolute quantity) that the prospecting chain actually sees.

#include "resource_manager.h"
#include "prospecting_grid.h"
#include "region_identity.h"
#include "game_constants.h"
#include "game_structs.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

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

static void DumpRegion(ResourceManager& rm, const LunarPoint& point)
{
    const RegionIdentity& region = rm.GroundAt(point).region;
    ResourceManager::OrbitalSurveyData survey = rm.SurveyAt(point);

    printf("\n=== Ground at %+.4f, %+.4f ===\n", point.latDeg, point.lonDeg);
    printf("region    %s%s%s\n",
           region.name[0] ? region.name : "(unnamed ground)",
           region.terrane[0] ? " -- " : "", region.terrane);
    printf("type      %s, %s\n", region.isMare ? "mare" : "highland",
           region.rock[0] ? region.rock : "rock unknown");
    printf("archetype %s\n", GetSiteArchetypeDescriptor(region.archetype).name);
    printf("survey    Fe %.1f%%  Ti %.1f%%  Al %.1f%%  Ca %.1f%%  Th %.1f ppm  H %.2f\n",
           survey.fePercent * 100.0f, survey.tiPercent * 100.0f,
           survey.alPercent * 100.0f, survey.caPercent * 100.0f,
           survey.thPpm, survey.hydrogenSignal);
    printf("terrain   slope %.1f deg  solar %.2f  earth %.2f\n",
           survey.terrainSlope, survey.solarIllumination, survey.earthVisibility);
}

static void DumpLayers(ResourceManager& rm, const LunarPoint& point)
{
    printf("\n=== ResourceManager raw quantities, by depth layer ===\n");

    const DepthLayer layers[] = {
        DepthLayer::SURFACE, DepthLayer::SHALLOW, DepthLayer::MID, DepthLayer::DEEP
    };

    for (DepthLayer layer : layers)
    {
        auto resources = rm.GetResourcesAtLayer(point, layer);
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

static void DumpProspectingView(ResourceManager& rm, const LunarPoint& point, int tier)
{
    ProspectingGrid grid(tier, point, rm);
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
    // Mare Imbrium by default: populated mare ground.
    LunarPoint point;
    point.latDeg = 32.8;
    point.lonDeg = -15.6;
    int tier = 3;

    int positional = 0;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--pick") == 0 && i + 1 < argc)
        {
            const char* v = argv[++i];
            const char* comma = strchr(v, ',');
            if (!comma)
            {
                fprintf(stderr, "--pick wants LAT,LON\n");
                return 2;
            }
            point.latDeg = atof(v);
            point.lonDeg = atof(comma + 1);
        }
        else if (strcmp(argv[i], "--tier") == 0 && i + 1 < argc)
        {
            tier = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            printf("usage: colony_inspect [LAT LON [TIER]] [--pick LAT,LON] [--tier N]\n");
            return 0;
        }
        else
        {
            // Positional: LAT LON [TIER]
            if (positional == 0) point.latDeg = atof(argv[i]);
            else if (positional == 1) point.lonDeg = atof(argv[i]);
            else if (positional == 2) tier = atoi(argv[i]);
            positional++;
        }
    }
    if (tier < 0) tier = 0;
    if (tier > 3) tier = 3;

    // Same fixed seed as the preview tool, so these numbers describe the
    // world the preview screenshots are rendering.
    ResourceManager rm(INSPECT_MAP_SEED);

    DumpRegion(rm, point);
    DumpLayers(rm, point);
    DumpProspectingView(rm, point, tier);

    return 0;
}
