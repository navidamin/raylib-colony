// The site-selection controller and what it reads, headless.
//
// Runs from the repository root (tests/CMakeLists.txt sets the working
// directory) so zones.json and the LOLA model resolve by the same relative
// paths the game uses. Tests that need the model skip themselves when it
// is absent rather than fail: the game runs without it too.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "lunar_dem_shared.h"
#include "region_identity.h"
#include "site_selection_constants.h"
#include "site_selection_controller.h"
#include "site_verdict.h"
#include "terrain_synthesis.h"

#include <cmath>

using Catch::Matchers::WithinAbs;

namespace
{

const int W = 1200, H = 1200;

// A pointer resting on the screen centre, which on the default globe
// camera is the sub-point (0 N, 0 E): real ground, so a claim lands.
SurveyInput Centre()
{
    SurveyInput in;
    in.pointer = { W * 0.5f, H * 0.5f };
    in.instant = true;
    return in;
}

// Two frames: one to settle the pointer (a fresh pointer counts as a
// jump, which the touch rule turns into "aim only"), one that clicks.
void SettleThen(SiteSelectionController& c, SurveyInput in, const LolaDem* dem)
{
    SurveyInput settle = in;
    settle.click = false;
    settle.escape = false;
    c.Update(settle, W, H, dem, nullptr);
    c.Commit();
    c.Update(in, W, H, dem, nullptr);
    c.Commit();
}

} // namespace

TEST_CASE("JudgeSite names the first limit that fails", "[site-selection]")
{
    TerrainBuildability b;
    b.meanSlopeDeg = 2.0f; b.maxSlopeDeg = 10.0f; b.roughnessM = 5.0f;
    b.reliefM = 50.0f; b.isPsr = false;
    REQUIRE(JudgeSite(b).allowed);

    TerrainBuildability steep = b;
    steep.meanSlopeDeg = SITE_MAX_MEAN_SLOPE_DEG + 0.1f;
    REQUIRE_FALSE(JudgeSite(steep).allowed);
    REQUIRE(std::string(JudgeSite(steep).reason).find("mean slope") != std::string::npos);

    TerrainBuildability face = b;
    face.maxSlopeDeg = SITE_MAX_PEAK_SLOPE_DEG + 0.1f;
    REQUIRE_FALSE(JudgeSite(face).allowed);
    REQUIRE(std::string(JudgeSite(face).reason).find("local face") != std::string::npos);

    TerrainBuildability rough = b;
    rough.roughnessM = SITE_MAX_ROUGHNESS_M + 0.1f;
    REQUIRE_FALSE(JudgeSite(rough).allowed);

    TerrainBuildability relief = b;
    relief.reliefM = SITE_MAX_RELIEF_M + 0.1f;
    REQUIRE_FALSE(JudgeSite(relief).allowed);

    TerrainBuildability dark = b;
    dark.isPsr = true;
    REQUIRE_FALSE(JudgeSite(dark).allowed);
    REQUIRE(std::string(JudgeSite(dark).reason) == "PERMANENT SHADOW");
}

TEST_CASE("Archetype descriptors cover every archetype", "[site-selection]")
{
    const SiteArchetype all[] = {
        SiteArchetype::MARE_INDUSTRIAL, SiteArchetype::HIGHLAND_CONSTRUCTION,
        SiteArchetype::POLAR_VOLATILE, SiteArchetype::KREEP_SCIENTIFIC,
        SiteArchetype::LAVA_TUBE, SiteArchetype::MIXED };
    for (SiteArchetype a : all)
    {
        const SiteArchetypeDescriptor& d = GetSiteArchetypeDescriptor(a);
        REQUIRE(d.archetype == a);
        REQUIRE(d.name[0] != '\0');
    }
    REQUIRE(std::string(GetSiteArchetypeDescriptor(SiteArchetype::MARE_INDUSTRIAL).name)
            == "MARE INDUSTRIAL");
}

TEST_CASE("Named regions identify with the real feature", "[site-selection][assets]")
{
    const LolaDem* dem = GetLunarDem();
    if (dem == nullptr)
    {
        WARN("LOLA model not present; identity falls back to highland everywhere");
    }

    // Mare Imbrium: PKT mare, named, iron-rich.
    RegionIdentity imbrium = IdentifyRegion(dem, 32.8, -15.6);
    REQUIRE(std::string(imbrium.name) == "Mare Imbrium");
    REQUIRE(std::string(imbrium.terrane) == "Procellarum KREEP Terrane");
    REQUIRE(imbrium.fePct > 10.0f);
    if (dem) REQUIRE(imbrium.archetype == SiteArchetype::MARE_INDUSTRIAL);

    // Tycho: a named highland crater. Its floor is 4 km down, which the
    // elevation proxy alone would read as mare; the dataset's rock record
    // (impact melt) is what says otherwise.
    RegionIdentity tycho = IdentifyRegion(dem, -43.3, -11.4);
    REQUIRE(std::string(tycho.name) == "Tycho");
    REQUIRE_FALSE(tycho.isMare);
    REQUIRE(tycho.archetype == SiteArchetype::HIGHLAND_CONSTRUCTION);
    REQUIRE(std::string(tycho.rock).find("impact-melt") != std::string::npos);

    // The south pole: polar whatever the ground.
    RegionIdentity pole = IdentifyRegion(dem, -89.7, 110.0);
    REQUIRE(pole.archetype == SiteArchetype::POLAR_VOLATILE);
    REQUIRE(std::string(pole.terrane) == "Feldspathic Highlands (polar)");

    // Unnamed far-side ground still answers.
    RegionIdentity farside = IdentifyRegion(dem, 5.0, 175.0);
    REQUIRE(farside.name[0] != '\0');
}

