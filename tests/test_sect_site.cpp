// The ground under the sect base and the base's art are laid out separately:
// SectLevelSite (terrain_synthesis.cpp) levels the terrain in km, and SectArt
// (sect_art.cpp) draws DomeForge's layout in px. They agreed by hand once and
// drifted (the site measured out 420-540 px from the centre, outside the ring
// road). This holds them together.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "sect_art.h"
#include "terrain_synthesis.h"

#include <cmath>

using Catch::Matchers::WithinRel;

TEST_CASE("The levelled site sits under the sect base", "[sect][site]")
{
    const float pxPerKm = 256.0f;   // 1280x720: the sect view's ground scale
    const SectArt::Frame f = SectArt::MakeFrame(Vector2{0.0f, 0.0f}, pxPerKm);
    TerrainSiteDisturbance colony;
    colony.enabled = true;
    const TerrainSiteDisturbance site = SectLevelSite(colony);

    SECTION("the ring road is where SECT_RING_ROAD_KM says")
    {
        CHECK_THAT(f.ringRoadR / pxPerKm, WithinRel(SECT_RING_ROAD_KM, 0.01f));
    }
    SECTION("the worked spots are on the unit domes")
    {
        for (int i = 0; i < SectArt::UNIT_SLOTS; i++)
        {
            const float r = std::hypot(f.unit[i].x, f.unit[i].y) / pxPerKm;
            CHECK_THAT(r, WithinRel(site.ringRadiusKm, 0.02f));
        }
    }
    SECTION("the core spot covers the core and its collar")
    {
        CHECK_THAT(site.coreRadiusKm, WithinRel((f.coreRimR + f.collar) / pxPerKm, 0.03f));
    }
    SECTION("the footprint reaches the ring road's outer edge, and not far past it")
    {
        const float outerKm = f.ringRoadOuterR / pxPerKm;
        CHECK(site.footprintRadiusKm >= outerKm - 0.01f);
        CHECK(site.footprintRadiusKm <= outerKm + 0.10f);
    }
    SECTION("a colony-level site gets no footprint of its own")
    {
        CHECK(colony.footprintRadiusKm == 0.0f);
    }
}
