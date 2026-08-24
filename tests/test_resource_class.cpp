// Resource classification: Measured / Indicated / Inferred.
//
// The classes are a GROUPING of ConfidenceLevel, not a second banding of the
// same number. These tests exist to keep it that way -- if someone ever
// reimplements GetResourceClass with thresholds of its own, the first test
// here fails the moment those thresholds drift from CONFIDENCE_THRESHOLD_*.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <string>
#include "test_helpers.h"

#include "prospecting_types.h"
#include "prospecting_constants.h"
#include "prospecting_grid.h"
#include "sample_tray.h"
#include "resource_manager.h"
#include "game_constants.h"

// The mapping the grouping is defined by. One place, so the test states the
// contract rather than restating arithmetic.
static ResourceClass ExpectedFrom(ConfidenceLevel level)
{
    switch (level)
    {
        case ConfidenceLevel::CERTAIN:   return ResourceClass::MEASURED;
        case ConfidenceLevel::HIGH:
        case ConfidenceLevel::MODERATE:  return ResourceClass::INDICATED;
        case ConfidenceLevel::LOW:       return ResourceClass::INFERRED;
        default:                         return ResourceClass::UNCLASSIFIED;
    }
}

TEST_CASE("ResourceClass never contradicts ConfidenceLevel", "[class]")
{
    // Walk the whole range finely enough to land on both sides of every
    // boundary. This is the drift test: it does not care what the thresholds
    // ARE, only that one reading is derived from the other.
    for (int i = 0; i <= 1000; i++)
    {
        float c = static_cast<float>(i) / 1000.0f;
        REQUIRE(GetResourceClass(c) == ExpectedFrom(GetConfidenceLevel(c)));
    }
}

TEST_CASE("classes sit on the existing threshold constants", "[class]")
{
    // Just above each boundary, so the >= / > convention is pinned too.
    const float eps = 0.001f;

    REQUIRE(GetResourceClass(0.0f) == ResourceClass::UNCLASSIFIED);
    REQUIRE(GetResourceClass(CONFIDENCE_THRESHOLD_LOW) == ResourceClass::UNCLASSIFIED);
    REQUIRE(GetResourceClass(CONFIDENCE_THRESHOLD_LOW + eps) == ResourceClass::INFERRED);

    REQUIRE(GetResourceClass(CONFIDENCE_THRESHOLD_MODERATE) == ResourceClass::INFERRED);
    REQUIRE(GetResourceClass(CONFIDENCE_THRESHOLD_MODERATE + eps) == ResourceClass::INDICATED);

    REQUIRE(GetResourceClass(CONFIDENCE_THRESHOLD_HIGH) == ResourceClass::INDICATED);
    REQUIRE(GetResourceClass(CONFIDENCE_THRESHOLD_HIGH + eps) == ResourceClass::INDICATED);

    REQUIRE(GetResourceClass(CONFIDENCE_THRESHOLD_CERTAIN) == ResourceClass::INDICATED);
    REQUIRE(GetResourceClass(CONFIDENCE_THRESHOLD_CERTAIN + eps) == ResourceClass::MEASURED);
    REQUIRE(GetResourceClass(1.0f) == ResourceClass::MEASURED);
}

TEST_CASE("classification is monotonic", "[class]")
{
    // More evidence must never demote a spot. A regression that broke this
    // would be invisible in a screenshot and obvious here.
    ResourceClass previous = GetResourceClass(0.0f);
    for (int i = 1; i <= 1000; i++)
    {
        ResourceClass current = GetResourceClass(static_cast<float>(i) / 1000.0f);
        REQUIRE(static_cast<int>(current) >= static_cast<int>(previous));
        previous = current;
    }
}

TEST_CASE("only Measured and Indicated are committable", "[class]")
{
    REQUIRE(IsCommittable(ResourceClass::MEASURED));
    REQUIRE(IsCommittable(ResourceClass::INDICATED));
    REQUIRE_FALSE(IsCommittable(ResourceClass::INFERRED));
    REQUIRE_FALSE(IsCommittable(ResourceClass::UNCLASSIFIED));
}