TEST_CASE("The descent is a stack: claim, descend, ascend, reset", "[site-selection]")
{
    SetOrbitalCamera(OrbitalCamera{});
    const LolaDem* dem = GetLunarDem();
    SiteSelectionController c;
    REQUIRE(c.Level() == 0);
    REQUIRE_FALSE(c.Claimed());

    // A settled click on the globe claims and lands at the district.
    SurveyInput click = Centre();
    click.click = true;
    SettleThen(c, click, dem);
    REQUIRE(c.Level() == 1);
    REQUIRE(c.Claimed());
    REQUIRE(c.Cursor()->windowSpanKm == GetSurveyLadder()[1].windowSpanKm);
    REQUIRE_THAT(c.Cursor()->windowLatDeg, WithinAbs(0.0, 0.2));
    REQUIRE_THAT(c.Cursor()->windowLonDeg, WithinAbs(0.0, 0.2));

    // The region card is fixed from the claim.
    std::string claimedName = c.Region().name;
    REQUIRE(std::string(c.Shown().name) == claimedName);

    // A click on the district descends to the site rung.
    SettleThen(c, click, dem);
    REQUIRE(c.Level() == 2);
    REQUIRE(c.AtSiteRung());
    REQUIRE(c.Cursor()->footprintKm == SURVEY_BUILD_FOOTPRINT_KM);
    REQUIRE(std::string(c.Shown().name) == claimedName);

    // Escape backs out one rung with the region still claimed...
    SurveyInput esc = Centre();
    esc.escape = true;
    c.Update(esc, W, H, dem, nullptr);
    REQUIRE(c.Commit());
    REQUIRE(c.Level() == 1);
    REQUIRE(c.Claimed());

    // ...and out of the district the claim is released.
    c.Update(esc, W, H, dem, nullptr);
    REQUIRE(c.Commit());
    REQUIRE(c.Level() == 0);
    REQUIRE_FALSE(c.Claimed());

    // Nothing happens below the globe.
    c.Update(esc, W, H, dem, nullptr);
    REQUIRE_FALSE(c.Commit());
    REQUIRE(c.Level() == 0);
}

TEST_CASE("A jumped pointer aims but does not click", "[site-selection]")
{
    SetOrbitalCamera(OrbitalCamera{});
    SiteSelectionController c;
    SurveyInput tap = Centre();
    tap.click = true;
    // First frame: the pointer arrives and clicks at once. A touch.
    c.Update(tap, W, H, nullptr, nullptr);
    REQUIRE_FALSE(c.Commit());
    REQUIRE(c.Level() == 0);
    REQUIRE(c.TouchStyle());
    // Second tap in the same place commits.
    c.Update(tap, W, H, nullptr, nullptr);
    REQUIRE(c.Commit());
    REQUIRE(c.Level() == 1);
}

TEST_CASE("Founding needs a green verdict and records the ground", "[site-selection][assets]")
{
    const LolaDem* dem = GetLunarDem();
    if (dem == nullptr)
    {
        SKIP("LOLA model not present");
    }
    SetOrbitalCamera(OrbitalCamera{});
    SiteSelectionController c;
    SurveyInput click = Centre();
    click.click = true;
    SettleThen(c, click, dem);          // claim
    SettleThen(c, click, dem);          // descend
    REQUIRE(c.AtSiteRung());

    // On the site rung a settled click either founds (green) or does
    // nothing (red); either way the verdict is measured and named.
    SettleThen(c, click, dem);
    REQUIRE(c.HaveVerdict());
    REQUIRE(c.Verdict().reason[0] != '\0');
    if (c.Verdict().allowed)
    {
        REQUIRE(c.Founded());
        REQUIRE_THAT(c.FoundLat(), WithinAbs(c.HoverLat(), 1e-9));
        REQUIRE_THAT(c.FoundLon(), WithinAbs(c.HoverLon(), 1e-9));
        REQUIRE(c.FoundBuildability().meanSlopeDeg <= SITE_MAX_MEAN_SLOPE_DEG);
        // Escape after founding resets to the globe.
        SurveyInput esc = Centre();
        esc.escape = true;
        c.Update(esc, W, H, dem, nullptr);
        REQUIRE(c.Commit());
        REQUIRE(c.Level() == 0);
        REQUIRE_FALSE(c.Founded());
    }
    else
    {
        REQUIRE_FALSE(c.Founded());
        REQUIRE(c.AtSiteRung());
    }
}

TEST_CASE("Region card hit rows follow the shared layout", "[site-selection]")
{
    int count = 0;
    const SurveyCardRow* rows = GetRegionCardRows(&count);
    REQUIRE(count == 5);
    const int px = 16, py = 64, pw = 336;
    for (int i = 0; i < count; i++)
    {
        Vector2 m = { (float)(px + 20), (float)(py + rows[i].yOffset + 5) };
        REQUIRE(std::string(SurveyRegionCardHintAt(m, px, py, pw)) == rows[i].hintKey);
    }
    Vector2 off = { (float)(px + pw + 10), (float)(py + 60) };
    REQUIRE(SurveyRegionCardHintAt(off, px, py, pw) == nullptr);
}
