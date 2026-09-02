// Resource classification: Measured / Indicated / Inferred.
//
// The classes are a GROUPING of ConfidenceLevel, not a second banding of the
// same number. These tests exist to keep it that way -- if someone ever
// reimplements GetResourceClass with thresholds of its own, the first test
// here fails the moment those thresholds drift from CONFIDENCE_THRESHOLD_*.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
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

    // The total is NOT conserved any more, and that is correct: the statement
    // is built from the ESTIMATE, so learning revises the belief -- a rich
    // core pulls the estimate up around it, a barren one pulls it down. The
    // old conservation held only while the statement was secretly reading
    // ground truth, which was the defect.
    REQUIRE(after.Total() > 0.0f);
}

TEST_CASE("the statement is knowledge-scoped, not window-scoped", "[class][split]")
{
    ResourceManager rm(PLANET_SIZE, SECT_CORE_RADIUS * 2.0f);
    rm.GenerateResourceMap(4242u);

    ProspectingGrid grid(3, 10, 10, rm);
    SampleTray tray(3);

    ResourceType type = grid.GetGroundTruth(0, 0, DepthLayer::SURFACE).begin()->first;

    // Reach and depth are ungated: the statement covers the whole lattice at
    // every depth regardless of tier. What varies between players is how well
    // ground is KNOWN, which the classes already express.
    float t0 = GetClassSplit(grid, tray, type, 0).Total();
    for (int tier = 1; tier <= 3; tier++)
    {
        REQUIRE_THAT(GetClassSplit(grid, tray, type, tier).Total(),
                     Catch::Matchers::WithinRel(t0, 0.0001f));
    }
    REQUIRE(t0 > 0.0f);
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

TEST_CASE("a core builds the designed halo of classes around it", "[class][field]")
{
    ResourceManager rm(PLANET_SIZE, SECT_CORE_RADIUS * 2.0f);
    rm.GenerateResourceMap(4242u);

    ProspectingGrid grid(0, 10, 10, rm);
    SampleTray tray(0);
    SamplingEngine engine(0);

    REQUIRE(engine.CollectSample(grid, tray, 3, 3, DepthLayer::SURFACE));

    // The support ladder at RANGE = 20 m, on the 3.125 m lattice. The METRIC
    // footprint is identical to the 12.5 m and 6.25 m lattices before it --
    // the same distances just land on different cell offsets:
    //   cored block            1.00  MEASURED  (the rock is in your hand)
    //   neighbour     3.125 m  0.98  MEASURED
    //   two cells     6.25 m   0.91  MEASURED  (the old one-cell column)
    //   four cells   12.50 m   0.68  INDICATED
    //   eight cells  25.00 m   0.21  INFERRED
    //   twelve cells 37.50 m   0.03  UNCLASSIFIED
    auto cls = [&](int x, int y) {
        return GetResourceClass(GetDepthConfidence(grid, tray, x, y,
                                                   DepthLayer::SURFACE));
    };
    REQUIRE(cls(3, 3)  == ResourceClass::MEASURED);
    REQUIRE(cls(4, 3)  == ResourceClass::MEASURED);
    REQUIRE(cls(5, 3)  == ResourceClass::MEASURED);
    REQUIRE(cls(7, 3)  == ResourceClass::INDICATED);
    REQUIRE(cls(11, 3) == ResourceClass::INFERRED);
    REQUIRE(cls(15, 3) == ResourceClass::UNCLASSIFIED);

    // And per depth: the SURFACE core supports the layer below it only
    // weakly (17 m -> 0.49, INDICATED), and the deep layers not at all.
    REQUIRE(cls(3, 3) == ResourceClass::MEASURED);
    REQUIRE(GetResourceClass(GetDepthConfidence(grid, tray, 3, 3,
                                                DepthLayer::SHALLOW))
            == ResourceClass::INDICATED);
    REQUIRE(GetResourceClass(GetDepthConfidence(grid, tray, 3, 3,
                                                DepthLayer::DEEP))
            == ResourceClass::UNCLASSIFIED);
}

TEST_CASE("the ground is vertically continuous", "[field][generator]")
{
    // The generator seeds one 3D field per (cell, resource) -- depth is NOT
    // in the seed -- so adjacent layers must correlate. Four independent
    // layers was the defect that made aiming worthless.
    //
    // Measured, not hoped: the statistic is the Pearson correlation of the
    // yield fields of layers 0 and 1 across the whole lattice. A global-peak
    // drift test was the 8x8 version, and the finer lattice broke it for a
    // reason worth recording -- with two shoots per field the global maximum
    // can switch shoots between layers, reading as a huge "drift" over
    // ground that is perfectly continuous. Correlation sees through that.
    ResourceManager rm(PLANET_SIZE, SECT_CORE_RADIUS * 2.0f);
    rm.GenerateResourceMap(4242u);

    int strong = 0, cells = 0;
    for (int gx = 4; gx < 16; gx += 3)
    {
        for (int gy = 4; gy < 16; gy += 3)
        {
            ProspectingGrid grid(3, gx, gy, rm);

            // A resource that actually exists in BOTH layers -- picking the
            // first surface element compared nine of these cells against a
            // FLAT deeper layer (variance zero, correlation meaningless),
            // which the old peak-drift test silently did too.
            ResourceType type = ResourceType::Fe;
            {
                double bestVar = -1.0;
                for (const auto& kv :
                     grid.GetGroundTruth(0, 0, DepthLayer::SURFACE))
                {
                    double v0 = 0, v1 = 0, m0 = 0, m1 = 0;
                    int nn = 0;
                    for (int y = 0; y < PROSPECTING_GRID_SIZE; y += 2)
                        for (int x = 0; x < PROSPECTING_GRID_SIZE; x += 2)
                        {
                            double a = GetSubCellYield(grid, x, y,
                                                       DepthLayer::SURFACE, kv.first);
                            double b = GetSubCellYield(grid, x, y,
                                                       DepthLayer::SHALLOW, kv.first);
                            m0 += a; m1 += b; v0 += a * a; v1 += b * b; nn++;
                        }
                    m0 /= nn; m1 /= nn;
                    double var = std::min(v0 / nn - m0 * m0, v1 / nn - m1 * m1);
                    if (var > bestVar) { bestVar = var; type = kv.first; }
                }
            }

            const int n = PROSPECTING_GRID_SIZE * PROSPECTING_GRID_SIZE;
            double sa = 0, sb = 0, saa = 0, sbb = 0, sab = 0;
            for (int y = 0; y < PROSPECTING_GRID_SIZE; y++)
            {
                for (int x = 0; x < PROSPECTING_GRID_SIZE; x++)
                {
                    double a = GetSubCellYield(grid, x, y, DepthLayer::SURFACE, type);
                    double b = GetSubCellYield(grid, x, y, DepthLayer::SHALLOW, type);
                    sa += a; sb += b; saa += a * a; sbb += b * b; sab += a * b;
                }
            }
            double cov = sab / n - (sa / n) * (sb / n);
            double va = saa / n - (sa / n) * (sa / n);
            double vb = sbb / n - (sb / n) * (sb / n);
            double r = (va > 1e-12 && vb > 1e-12) ? cov / std::sqrt(va * vb) : 0.0;
            if (r > 0.35) strong++;
            cells++;
        }
    }
    REQUIRE(cells >= 16);
    REQUIRE(strong * 4 >= cells * 3);   // a strong positive in >= 3/4 of cells
}
