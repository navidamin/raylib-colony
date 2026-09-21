// Founding through the descent, headless.
//
// The controller is driven by the shared script (SurveyScript), so what
// is verified is the shipping state machine; GameManager then founds
// from what it decided, exactly as the Engine does. Tests that need the
// elevation model skip themselves when it is absent.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "gamemanager.h"
#include "lunar_dem_shared.h"
#include "lunar_frame.h"
#include "site_selection_constants.h"
#include "survey_script.h"

#include <string>

using Catch::Matchers::WithinAbs;

namespace
{
const int W = 1280, H = 720;

LunarPoint At(double lat, double lon)
{
    LunarPoint p;
    p.latDeg = lat;
    p.lonDeg = lon;
    return p;
}

// Claim a place from the globe and descend twice, the cursor aimed at
// it all the way down. True once the controller stands on the site rung.
bool DescendTo(SiteSelectionController& c, const LolaDem* dem, double lat, double lon)
{
    SetOrbitalCamera(OrbitalCamera{});
    SurveyScript::FaceGlobe(lat, lon);
    if (!SurveyScript::ClickAt(c, W, H, dem, lat, lon)) return false;      // claim
    if (c.Level() != 1) return false;
    if (!SurveyScript::ClickAt(c, W, H, dem, lat, lon)) return false;      // descend
    if (!c.AtSiteRung()) return false;
    return SurveyScript::Aim(c, W, H, dem, lat, lon);
}
}

TEST_CASE("A red verdict refuses founding", "[founding][assets]")
{
    const LolaDem* dem = GetLunarDem();
    if (!dem) SKIP("LOLA model not present");

    // Tycho's floor: 28 degrees of mean slope over a 5 km footprint.
    SiteSelectionController c;
    REQUIRE(DescendTo(c, dem, -43.3, -11.4));
    REQUIRE(c.HaveVerdict());
    REQUIRE_FALSE(c.Verdict().allowed);
    REQUIRE(c.Verdict().reason[0] != '\0');

    REQUIRE_FALSE(SurveyScript::ClickAt(c, W, H, dem, -43.3, -11.4));
    REQUIRE_FALSE(c.Founded());
    REQUIRE(c.AtSiteRung());
}

TEST_CASE("Founding through the descent puts the colony where the card said", "[founding][assets]")
{
    const LolaDem* dem = GetLunarDem();
    if (!dem) SKIP("LOLA model not present");

    // Mare Imbrium: flat basalt, green.
    SiteSelectionController c;
    REQUIRE(DescendTo(c, dem, 32.8, -15.6));
    REQUIRE(c.HaveVerdict());
    REQUIRE(c.Verdict().allowed);
    RegionIdentity card = c.Region();
    REQUIRE(std::string(card.name) == "Mare Imbrium");

    LunarPoint windowCentre = At(c.Cursor()->windowLatDeg, c.Cursor()->windowLonDeg);
    REQUIRE(SurveyScript::ClickAt(c, W, H, dem, 32.8, -15.6));
    REQUIRE(c.Founded());
    LunarPoint found = At(c.FoundLat(), c.FoundLon());
    REQUIRE_THAT(found.latDeg, WithinAbs(32.8, 0.05));
    REQUIRE_THAT(found.lonDeg, WithinAbs(-15.6, 0.05));

    // What the Engine does with that.
    GameManager gm;
    Colony* colony = gm.FoundColony(found, windowCentre, &card);
    REQUIRE(colony != nullptr);
    REQUIRE(gm.GetCurrentColony() == colony);
    REQUIRE_THAT(colony->GetCentre().latDeg, WithinAbs(windowCentre.latDeg, 1e-9));
    REQUIRE_THAT(colony->GetCentre().lonDeg, WithinAbs(windowCentre.lonDeg, 1e-9));
    REQUIRE(colony->GetSects().size() == 1);
    REQUIRE_THAT(colony->GetSects()[0]->GetPoint().latDeg, WithinAbs(found.latDeg, 1e-9));
    REQUIRE(colony->GetArchetype() == card.archetype);

    // The ground the sect stands on is the region the card described.
    const RegionIdentity& ground = gm.GetPlanet()->GetResourceManager().GroundAt(found).region;
    REQUIRE(std::string(ground.name) == std::string(card.name));
    REQUIRE_THAT(ground.fePct, WithinAbs(card.fePct, 1e-6f));

    // The descent landing on this window again finds the colony.
    REQUIRE(gm.ColonyInWindow(windowCentre, COLONY_WINDOW_KM) == colony);
    REQUIRE(gm.ColonyInWindow(LunarOffsetPoint(windowCentre, 60.0, 0.0), COLONY_WINDOW_KM) == nullptr);

    // Founding inside its territory is refused.
    REQUIRE(gm.FoundColony(LunarOffsetPoint(found, 1.0, 0.0)) == nullptr);
    REQUIRE(gm.GetColonies().size() == 1);
}

TEST_CASE("A sect keeps its spacing and stays inside the window", "[founding]")
{
    GameManager gm;
    LunarPoint imbrium = At(32.8, -15.6);
    Colony* colony = gm.FoundColony(imbrium);
    REQUIRE(colony != nullptr);

    // Too close to the first sect.
    REQUIRE(gm.FoundSect(LunarOffsetPoint(imbrium, 1.0, 0.0)) == nullptr);
    // Inside the window, a footprint away: fine.
    Sect* east = gm.FoundSect(LunarOffsetPoint(imbrium, 8.0, 0.0));
    REQUIRE(east != nullptr);
    REQUIRE(colony->GetSects().size() == 2);
    // Its local position is east of the centre, in 50 m units.
    REQUIRE(east->GetPosition().x > 150.0f);
    REQUIRE_THAT(east->GetPosition().y, WithinAbs(0.0f, 2.0f));
    // Outside the 25 km window.
    REQUIRE(gm.FoundSect(LunarOffsetPoint(imbrium, 20.0, 0.0)) == nullptr);
    REQUIRE(colony->GetSects().size() == 2);
}
