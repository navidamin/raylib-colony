#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"
#include <algorithm>

// The prescribed line: aim from a collar on the surface toward a cell at a
// chosen layer, drill it over time, and every layer it crosses is cored at
// the cell the line actually passes through. See
// docs/design/prospecting/prototypes/drill-dock.html and
// docs/design/graphics/dark-plating.md section 9.

static ResourceManager& TestRM()
{
    static ResourceManager rm(20, 100.0f);
    return rm;
}
static ProspectingSystem MakeSystem()
{
    return ProspectingSystem(3, 10, 10, TestRM());
}

TEST_CASE("a line hole cores each crossing as the bit passes it", "[linehole]")
{
    ProspectingSystem sys = MakeSystem();
    sys.StartAim(2, 5);
    sys.AimAt(3, 5, 2);
    REQUIRE(sys.lineHole.state == LineHoleState::AIMING);
    REQUIRE(sys.CommitHole());
    REQUIRE(sys.lineHole.state == LineHoleState::DRILLING);

    // crossing cells drift from the collar toward the aim, layer by layer
    int cx = 0, cy = 0;
    sys.GetCrossingCell(0, cx, cy);
    REQUIRE(cx == 2); REQUIRE(cy == 5);          // near the collar at 6 m
    sys.GetCrossingCell(3, cx, cy);
    REQUIRE(cx == 5); REQUIRE(cy == 2);          // the aimed cell at 94 m

    // nothing cored before the bit reaches the first layer centre
    REQUIRE_FALSE(sys.lineHole.cored[0]);

    // drill it out in small steps -- cores must land IN ORDER, mid-run
    bool doneEarly = false;
    int coredWhenSurfaceDone = -1;
    for (int i = 0; i < 2000 && sys.lineHole.state == LineHoleState::DRILLING; i++)
    {
        doneEarly = sys.UpdateLineHole(0.05f) && sys.lineHole.depthM < 100.0f;
        if (sys.lineHole.cored[0] && coredWhenSurfaceDone < 0)
        {
            coredWhenSurfaceDone = sys.lineHole.cored[3] ? 1 : 0;
        }
    }
    REQUIRE_FALSE(doneEarly);
    REQUIRE(coredWhenSurfaceDone == 0);          // surface cored before deep
    REQUIRE(sys.lineHole.state == LineHoleState::DONE);

    for (int L = 0; L < 4; L++)
    {
        sys.GetCrossingCell(L, cx, cy);
        REQUIRE(sys.GetGrid().GetSubCell(cx, cy).HasCore(L));
        REQUIRE(sys.lineHole.cored[L]);
    }

    // one specimen per hole, from the deepest interval
    REQUIRE(sys.GetTray().GetCount() == 1);
    REQUIRE(sys.GetTray().GetSampleByIndex(0)->depthLayer == DepthLayer::DEEP);
}

TEST_CASE("a surface release is only ever a click", "[linehole]")
{
    ProspectingSystem sys = MakeSystem();
    sys.StartAim(4, 4);
    sys.AimAt(0, 4, 4);
    REQUIRE(sys.lineHole.targetLayer == 0);
    sys.CancelAim();
    REQUIRE(sys.lineHole.state == LineHoleState::NONE);
    // and nothing was cored by any of it
    REQUIRE_FALSE(sys.GetGrid().GetSubCell(4, 4).HasCore(0));
}

TEST_CASE("aiming is refused while the string is down", "[linehole]")
{
    ProspectingSystem sys = MakeSystem();
    sys.StartAim(1, 1);
    sys.AimAt(2, 3, 3);
    sys.CommitHole();
    sys.UpdateLineHole(1.0f);
    REQUIRE(sys.lineHole.state == LineHoleState::DRILLING);

    sys.StartAim(6, 6);                           // must not reset the hole
    REQUIRE(sys.lineHole.state == LineHoleState::DRILLING);
    REQUIRE(sys.lineHole.collarX == 1);
}

TEST_CASE("the crossing cell never leaves the lattice", "[linehole]")
{
    ProspectingSystem sys = MakeSystem();
    sys.StartAim(0, 0);
    sys.AimAt(1, 7, 7);                           // steep drift, extrapolates far
    sys.CommitHole();
    while (sys.lineHole.state == LineHoleState::DRILLING) sys.UpdateLineHole(0.5f);
    for (int L = 0; L <= sys.lineHole.targetLayer; L++)
    {
        int cx = -1, cy = -1;
        sys.GetCrossingCell(L, cx, cy);
        REQUIRE(cx >= 0); REQUIRE(cx < sys.GetGrid().GetGridSize());
        REQUIRE(cy >= 0); REQUIRE(cy < sys.GetGrid().GetGridSize());
    }
}

TEST_CASE("hard rock heats the bit and the string pecks to cool", "[linehole]")
{
    ProspectingSystem sys = MakeSystem();
    sys.StartAim(3, 3);
    sys.AimAt(3, 3, 3);          // vertical, all the way into basalt
    sys.CommitHole();

    bool dwellSeen = false, advancedWhileDwelling = false;
    float maxHeat = 0.0f;
    for (int i = 0; i < 4000 && sys.lineHole.state == LineHoleState::DRILLING; i++)
    {
        float before = sys.lineHole.depthM;
        sys.UpdateLineHole(0.05f);
        maxHeat = std::max(maxHeat, sys.lineHole.heat);
        if (sys.lineHole.dwelling)
        {
            dwellSeen = true;
            if (sys.lineHole.depthM > before + 0.0001f) advancedWhileDwelling = true;
        }
    }
    REQUIRE(sys.lineHole.state == LineHoleState::DONE);   // a dwell delays, never ends
    REQUIRE(maxHeat >= DRILL_HEAT_MAX - 0.01f);           // basalt cooks the bit
    REQUIRE(dwellSeen);                                   // and forces a peck
    REQUIRE_FALSE(advancedWhileDwelling);                 // which stops the advance
    REQUIRE(sys.lineHole.heat < sys.lineHole.endM);       // sanity: fields distinct
}
