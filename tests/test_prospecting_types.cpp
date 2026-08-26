#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"

TEST_CASE("GetConfidenceLevel maps ranges correctly", "[types]")
{
    REQUIRE(GetConfidenceLevel(0.0f) == ConfidenceLevel::VERY_LOW);
    REQUIRE(GetConfidenceLevel(0.10f) == ConfidenceLevel::VERY_LOW);
    REQUIRE(GetConfidenceLevel(0.20f) == ConfidenceLevel::VERY_LOW);
    REQUIRE(GetConfidenceLevel(0.21f) == ConfidenceLevel::LOW);
    REQUIRE(GetConfidenceLevel(0.40f) == ConfidenceLevel::LOW);
    REQUIRE(GetConfidenceLevel(0.41f) == ConfidenceLevel::MODERATE);
    REQUIRE(GetConfidenceLevel(0.60f) == ConfidenceLevel::MODERATE);
    REQUIRE(GetConfidenceLevel(0.61f) == ConfidenceLevel::HIGH);
    REQUIRE(GetConfidenceLevel(0.80f) == ConfidenceLevel::HIGH);
    REQUIRE(GetConfidenceLevel(0.81f) == ConfidenceLevel::CERTAIN);
    REQUIRE(GetConfidenceLevel(1.0f) == ConfidenceLevel::CERTAIN);
}

TEST_CASE("GetGlowLevel maps to 0-4", "[types]")
{
    REQUIRE(GetGlowLevel(0.0f) == 0);
    REQUIRE(GetGlowLevel(0.30f) == 1);
    REQUIRE(GetGlowLevel(0.50f) == 2);
    REQUIRE(GetGlowLevel(0.70f) == 3);
    REQUIRE(GetGlowLevel(0.90f) == 4);
}

TEST_CASE("GetSizeLevel maps richness to quartiles", "[types]")
{
    REQUIRE(GetSizeLevel(0.0f) == 1);
    REQUIRE(GetSizeLevel(0.24f) == 1);
    REQUIRE(GetSizeLevel(0.25f) == 2);
    REQUIRE(GetSizeLevel(0.49f) == 2);
    REQUIRE(GetSizeLevel(0.50f) == 3);
    REQUIRE(GetSizeLevel(0.74f) == 3);
    REQUIRE(GetSizeLevel(0.75f) == 4);
    REQUIRE(GetSizeLevel(1.0f) == 4);
}

TEST_CASE("GetPrimaryShapeFamily maps depth to family", "[types]")
{
    REQUIRE(GetPrimaryShapeFamily(DepthLayer::SURFACE) == ShapeFamily::ANGULAR_CHUNKS);
    REQUIRE(GetPrimaryShapeFamily(DepthLayer::SHALLOW) == ShapeFamily::ROUNDED_NODULES);
    REQUIRE(GetPrimaryShapeFamily(DepthLayer::MID) == ShapeFamily::LAYERED_SLABS);
    REQUIRE(GetPrimaryShapeFamily(DepthLayer::DEEP) == ShapeFamily::CRYSTALLINE_SHARDS);
}

TEST_CASE("GetElementColor returns distinct colors for each element", "[types]")
{
    Color fe = GetElementColor(ResourceType::Fe);
    Color ti = GetElementColor(ResourceType::Ti);
    Color si = GetElementColor(ResourceType::Si);

    REQUIRE(fe.r != ti.r);
    REQUIRE(fe.r != si.r);
    REQUIRE(ti.r != si.r);
    REQUIRE(fe.a == 255);
}

TEST_CASE("GetDepthLayerInfo returns valid names", "[types]")
{
    auto& info = GetDepthLayerInfo(DepthLayer::SURFACE);
    REQUIRE(std::string(info.name) == "Regolith");

    auto& deep = GetDepthLayerInfo(DepthLayer::DEEP);
    REQUIRE(std::string(deep.name) == "Intact Bedrock");
}

TEST_CASE("every layer is accessible at every tier", "[types]")
{
    // Depth is ungated -- MAX_DEPTH_PER_TIER is all fours, kept as a table
    // only so its call sites need no churn.
    for (int tier = 0; tier <= 3; tier++)
        for (int d = 0; d < 4; d++)
            REQUIRE(IsLayerAccessible(tier, static_cast<DepthLayer>(d)));
}

TEST_CASE("the lattice is fixed; tier changes reach, not size", "[types]")
{
    // The grid is never reallocated, which is what lets survey data and
    // collected samples survive a tier upgrade.
    for (int tier = 0; tier <= 3; tier++)
    {
        REQUIRE(GetGridSizeForTier(tier) == PROSPECTING_GRID_SIZE);
    }

    // What tier actually buys is a wider ring out from the sect at the centre.
    REQUIRE(GetReachForTier(0) == 2);
    REQUIRE(GetReachForTier(1) == 4);
    REQUIRE(GetReachForTier(2) == 6);
    REQUIRE(GetReachForTier(3) == 8);

    // Even sizes nest, so every tier-up lights a complete ring around what
    // the previous tier could already see.
    for (int tier = 0; tier < 3; tier++)
    {
        for (int y = 0; y < PROSPECTING_GRID_SIZE; y++)
        {
            for (int x = 0; x < PROSPECTING_GRID_SIZE; x++)
            {
                if (IsSubCellInReach(x, y, tier))
                {
                    REQUIRE(IsSubCellInReach(x, y, tier + 1));
                }
            }
        }
    }
}

TEST_CASE("GetTrayCapacityForTier returns correct capacities", "[types]")
{
    REQUIRE(GetTrayCapacityForTier(0) == 4);
    REQUIRE(GetTrayCapacityForTier(1) == 8);
    REQUIRE(GetTrayCapacityForTier(2) == 12);
    REQUIRE(GetTrayCapacityForTier(3) == 16);
}

TEST_CASE("Sample::IsElementRevealed checks confidence > 0", "[sample]")
{
    Sample s = MakeSampleWithConfidence(0.5f, 0.0f);
    REQUIRE(s.IsElementRevealed(ResourceType::Fe) == true);
    REQUIRE(s.IsElementRevealed(ResourceType::Si) == false);
    REQUIRE(s.IsElementRevealed(ResourceType::Ti) == false);
}

TEST_CASE("Sample::GetRevealedValue returns true value when confident", "[sample]")
{
    Sample s = MakeSampleWithConfidence(0.8f, 0.0f);
    REQUIRE_THAT(s.GetRevealedValue(ResourceType::Fe),
                 Catch::Matchers::WithinAbs(0.40f, 0.001f));
    REQUIRE_THAT(s.GetRevealedValue(ResourceType::Si),
                 Catch::Matchers::WithinAbs(0.0f, 0.001f));
}

TEST_CASE("Sample::GetAggregateConfidence is abundance-weighted", "[sample]")
{
    Sample s = MakeDummySample();
    s.elementConfidence[ResourceType::Fe] = 0.80f;
    s.elementConfidence[ResourceType::Si] = 0.50f;
    // Ti = 0.10 abundance, above 0.05 threshold, but no confidence set

    // Expected: (0.80 * 0.40 + 0.50 * 0.25 + 0.0 * 0.10) / (0.40 + 0.25 + 0.10)
    // = (0.32 + 0.125 + 0.0) / 0.75 = 0.445 / 0.75 = 0.5933...
    float expected = (0.80f * 0.40f + 0.50f * 0.25f) / (0.40f + 0.25f + 0.10f);
    REQUIRE_THAT(s.GetAggregateConfidence(),
                 Catch::Matchers::WithinAbs(expected, 0.001f));
}
