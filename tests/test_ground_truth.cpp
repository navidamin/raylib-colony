// The ground is a function of where you stand.
//
// ResourceManager answers for any point on the Moon, generated from the
// point itself: the same place always answers the same, a sect's width
// away answers differently, and the answer leans the way the region's
// composition says. These pin those three promises, and the depth-bias
// split and founding floor the extraction loop relies on.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include "region_identity.h"

#include <cmath>

namespace
{
    float Quantity(const std::vector<std::pair<ResourceType, float>>& resources, ResourceType type)
    {
        for (const auto& [t, q] : resources)
        {
            if (t == type) return q;
        }
        return 0.0f;
    }

    // Unnamed highland ground on the far side, high enough that the
    // elevation proxy (no named feature there) reads it as highland.
    LunarPoint FarsideHighland()
    {
        LunarPoint p;
        p.latDeg = 30.0;
        p.lonDeg = 170.0;
        return p;
    }

    // A named highland feature: the region card's own composition.
    LunarPoint Tycho()
    {
        LunarPoint p;
        p.latDeg = -43.3;
        p.lonDeg = -11.4;
        return p;
    }

    LunarPoint SouthPole()
    {
        LunarPoint p;
        p.latDeg = -88.5;
        p.lonDeg = 30.0;
        return p;
    }
}

TEST_CASE("The same place always answers the same", "[ground]")
{
    ResourceManager a(42);
    ResourceManager b(42);

    auto ra = a.GetResourcesAt(TestPoint());
    auto rb = b.GetResourcesAt(TestPoint());
    REQUIRE(!ra.empty());
    REQUIRE(ra.size() == rb.size());
    for (size_t i = 0; i < ra.size(); i++)
    {
        REQUIRE(ra[i].first == rb[i].first);
        REQUIRE_THAT(ra[i].second, Catch::Matchers::WithinAbs(rb[i].second, 1e-4f));
    }

    // Asking twice of one manager is the same ground, not a regeneration.
    const auto& g1 = a.GroundAt(TestPoint());
    const auto& g2 = a.GroundAt(TestPoint());
    REQUIRE(&g1 == &g2);
}

TEST_CASE("A sect's width away is different ground", "[ground]")
{
    ResourceManager rm(42);
    auto here = rm.GetResourcesAt(TestPoint(0, 0));
    auto next = rm.GetResourcesAt(TestPoint(1, 0));

    bool differs = false;
    for (const auto& [type, q] : here)
    {
        if (std::fabs(q - Quantity(next, type)) > 1.0f) differs = true;
    }
    REQUIRE(differs);

    // ...but not wildly: both are Imbrium, so iron leads in both.
    REQUIRE(Quantity(here, ResourceType::Fe) > Quantity(here, ResourceType::Al));
    REQUIRE(Quantity(next, ResourceType::Fe) > Quantity(next, ResourceType::Al));
}

TEST_CASE("A different world seed is a different Moon", "[ground]")
{
    ResourceManager a(42);
    ResourceManager b(43);
    auto ra = a.GetResourcesAt(TestPoint());
    auto rb = b.GetResourcesAt(TestPoint());

    bool differs = false;
    for (const auto& [type, q] : ra)
    {
        if (std::fabs(q - Quantity(rb, type)) > 1.0f) differs = true;
    }
    REQUIRE(differs);
}

TEST_CASE("Mare out-irons highland; highland out-aluminiums mare", "[ground]")
{
    ResourceManager rm(42);
    auto mare = rm.GetResourcesAt(TestPoint());
    REQUIRE(rm.GroundAt(TestPoint()).region.isMare);

    for (const LunarPoint& where : { Tycho(), FarsideHighland() })
    {
        auto highland = rm.GetResourcesAt(where);
        REQUIRE_FALSE(rm.GroundAt(where).region.isMare);

        REQUIRE(Quantity(mare, ResourceType::Fe) > Quantity(highland, ResourceType::Fe));
        REQUIRE(Quantity(mare, ResourceType::Ti) > Quantity(highland, ResourceType::Ti));
        REQUIRE(Quantity(highland, ResourceType::Al) > Quantity(mare, ResourceType::Al));
        REQUIRE(Quantity(highland, ResourceType::Ca) > Quantity(mare, ResourceType::Ca));
    }
}

