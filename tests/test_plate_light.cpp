#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"
#include <cmath>

// The block model's focus law (prospecting_constants.h, PLATE_REST_LIGHT):
// the surface plate is pinned lit, the three below rest dim, and the one
// under the pointer rises to full. The rise is eased, so it is state that has
// to live somewhere across frames -- here, on the facade.

static ResourceManager& PlateRM()
{
    static ResourceManager rm(20, 100.0f);
    return rm;
}
static ProspectingSystem MakeSys()
{
    return ProspectingSystem(3, 10, 10, PlateRM());
}
static void Settle(ProspectingSystem& sys, int hovered, int active = -1,
                   float seconds = 1.0f)
{
    for (float t = 0.0f; t < seconds; t += 1.0f / 60.0f)
        sys.UpdatePlateLight(hovered, active, 1.0f / 60.0f);
}

TEST_CASE("the surface plate is always lit, the rest rest dim", "[platelight]")
{
    ProspectingSystem sys = MakeSys();
    Settle(sys, -1);                              // pointer nowhere near

    REQUIRE(sys.plateLight[0] > 0.99f);           // pinned, with no hover at all
    for (int L = 1; L < 4; L++)
    {
        REQUIRE(sys.plateLight[L] < 0.6f);        // clearly receded
        REQUIRE(sys.plateLight[L] > 0.3f);        // but not hidden: dim means quiet
        REQUIRE(std::fabs(sys.plateLight[L] - PLATE_REST_LIGHT[L]) < 0.01f);
    }
    // depth still reads when nothing is hovered
    REQUIRE(sys.plateLight[1] > sys.plateLight[2]);
    REQUIRE(sys.plateLight[2] > sys.plateLight[3]);
}

TEST_CASE("hovering a plate lights that plate and no other", "[platelight]")
{
    ProspectingSystem sys = MakeSys();
    Settle(sys, 2);

    REQUIRE(sys.plateLight[2] > 0.99f);           // the one under the pointer
    REQUIRE(sys.plateLight[0] > 0.99f);           // the surface, as ever
    REQUIRE(std::fabs(sys.plateLight[1] - PLATE_REST_LIGHT[1]) < 0.01f);
    REQUIRE(std::fabs(sys.plateLight[3] - PLATE_REST_LIGHT[3]) < 0.01f);

    // and it goes back down when the pointer leaves
    Settle(sys, -1);
    REQUIRE(std::fabs(sys.plateLight[2] - PLATE_REST_LIGHT[2]) < 0.01f);
}

TEST_CASE("the plate light eases, and settles quickly", "[platelight]")
{
    // A snap would flicker as the pointer crosses the stack; a slow fade
    // would lag the pointer. One frame moves it part way, and it is
    // essentially there within a few.
    ProspectingSystem sys = MakeSys();
    float start = sys.plateLight[3];

    sys.UpdatePlateLight(3, -1, 1.0f / 60.0f);
    float afterOne = sys.plateLight[3];
    REQUIRE(afterOne > start);                    // it moved
    REQUIRE(afterOne < 0.9f);                     // but did not snap

    for (int i = 0; i < 7; i++) sys.UpdatePlateLight(3, -1, 1.0f / 60.0f);
    REQUIRE(sys.plateLight[3] > 0.9f);            // ~0.13 s and it is there

    // framerate independence: the same wall clock gets to the same place
    ProspectingSystem slow = MakeSys();
    ProspectingSystem fast = MakeSys();
    for (int i = 0; i < 6; i++)  slow.UpdatePlateLight(1, -1, 1.0f / 30.0f);
    for (int i = 0; i < 24; i++) fast.UpdatePlateLight(1, -1, 1.0f / 120.0f);
    REQUIRE(std::fabs(slow.plateLight[1] - fast.plateLight[1]) < 0.02f);
}

TEST_CASE("the plate being cut lights, pointer or no pointer", "[platelight]")
{
    // The stratum the bit is in is live work. It rim-lights either way, but a
    // dim plate with a bright rim reads as marked-but-inactive, and it is the
    // one plate the player is actually operating on.
    ProspectingSystem sys = MakeSys();
    Settle(sys, -1, 3);                           // drilling DEEP, pointer away

    REQUIRE(sys.plateLight[3] > 0.99f);
    REQUIRE(std::fabs(sys.plateLight[1] - PLATE_REST_LIGHT[1]) < 0.01f);
    REQUIRE(std::fabs(sys.plateLight[2] - PLATE_REST_LIGHT[2]) < 0.01f);

    // hovering elsewhere lights that one too -- both are live at once
    Settle(sys, 1, 3);
    REQUIRE(sys.plateLight[1] > 0.99f);
    REQUIRE(sys.plateLight[3] > 0.99f);
    REQUIRE(std::fabs(sys.plateLight[2] - PLATE_REST_LIGHT[2]) < 0.01f);

    // and when the hole ends, only the pointer holds a plate up
    Settle(sys, 1, -1);
    REQUIRE(sys.plateLight[1] > 0.99f);
    REQUIRE(std::fabs(sys.plateLight[3] - PLATE_REST_LIGHT[3]) < 0.01f);
}
