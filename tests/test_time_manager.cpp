#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "time_manager.h"

TEST_CASE("TimeManager initialization", "[time_manager]")
{
    TimeManager tm;

    REQUIRE(tm.GetTicks() == 0);
    REQUIRE(tm.GetGameTime() == 0.0f);
    REQUIRE_FALSE(tm.IsPaused());
    REQUIRE(tm.GetTimeScale() == 1.0f);
    REQUIRE(tm.GetCurrentDay() == 0);
}

TEST_CASE("TimeManager tick progression", "[time_manager]")
{
    TimeManager tm;

    SECTION("Ticks advance with Update")
    {
        // TICK_DURATION = 1.0f, so updating by 1.0 should produce 1 tick
        tm.Update(1.0f);
        REQUIRE(tm.GetTicks() == 1);
    }

    SECTION("Multiple ticks per large deltaTime")
    {
        tm.Update(5.0f);
        REQUIRE(tm.GetTicks() == 5);
    }

    SECTION("Day changes after TICKS_PER_DAY ticks")
    {
        // TICKS_PER_DAY = 20
        for (int i = 0; i < 20; i++)
        {
            tm.Update(1.0f);
        }
        REQUIRE(tm.GetCurrentDay() == 1);
    }
}

TEST_CASE("TimeManager time-of-day", "[time_manager]")
{
    TimeManager tm;

    SECTION("Starts at 0.0")
    {
        REQUIRE(tm.GetTimeOfDay() == 0.0f);
    }

    SECTION("Midday at half TICKS_PER_DAY")
    {
        for (int i = 0; i < TICKS_PER_DAY / 2; i++)
        {
            tm.Update(1.0f);
        }
        REQUIRE_THAT(tm.GetTimeOfDay(),
            Catch::Matchers::WithinAbs(0.5f, 0.01f));
    }

    SECTION("Wraps at day boundary")
    {
        for (int i = 0; i < TICKS_PER_DAY; i++)
        {
            tm.Update(1.0f);
        }
        REQUIRE_THAT(tm.GetTimeOfDay(),
            Catch::Matchers::WithinAbs(0.0f, 0.01f));
    }
}

TEST_CASE("TimeManager pause/resume", "[time_manager]")
{
    TimeManager tm;

    tm.Update(1.0f);
    REQUIRE(tm.GetTicks() == 1);

    tm.Pause();
    REQUIRE(tm.IsPaused());

    tm.Update(5.0f);
    REQUIRE(tm.GetTicks() == 1);  // No advancement while paused

    tm.Resume();
    tm.Update(1.0f);
    REQUIRE(tm.GetTicks() == 2);
}

TEST_CASE("TimeManager conversion functions", "[time_manager]")
{
    TimeManager tm;

    REQUIRE_THAT(tm.TicksToSeconds(10),
        Catch::Matchers::WithinAbs(10.0f * TICK_DURATION, 0.001f));
    REQUIRE(tm.SecondsToTicks(20.0f) == 20);
}

TEST_CASE("TimeManager time scale", "[time_manager]")
{
    TimeManager tm;

    tm.SetTimeScale(2.0f);
    REQUIRE(tm.GetTimeScale() == 2.0f);

    // At 2x speed, 1 second of real time = 2 ticks
    tm.Update(1.0f);
    REQUIRE(tm.GetTicks() == 2);

    // Invalid time scale defaults to 1.0
    tm.SetTimeScale(-1.0f);
    REQUIRE(tm.GetTimeScale() == 1.0f);
}