TEST_CASE("The poles hold more hydrogen than the equator", "[ground]")
{
    ResourceManager rm(42);
    auto polar = rm.GetResourcesAt(SouthPole());
    auto equatorial = rm.GetResourcesAt(TestPoint());
    REQUIRE(Quantity(polar, ResourceType::H2) > Quantity(equatorial, ResourceType::H2));
    REQUIRE(rm.SurveyAt(SouthPole()).hydrogenSignal > rm.SurveyAt(TestPoint()).hydrogenSignal);
}

TEST_CASE("The depth layers are the one deposit seen through the bias table", "[ground]")
{
    // Layers are not a partition: each is the deposit scaled by the
    // element's bias for that depth (shallow is the reference, 1.0), so
    // iron reads richer deep down and hydrogen richer at the surface.
    ResourceManager rm(42);
    auto whole = rm.GetResourcesAt(TestPoint());
    REQUIRE(!whole.empty());

    for (const auto& [type, total] : whole)
    {
        for (int d = 0; d < 4; d++)
        {
            DepthLayer layer = static_cast<DepthLayer>(d);
            float bias = ResourceManager::DepthBias(type, layer);
            REQUIRE(bias > 0.0f);
            float q = Quantity(rm.GetResourcesAtLayer(TestPoint(), layer), type);
            REQUIRE_THAT(q, Catch::Matchers::WithinRel(total * bias, 0.001f));
        }
        REQUIRE_THAT(ResourceManager::DepthBias(type, DepthLayer::SHALLOW),
                     Catch::Matchers::WithinAbs(1.0f, 1e-6f));
    }
    REQUIRE(ResourceManager::DepthBias(ResourceType::Fe, DepthLayer::DEEP)
            > ResourceManager::DepthBias(ResourceType::Fe, DepthLayer::SURFACE));
    REQUIRE(ResourceManager::DepthBias(ResourceType::H2, DepthLayer::SURFACE)
            > ResourceManager::DepthBias(ResourceType::H2, DepthLayer::DEEP));
}

TEST_CASE("Depletion is remembered at the place", "[ground]")
{
    ResourceManager rm(42);
    float before = Quantity(rm.GetResourcesAt(TestPoint()), ResourceType::Fe);
    REQUIRE(before > 100.0f);

    rm.Deplete(TestPoint(), ResourceType::Fe, 100.0f);
    float after = Quantity(rm.GetResourcesAt(TestPoint()), ResourceType::Fe);
    REQUIRE_THAT(after, Catch::Matchers::WithinAbs(before - 100.0f, 1e-3f));

    // The next sect over is untouched.
    float neighbour = Quantity(rm.GetResourcesAt(TestPoint(1, 0)), ResourceType::Fe);
    REQUIRE(neighbour > 0.0f);
    REQUIRE(rm.GroundAt(TestPoint()).isExploited);
    REQUIRE_FALSE(rm.GroundAt(TestPoint(1, 0)).isExploited);
}

TEST_CASE("The founding floor leaves no first sect on empty ground", "[ground]")
{
    ResourceManager rm(42);
    rm.EnsureBasicResources(FarsideHighland());
    auto ground = rm.GetResourcesAt(FarsideHighland());
    REQUIRE(Quantity(ground, ResourceType::Fe) > 0.0f);
    REQUIRE(Quantity(ground, ResourceType::Si) > 0.0f);
    REQUIRE(Quantity(ground, ResourceType::O2) > 0.0f);
}

TEST_CASE("The survey names the region's archetype", "[ground]")
{
    ResourceManager rm(42);
    REQUIRE(rm.ArchetypeAt(TestPoint()) == rm.GroundAt(TestPoint()).region.archetype);
    // Imbrium is mare inside the Procellarum KREEP Terrane.
    SiteArchetype a = rm.ArchetypeAt(TestPoint());
    REQUIRE((a == SiteArchetype::MARE_INDUSTRIAL || a == SiteArchetype::KREEP_SCIENTIFIC));
    REQUIRE(rm.ArchetypeAt(SouthPole()) == SiteArchetype::POLAR_VOLATILE);
}