TEST_CASE("class names are distinct and non-empty", "[class]")
{
    const ResourceClass all[] = { ResourceClass::UNCLASSIFIED, ResourceClass::INFERRED,
                                  ResourceClass::INDICATED, ResourceClass::MEASURED };
    for (int a = 0; a < 4; a++)
    {
        REQUIRE(std::string(ResourceClassName(all[a])).size() > 0);
        for (int b = a + 1; b < 4; b++)
        {
            REQUIRE(std::string(ResourceClassName(all[a])) !=
                    std::string(ResourceClassName(all[b])));
        }
    }
}

// ---------------------------------------------------------------------------
// The roll-up
// ---------------------------------------------------------------------------

TEST_CASE("an unsurveyed grid is entirely unclassified", "[class][split]")
{
    ResourceManager rm(PLANET_SIZE, SECT_CORE_RADIUS * 2.0f);
    rm.GenerateResourceMap(4242u);

    ProspectingGrid grid(3, 10, 10, rm);
    SampleTray tray(3);

    ResourceType type = grid.GetGroundTruth(0, 0, DepthLayer::SURFACE).begin()->first;
    ClassSplit split = GetClassSplit(grid, tray, type, 3);

    REQUIRE(split.Total() > 0.0f);
    REQUIRE(split.measured == 0.0f);
    REQUIRE(split.indicated == 0.0f);
    REQUIRE(split.inferred == 0.0f);
    REQUIRE(split.unclassified == split.Total());
    REQUIRE(split.Committable() == 0.0f);
}

TEST_CASE("digging moves tonnage into Measured", "[class][split]")
{
    ResourceManager rm(PLANET_SIZE, SECT_CORE_RADIUS * 2.0f);
    rm.GenerateResourceMap(4242u);

    ProspectingGrid grid(3, 10, 10, rm);
    SampleTray tray(3);

    ResourceType type = grid.GetGroundTruth(0, 0, DepthLayer::SURFACE).begin()->first;
    ClassSplit before = GetClassSplit(grid, tray, type, 3);

    // Digging is direct observation, so it is the cheapest way to force a
    // spot to MEASURED without standing up the whole sweep/sample chain.
    grid.RecordExcavation(4, 4, DepthLayer::SURFACE, 1.0f);
    ClassSplit after = GetClassSplit(grid, tray, type, 3);

    REQUIRE(after.measured > before.measured);
    REQUIRE(after.unclassified < before.unclassified);

    // Nothing is created or destroyed by learning about it.
    REQUIRE(after.Total() == Catch::Approx(before.Total()).epsilon(0.001));
}

TEST_CASE("the split only counts ground the tier can reach", "[class][split]")
{
    ResourceManager rm(PLANET_SIZE, SECT_CORE_RADIUS * 2.0f);
    rm.GenerateResourceMap(4242u);

    ProspectingGrid grid(3, 10, 10, rm);
    SampleTray tray(3);

    ResourceType type = grid.GetGroundTruth(0, 0, DepthLayer::SURFACE).begin()->first;

    // Reach grows 2x2 -> 4x4 -> 6x6 -> 8x8 and depth 1 -> 4 layers, so the
    // statement can only get bigger.
    float previous = 0.0f;
    for (int tier = 0; tier <= 3; tier++)
    {
        float total = GetClassSplit(grid, tray, type, tier).Total();
        REQUIRE(total > previous);
        previous = total;
    }
}

TEST_CASE("depth confidence is per depth, not per column", "[class][split]")
{
    ResourceManager rm(PLANET_SIZE, SECT_CORE_RADIUS * 2.0f);
    rm.GenerateResourceMap(4242u);

    ProspectingGrid grid(3, 10, 10, rm);
    SampleTray tray(3);

    grid.RecordExcavation(4, 4, DepthLayer::SURFACE, 1.0f);

    // Digging the surface says nothing about what lies under it -- this is
    // what keeps the deep layers a bet long after the surface is mapped.
    REQUIRE(GetResourceClass(GetDepthConfidence(grid, tray, 4, 4, DepthLayer::SURFACE))
            == ResourceClass::MEASURED);
    REQUIRE(GetResourceClass(GetDepthConfidence(grid, tray, 4, 4, DepthLayer::DEEP))
            == ResourceClass::UNCLASSIFIED);
}
