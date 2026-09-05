#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"
#include "excavation_system.h"
#include "excavation_constants.h"
#include <cmath>

// What the excavation panel reports is a RATE, and a rate is measured, not
// reconstructed. DigResult carries masses -- quantities that scale with the
// tick length -- plus the interval they were dug over, and the facade turns
// the pair into mass per second on the dig tick.
//
// The bug these guard: the readout used to divide the mass by the DRAW
// frame's GetFrameTime(), which is not the interval the dig ran over (the
// caller scales dt by the efficiency multiplier; the preview harness steps a
// fixed half-second). The same panel rendered twice reported 323 and 178
// C/day for identical work.

// A generated map, once, on a fixed seed. Without one the ground truth is
// empty and every dig moves nothing -- which looks exactly like a broken
// rate and is really just bare ground.
static ResourceManager& RateRM()
{
    static ResourceManager rm = []
    {
        ResourceManager m(20, 100.0f);
        m.GenerateResourceMap(4242u);
        return m;
    }();
    return rm;
}

static ProspectingSystem MakeProspecting()
{
    return ProspectingSystem(3, 10, 10, RateRM());
}

// A system parked on a reachable spot, ready to dig, with the automation off:
// these tests are about the rate law, and BASIC (the default) drives the spot,
// the machine and the pace, so a test that set a pace would not be measuring
// the pace it set.
static void Aim(ExcavationSystem& es)
{
    es.aiLevel = AiLevel::OFF;
    es.autoMachine = false;
    es.activeMachine = MachineId::SCOOP;
    es.selectedSpotX = 4;
    es.selectedSpotY = 4;
    es.selectedDepth = DepthLayer::SURFACE;
    es.pace = 1.0f;
    es.powerCap = 0.0f;
}

TEST_CASE("the dig carries the interval it was dug over", "[digrate]")
{
    ProspectingSystem ps = MakeProspecting();
    ExcavationSystem es(3);
    Aim(es);

    DigResult r = es.Dig(ps, 1, 1.0f, 0.25f);
    REQUIRE(r.dtSeconds == 0.25f);

    REQUIRE(r.totalMass > 0.0f);

    // Stamped even when the dig moves nothing, so a zero weighs the same as a
    // productive tick of the same length rather than vanishing.
    es.selectedSpotX = 999;                       // far out of reach
    DigResult none = es.Dig(ps, 1, 1.0f, 0.25f);
    REQUIRE(none.totalMass == 0.0f);
    REQUIRE(none.dtSeconds == 0.25f);
}

TEST_CASE("the reported rate does not depend on the tick length", "[digrate]")
{
    // The whole point. Two identical machines working identical ground, one
    // stepped in short ticks and one in long, must report the same output.
    ProspectingSystem psA = MakeProspecting();
    ExcavationSystem a(3);
    Aim(a);
    a.Dig(psA, 1, 1.0f, 1.0f / 60.0f);

    ProspectingSystem psB = MakeProspecting();
    ExcavationSystem b(3);
    Aim(b);
    b.Dig(psB, 1, 1.0f, 0.5f);

    REQUIRE(a.massPerSecTotal > 0.0f);
    REQUIRE(std::fabs(a.massPerSecTotal - b.massPerSecTotal) < 0.01f);
    REQUIRE(std::fabs(a.massPerSecTarget - b.massPerSecTarget) < 0.01f);

    // The MASSES do differ -- a longer tick moves more ground. That is what
    // makes the division necessary in the first place.
    REQUIRE(b.GetLastResult().totalMass > a.GetLastResult().totalMass * 10.0f);
}

TEST_CASE("the first sample lands whole", "[digrate]")
{
    // A headless preview renders one dig and two frames. Easing in from zero
    // would show a figure that is merely young and be read as one that is
    // wrong.
    ProspectingSystem ps = MakeProspecting();
    ExcavationSystem es(3);
    Aim(es);

    DigResult r = es.Dig(ps, 1, 1.0f, 0.5f);
    REQUIRE(std::fabs(es.massPerSecTotal - r.totalMass / 0.5f) < 0.01f);
}

TEST_CASE("the reported rate eases rather than jumps", "[digrate]")
{
    ProspectingSystem ps = MakeProspecting();
    ExcavationSystem es(3);
    Aim(es);

    const float dt = 1.0f / 60.0f;
    es.Dig(ps, 1, 1.0f, dt);                      // seed at full pace
    float settled = es.massPerSecTotal;
    REQUIRE(settled > 0.0f);

    // Cut the pace to nothing: one tick must NOT take the readout with it.
    es.pace = 0.0f;
    es.Dig(ps, 1, 1.0f, dt);
    REQUIRE(es.GetLastResult().totalMass == 0.0f);   // the dig really stopped
    REQUIRE(es.massPerSecTotal > settled * 0.9f);    // the readout did not

    // ...but it does get there. A second of game time is well past tau.
    for (float t = 0.0f; t < 1.0f; t += dt) es.Dig(ps, 1, 1.0f, dt);
    REQUIRE(es.massPerSecTotal < settled * 0.25f);
}

TEST_CASE("a longer tick eases further in one step", "[digrate]")
{
    // The easing is on the tick's own clock, not per call: stepping half a
    // second must cover as much ground as thirty sixtieths of one, or a
    // paused-and-resumed game would smooth differently from a running one.
    REQUIRE(EXC_RATE_SMOOTH_TAU_S > 0.0f);

    ProspectingSystem psA = MakeProspecting();
    ExcavationSystem a(3);
    Aim(a);
    a.Dig(psA, 1, 1.0f, 1.0f / 60.0f);
    float startA = a.massPerSecTotal;
    a.pace = 0.0f;
    a.Dig(psA, 1, 1.0f, 1.0f / 60.0f);
    float dropShort = startA - a.massPerSecTotal;

    ProspectingSystem psB = MakeProspecting();
    ExcavationSystem b(3);
    Aim(b);
    b.Dig(psB, 1, 1.0f, 1.0f / 60.0f);
    float startB = b.massPerSecTotal;
    b.pace = 0.0f;
    b.Dig(psB, 1, 1.0f, 0.5f);
    float dropLong = startB - b.massPerSecTotal;

    REQUIRE(dropLong > dropShort * 5.0f);
}
