#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

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
    for (int i = 0; i < 8000 && sys.lineHole.state != LineHoleState::DONE; i++)
    {
        doneEarly = doneEarly ||
                    (sys.UpdateLineHole(0.05f) &&
                     sys.lineHole.depthM < sys.lineHole.endM - 0.5f);
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
    while (sys.lineHole.state != LineHoleState::DONE) sys.UpdateLineHole(0.5f);
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
    sys.StartAim(7, 6);
    sys.AimAt(3, 7, 6);          // vertical, all the way into basalt
    sys.CommitHole();

    // Idle deliberately never outruns the heat bleed -- an untouched hole
    // finishes cold. Heat is a CLICKING consequence, so drive like an
    // engaged player: ~4 clicks/s.
    bool dwellSeen = false, advancedWhileDwelling = false;
    float maxHeat = 0.0f;
    for (int i = 0; i < 8000 && sys.lineHole.state != LineHoleState::DONE; i++)
    {
        if (i % 5 == 0) sys.KickString();
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

TEST_CASE("clicking drives the spindle, which sags back to idle", "[linehole]")
{
    ProspectingSystem sys = MakeSystem();
    sys.StartAim(2, 2);
    sys.AimAt(2, 4, 4);
    sys.CommitHole();
    sys.UpdateLineHole(0.05f);
    REQUIRE(sys.lineHole.rpm <= DRILL_RPM_IDLE + 0.01f);

    for (int i = 0; i < 10; i++) sys.KickString();
    REQUIRE(sys.lineHole.rpm > DRILL_RPM_IDLE + 0.3f);
    REQUIRE(sys.lineHole.rpm <= DRILL_RPM_MAX + 0.001f);

    for (int i = 0; i < 200; i++) sys.UpdateLineHole(0.05f);   // 10 s, no clicks
    REQUIRE(sys.lineHole.rpm < DRILL_RPM_IDLE + 0.05f);        // sagged home
}

TEST_CASE("an idle hole never cooks the bit", "[linehole]")
{
    // The crawl is the floor: with no clicks at all, heat bleed beats gain
    // even in basalt, so AUTO finishes every hole cold -- just slowly.
    ProspectingSystem sys = MakeSystem();
    sys.StartAim(7, 6);
    sys.AimAt(3, 7, 6);
    sys.CommitHole();
    float maxHeat = 0.0f;
    for (int i = 0; i < 8000 && sys.lineHole.state != LineHoleState::DONE; i++)
    {
        sys.UpdateLineHole(0.05f);
        maxHeat = std::max(maxHeat, sys.lineHole.heat);
    }
    REQUIRE(sys.lineHole.state == LineHoleState::DONE);
    REQUIRE(maxHeat < DRILL_HEAT_RESUME);
}

TEST_CASE("too hot for too long fractures the bit - a trip, never an ending", "[linehole]")
{
    // Rule 1 (drilling-procedure.md): broken equipment is the LESSER
    // penalty. Driven flat out, thermal fatigue fractures the bit; the
    // fracture buys a depth-priced trip with no advance, the bit returns
    // fresh, and the hole still finishes.
    ProspectingSystem sys = MakeSystem();
    sys.StartAim(7, 6);
    sys.AimAt(3, 7, 6);
    sys.CommitHole();

    bool advancedWhileTripping = false;
    float wearAfterTrip = -1.0f;
    for (int i = 0; i < 20000 && sys.lineHole.state != LineHoleState::DONE; i++)
    {
        sys.KickString();                        // flat out, every step
        float before = sys.lineHole.depthM;
        bool wasTripping = sys.lineHole.tripping;
        sys.UpdateLineHole(0.05f);
        if (sys.lineHole.tripping && sys.lineHole.depthM > before + 0.0001f)
            advancedWhileTripping = true;
        if (wasTripping && !sys.lineHole.tripping && wearAfterTrip < 0.0f)
            wearAfterTrip = sys.lineHole.wear;
    }
    REQUIRE(sys.lineHole.state == LineHoleState::DONE);   // never an ending
    REQUIRE(sys.lineHole.trips >= 1);                     // it did fracture
    REQUIRE_FALSE(advancedWhileTripping);                 // a trip is downtime
    REQUIRE(wearAfterTrip >= 0.0f);
    REQUIRE(wearAfterTrip < 0.1f);                        // fresh bit after
}

TEST_CASE("a cool hole never fractures the bit", "[linehole]")
{
    // Gentle pace: heat mostly under the fatigue onset, wear driven by
    // abrasion alone -- a full column must not cost a trip.
    ProspectingSystem sys = MakeSystem();
    sys.StartAim(7, 6);
    sys.AimAt(3, 7, 6);
    sys.CommitHole();
    for (int i = 0; i < 8000 && sys.lineHole.state != LineHoleState::DONE; i++)
    {
        if (i % 10 == 0) sys.KickString();       // ~2 clicks/s
        sys.UpdateLineHole(0.05f);
    }
    REQUIRE(sys.lineHole.state == LineHoleState::DONE);
    REQUIRE(sys.lineHole.trips == 0);
    REQUIRE(sys.lineHole.wear < 1.0f);
}

TEST_CASE("the core log grades each stick by the heat it was cut at", "[linehole]")
{
    // Gentle pace: heat never clears the fatigue onset, so every stick the
    // bit passes logs INTACT (3).
    ProspectingSystem cool = MakeSystem();
    cool.StartAim(7, 6);
    cool.AimAt(3, 7, 6);
    cool.CommitHole();
    for (int i = 0; i < 12000 && cool.lineHole.state != LineHoleState::DONE; i++)
    {
        if (i % 20 == 0) cool.KickString();      // ~1 click/s
        cool.UpdateLineHole(0.05f);
    }
    REQUIRE(cool.lineHole.state == LineHoleState::DONE);
    int cut = 0;
    bool allIntact = true;
    for (int iv = 0; iv < PROS_LOG_INTERVALS; iv++)
    {
        if (cool.lineHole.logQ[iv] == 0) continue;
        cut++;
        if (cool.lineHole.logQ[iv] != 3) allIntact = false;
    }
    REQUIRE(cut >= 10);                          // 79 m of 5 m sticks
    REQUIRE(allIntact);

    // Riding the edge (~4 clicks/s) smokes the hot run of the column as
    // ONE sustained band -- dose per metre, not the worst instant, so the
    // auto-peck sawtooth does not flicker stick by stick.
    ProspectingSystem hot = MakeSystem();
    hot.StartAim(7, 6);
    hot.AimAt(3, 7, 6);
    hot.CommitHole();
    for (int i = 0; i < 20000 && hot.lineHole.state != LineHoleState::DONE; i++)
    {
        if (i % 5 == 0) hot.KickString();        // ~4 clicks/s
        hot.UpdateLineHole(0.05f);
    }
    REQUIRE(hot.lineHole.state == LineHoleState::DONE);
    bool sawSmoked = false, flicker = false;
    for (int iv = 1; iv + 1 < PROS_LOG_INTERVALS; iv++)
    {
        if (hot.lineHole.logQ[iv] == 2) sawSmoked = true;
        // a smoked stick between two intact ones, or the reverse, is the
        // flicker the dose grading exists to remove
        // LOST is excluded: it marks the stick the bit fractured in, an
        // event rather than a dose grade, so P-L-P is a record of something
        // that happened and not the grading alternating with itself.
        if (hot.lineHole.logQ[iv] != 0 && hot.lineHole.logQ[iv] != 1 &&
            hot.lineHole.logQ[iv - 1] != 0 && hot.lineHole.logQ[iv - 1] != 1 &&
            hot.lineHole.logQ[iv + 1] != 0 && hot.lineHole.logQ[iv + 1] != 1 &&
            hot.lineHole.logQ[iv - 1] == hot.lineHole.logQ[iv + 1] &&
            hot.lineHole.logQ[iv] != hot.lineHole.logQ[iv - 1])
            flicker = true;
    }
    REQUIRE(sawSmoked);
    REQUIRE_FALSE(flicker);

    // LOST is the stick the bit fractured in -- rubble where the core was.
    ProspectingSystem spam = MakeSystem();
    spam.StartAim(7, 6);
    spam.AimAt(3, 7, 6);
    spam.CommitHole();
    for (int i = 0; i < 20000 && spam.lineHole.state != LineHoleState::DONE; i++)
    {
        spam.KickString();
        spam.UpdateLineHole(0.05f);
    }
    REQUIRE(spam.lineHole.trips >= 1);
    bool sawLost = false;
    for (int iv = 0; iv < PROS_LOG_INTERVALS; iv++)
        if (spam.lineHole.logQ[iv] == 1) sawLost = true;
    REQUIRE(sawLost);
}

TEST_CASE("a plate is one depth: where you click on it does not change z", "[linehole]")
{
    // The block model is exploded so that DEPTH is the axis between plates.
    // Within a plate, the two screen axes are x and y -- so every cell on a
    // plate targets the same z, and moving across it only moves where the
    // hole comes out, never how deep it goes.
    ProspectingSystem near = MakeSystem();
    near.StartAim(4, 4);
    near.AimAt(2, 0, 0);                          // front corner of MID
    float nearEnd = near.lineHole.endM;

    ProspectingSystem far = MakeSystem();
    far.StartAim(4, 4);
    far.AimAt(2, 31, 31);                         // back corner of the same plate
    REQUIRE(far.lineHole.endM == nearEnd);
    REQUIRE(nearEnd == PlateTargetM(2));

    // and the four plates are four distinct depths, in order
    ProspectingSystem sys = MakeSystem();
    float last = -1.0f;
    for (int L = 0; L < 4; L++)
    {
        sys.StartAim(4, 4);
        sys.AimAt(L, 6, 6);
        REQUIRE(sys.lineHole.endM == PlateTargetM(L));
        REQUIRE(sys.lineHole.endM > last);
        last = sys.lineHole.endM;
        sys.CancelAim();
    }

    // the deepest plate still reaches into basalt -- the stratum the whole
    // drill campaign is tuned against
    REQUIRE(LayerOfDepthM(PlateTargetM(3)) == 3);

    // The plate is the TOP FACE of its rock, and the strip hangs each band
    // below its own plate, so the plane depth has to be the stratum's top or
    // the plate drifts off the line that names it (drill-tuning section 4).
    for (int L = 0; L < 4; L++)
    {
        REQUIRE(PlatePlaneM(L) == LayerTopM(L));
        // and the hole aimed at that plate goes INTO the rock under it,
        // staying strictly inside the stratum -- on a boundary the trace
        // would draw its end on the next plate down
        REQUIRE(PlateTargetM(L) > PlatePlaneM(L));
        REQUIRE(PlateTargetM(L) < LayerBottomM(L));
        REQUIRE(LayerOfDepthM(PlateTargetM(L)) == L);
    }
}

TEST_CASE("a finished hole hoists its string out before it reads DONE", "[linehole]")
{
    // Reaching the bottom does not end the line: the string comes back up
    // first, and only when it is out does the hole read DONE. That end state
    // is what the block model watches -- a DONE hole draws no line over the
    // plates, because there is no string in the ground to draw.
    ProspectingSystem sys = MakeSystem();
    sys.StartAim(7, 6);
    sys.AimAt(3, 7, 6);
    sys.CommitHole();

    bool payoutFired = false;
    for (int i = 0; i < 20000 && sys.lineHole.state == LineHoleState::DRILLING; i++)
    {
        if (i % 10 == 0) sys.KickString();
        payoutFired = sys.UpdateLineHole(0.05f) || payoutFired;
    }

    // Bottom reached: hoisting, not finished.
    REQUIRE(sys.lineHole.state == LineHoleState::RETRACTING);
    REQUIRE(payoutFired);                        // and the payout fired AT the bottom
    REQUIRE(std::fabs(sys.lineHole.pullDur
                      - DrillPullSeconds(sys.lineHole.endM)) < 0.001f);

    // The knowledge is already banked -- the hoist is a beat, not a gate.
    for (int L = 0; L <= sys.lineHole.targetLayer; L++)
        REQUIRE(sys.lineHole.cored[L]);
    REQUIRE(sys.GetTray().GetCount() >= 1);

    // A new line cannot be aimed while the string is still coming out.
    sys.StartAim(1, 1);
    REQUIRE(sys.lineHole.state == LineHoleState::RETRACTING);

    // Halfway out and still hoisting; the HOLE stays cut to its full depth
    // (the string leaves, the hole does not).
    float half = sys.lineHole.pullDur * 0.5f;
    for (float t = 0.0f; t < half; t += 0.05f) sys.UpdateLineHole(0.05f);
    REQUIRE(sys.lineHole.state == LineHoleState::RETRACTING);
    REQUIRE(std::fabs(sys.lineHole.depthM - sys.lineHole.endM) < 0.001f);

    // The last rod clears the collar.
    for (int i = 0; i < 2000 && sys.lineHole.state != LineHoleState::DONE; i++)
        sys.UpdateLineHole(0.05f);
    REQUIRE(sys.lineHole.state == LineHoleState::DONE);

    // What the hole produced outlives it: the core log is still there to read
    // after the line over the plates is gone.
    int cut = 0;
    for (int iv = 0; iv < PROS_LOG_INTERVALS; iv++)
        if (sys.lineHole.logQ[iv] != 0) cut++;
    REQUIRE(cut >= 10);

    // And the rig is free again.
    sys.StartAim(1, 1);
    REQUIRE(sys.lineHole.state == LineHoleState::AIMING);
}

TEST_CASE("driving the string harder is never slower", "[linehole]")
{
    // The bargain the whole clicking loop rests on. A fracture is meant to be
    // a GAMBLE -- push the redline, maybe pay a trip -- and a gamble whose
    // cost always exceeds its winnings is a trap, not a choice.
    //
    // This has now broken twice, both times silently, because nothing checked
    // it: once when the fatigue rate was too high, and again when the deepest
    // hole grew from ~79 m to 94 m and the depth-priced trip grew with it.
    // The campaign instrument would have shown it both times; nobody runs an
    // instrument by accident, so it is a test now.
    auto RunToDone = [](float clicksPerSecond)
    {
        ProspectingSystem sys = MakeSystem();
        sys.StartAim(7, 6);
        sys.AimAt(3, 7, 6);                       // the deepest plate: basalt
        sys.CommitHole();
        float t = 0.0f, acc = 0.0f;
        while (sys.lineHole.state != LineHoleState::DONE && t < 1200.0f)
        {
            acc += clicksPerSecond * 0.05f;
            while (acc >= 1.0f) { sys.KickString(); acc -= 1.0f; }
            sys.UpdateLineHole(0.05f);
            t += 0.05f;
        }
        return t;
    };

    float idle = RunToDone(0.0f);
    float easy = RunToDone(2.0f);
    float hard = RunToDone(8.0f);

    REQUIRE(easy < idle);                         // clicking is worth something
    REQUIRE(hard <= easy);                        // and more clicking never costs
    REQUIRE(idle < 1200.0f);                      // even hands-off finishes
}

TEST_CASE("drill tuning campaign", "[.][campaign]")
{
    // Not a check -- an instrument. Run explicitly:
    //   ./build/tests/colony_tests "[campaign]"
    // Prints click-rate vs column time for the full basalt hole; numbers
    // feed docs/design/prospecting/drill-tuning.md.
    for (float f : {0.0f, 1.0f, 2.0f, 4.0f, 6.0f, 8.0f, 12.0f})
    {
        ProspectingSystem sys = MakeSystem();
        sys.StartAim(7, 6);
        sys.AimAt(3, 7, 6);
        sys.CommitHole();
        float t = 0.0f, dwellT = 0.0f, clickAcc = 0.0f, rpmPeak = 0.0f;
        while (sys.lineHole.state == LineHoleState::DRILLING && t < 1200.0f)
        {
            clickAcc += f * 0.05f;
            while (clickAcc >= 1.0f) { sys.KickString(); clickAcc -= 1.0f; }
            sys.UpdateLineHole(0.05f);
            if (sys.lineHole.dwelling) dwellT += 0.05f;
            rpmPeak = std::max(rpmPeak, sys.lineHole.rpm);
            t += 0.05f;
        }
        char log[PROS_LOG_INTERVALS + 1] = {};
        for (int iv = 0; iv < PROS_LOG_INTERVALS; iv++)
            log[iv] = ".LPI"[sys.lineHole.logQ[iv]];
        printf("campaign: %.0f clicks/s -> %.0f m column in %.0f s "
               "(dwell %.0f s, rpm peak %.2f, trips %d, wear at end %.2f) log %s\n",
               f, sys.lineHole.endM, t, dwellT, rpmPeak, sys.lineHole.trips,
               sys.lineHole.wear, log);
        REQUIRE(sys.lineHole.state == LineHoleState::RETRACTING);
        for (int i = 0; i < 2000 && sys.lineHole.state != LineHoleState::DONE; i++)
            sys.UpdateLineHole(0.05f);
        REQUIRE(sys.lineHole.state == LineHoleState::DONE);
    }
}
