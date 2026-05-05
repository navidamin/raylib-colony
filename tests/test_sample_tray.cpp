#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"

TEST_CASE("SampleTray initializes with tier-based capacity", "[tray]")
{
    SampleTray t0(0);
    REQUIRE(t0.GetCapacity() == 4);

    SampleTray t1(1);
    REQUIRE(t1.GetCapacity() == 8);

    SampleTray t2(2);
    REQUIRE(t2.GetCapacity() == 12);

    SampleTray t3(3);
    REQUIRE(t3.GetCapacity() == 16);
}

TEST_CASE("SampleTray starts empty", "[tray]")
{
    SampleTray tray(0);
    REQUIRE(tray.IsEmpty());
    REQUIRE(tray.GetCount() == 0);
    REQUIRE_FALSE(tray.IsFull());
}

TEST_CASE("SampleTray AddSample fills tray and rejects overflow", "[tray]")
{
    SampleTray tray(0); // capacity = 4

    for (int i = 0; i < 4; i++)
    {
        REQUIRE(tray.AddSample(MakeDummySample()));
    }
    REQUIRE(tray.IsFull());
    REQUIRE(tray.GetCount() == 4);
    REQUIRE_FALSE(tray.AddSample(MakeDummySample()));
    REQUIRE(tray.GetCount() == 4);
}

TEST_CASE("SampleTray assigns unique IDs", "[tray]")
{
    SampleTray tray(1);
    tray.AddSample(MakeDummySample());
    tray.AddSample(MakeDummySample());

    int id1 = tray.GetSampleByIndex(0)->id;
    int id2 = tray.GetSampleByIndex(1)->id;
    REQUIRE(id1 != id2);
    REQUIRE(id1 > 0);
    REQUIRE(id2 > 0);
}

TEST_CASE("SampleTray RemoveSample by ID", "[tray]")
{
    SampleTray tray(1);
    tray.AddSample(MakeDummySample());
    tray.AddSample(MakeDummySample(DepthLayer::SHALLOW, 0.8f));
    REQUIRE(tray.GetCount() == 2);

    int id = tray.GetSampleByIndex(0)->id;
    REQUIRE(tray.RemoveSample(id));
    REQUIRE(tray.GetCount() == 1);
    REQUIRE(tray.GetSampleById(id) == nullptr);
}

TEST_CASE("SampleTray RemoveSample with invalid ID returns false", "[tray]")
{
    SampleTray tray(0);
    tray.AddSample(MakeDummySample());
    REQUIRE_FALSE(tray.RemoveSample(99999));
    REQUIRE(tray.GetCount() == 1);
}

TEST_CASE("SampleTray GetSampleByIndex bounds checking", "[tray]")
{
    SampleTray tray(0);
    REQUIRE(tray.GetSampleByIndex(-1) == nullptr);
    REQUIRE(tray.GetSampleByIndex(0) == nullptr);

    tray.AddSample(MakeDummySample());
    REQUIRE(tray.GetSampleByIndex(0) != nullptr);
    REQUIRE(tray.GetSampleByIndex(1) == nullptr);
}

TEST_CASE("SampleTray SetTier changes capacity", "[tray]")
{
    SampleTray tray(0);
    REQUIRE(tray.GetCapacity() == 4);

    tray.SetTier(2);
    REQUIRE(tray.GetCapacity() == 12);
}

TEST_CASE("SampleTray AddBonusSlots increases capacity", "[tray]")
{
    SampleTray tray(0); // base = 4
    tray.AddBonusSlots(1);
    REQUIRE(tray.GetCapacity() == 5);

    // Add samples up to new capacity
    for (int i = 0; i < 5; i++)
        tray.AddSample(MakeDummySample());
    REQUIRE(tray.IsFull());
}

TEST_CASE("SampleTray capacity clamped to TRAY_MAX_CAPACITY", "[tray]")
{
    SampleTray tray(3); // base = 16
    tray.AddBonusSlots(10); // would be 26, but max is 20
    REQUIRE(tray.GetCapacity() == TRAY_MAX_CAPACITY);
}

TEST_CASE("SampleTray FindLowestValueSampleIndex", "[tray]")
{
    SampleTray tray(1);
    tray.AddSample(MakeDummySample(DepthLayer::SURFACE, 0.7f));
    tray.AddSample(MakeDummySample(DepthLayer::SURFACE, 0.2f));
    tray.AddSample(MakeDummySample(DepthLayer::SURFACE, 0.9f));

    REQUIRE(tray.FindLowestValueSampleIndex() == 1);
}

TEST_CASE("SampleTray FindLowestValueSampleIndex on empty tray returns -1", "[tray]")
{
    SampleTray tray(0);
    REQUIRE(tray.FindLowestValueSampleIndex() == -1);
}
