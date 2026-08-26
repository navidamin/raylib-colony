#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include "sweep_engine.h"
#include <cmath>

TEST_CASE("SweepEngine initializes with full calibration", "[sweep]")
{
    SweepEngine engine(1);
    REQUIRE_THAT(engine.GetCalibrationQuality(), Catch::Matchers::WithinAbs(1.0f, 0.001f));
    REQUIRE_FALSE(engine.IsCalibrating());
    REQUIRE(engine.GetTier() == 1);
}

TEST_CASE("SweepEngine CanSweep respects tier gating", "[sweep]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(1, 5, 5, rm);

    SweepEngine t0(0);
    REQUIRE(t0.CanSweep(grid, 0));
    REQUIRE_FALSE(t0.CanSweep(grid, 1));

    SweepEngine t1(1);
    REQUIRE(t1.CanSweep(grid, 0));
    REQUIRE(t1.CanSweep(grid, 1));
    REQUIRE_FALSE(t1.CanSweep(grid, 2));

    ProspectingGrid grid2(2, 5, 5, rm);
    SweepEngine t2(2);
    REQUIRE(t2.CanSweep(grid2, 0));
    REQUIRE(t2.CanSweep(grid2, 1));
    REQUIRE(t2.CanSweep(grid2, 2));
    REQUIRE_FALSE(t2.CanSweep(grid2, 3));

    ProspectingGrid grid3(3, 5, 5, rm);
    SweepEngine t3(3);
    REQUIRE(t3.CanSweep(grid3, 0));
    REQUIRE(t3.CanSweep(grid3, 3));
}

TEST_CASE("SweepEngine CanSweep prevents frequency repeat", "[sweep]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(3, 5, 5, rm);
    SweepEngine engine(3);

    REQUIRE(engine.CanSweep(grid, 0));
    engine.ExecuteSweep(grid, 0, 100.0f);
    REQUIRE_FALSE(engine.CanSweep(grid, 0));
    REQUIRE(engine.CanSweep(grid, 1));
}

TEST_CASE("SweepEngine CanSweep rejects invalid bands", "[sweep]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(3, 5, 5, rm);
    SweepEngine engine(3);

    REQUIRE_FALSE(engine.CanSweep(grid, -1));
    REQUIRE_FALSE(engine.CanSweep(grid, 4));
    REQUIRE_FALSE(engine.CanSweep(grid, 99));
}

TEST_CASE("SweepEngine GetSweepCost returns correct costs", "[sweep]")
{
    SweepEngine engine(2);
    REQUIRE_THAT(engine.GetSweepCost(0), Catch::Matchers::WithinAbs(30.0f, 0.01f));
    REQUIRE_THAT(engine.GetSweepCost(1), Catch::Matchers::WithinAbs(60.0f, 0.01f));
    REQUIRE_THAT(engine.GetSweepCost(2), Catch::Matchers::WithinAbs(100.0f, 0.01f));
    REQUIRE_THAT(engine.GetSweepCost(3), Catch::Matchers::WithinAbs(150.0f, 0.01f));
    REQUIRE_THAT(engine.GetSweepCost(-1), Catch::Matchers::WithinAbs(0.0f, 0.01f));
}

TEST_CASE("SweepEngine ExecuteSweep marks all cells as swept", "[sweep]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(2, 8, 8, rm);
    SweepEngine engine(2);

    engine.ExecuteSweep(grid, 0, 100.0f);

    // Prospecting's reach is ungated, so a sweep covers the whole lattice.
    // (IsSubCellInReach survives for excavation's own tier only.)
    int size = grid.GetGridSize();
    for (int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++)
        {
            const auto& cell = grid.GetSubCell(x, y);
            REQUIRE(cell.hasBeenSwept);
            REQUIRE(cell.sweepFrequencyBand == 0);
        }
    }
}

TEST_CASE("SweepEngine ExecuteSweep signals are in 0-1 range", "[sweep]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(3, 10, 10, rm);
    SweepEngine engine(3);

    engine.ExecuteSweep(grid, 0, 100.0f);

    int size = grid.GetGridSize();
    for (int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++)
        {
            float signal = grid.GetSubCell(x, y).sweepSignal;
            REQUIRE(signal >= 0.0f);
            REQUIRE(signal <= 1.0f);
        }
    }
}

TEST_CASE("SweepEngine ExecuteSweep adds confidence to sub-cells", "[sweep]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(2, 8, 8, rm);
    SweepEngine engine(2);

    engine.ExecuteSweep(grid, 0, 100.0f);

    int size = grid.GetGridSize();
    bool anyConfidence = false;
    for (int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++)
        {
            float conf = grid.GetSubCell(x, y).aggregateConfidence;
            REQUIRE(conf >= 0.0f);
            REQUIRE(conf <= 1.0f);
            if (conf > 0.0f) anyConfidence = true;
        }
    }
    REQUIRE(anyConfidence);
}

TEST_CASE("SweepEngine ExecuteSweep returns valid result", "[sweep]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(2, 8, 8, rm);
    SweepEngine engine(2);

    auto result = engine.ExecuteSweep(grid, 0, 100.0f);
    int size2 = grid.GetGridSize();

    REQUIRE_THAT(result.energyCost, Catch::Matchers::WithinAbs(30.0f, 0.01f));
    REQUIRE(result.cellsSwept == size2 * size2);
    REQUIRE(result.anomaliesDetected >= 0);
    REQUIRE(result.avgSignal >= 0.0f);
    REQUIRE(result.avgSignal <= 1.0f);
}

TEST_CASE("SweepEngine ExecuteSweep on invalid band returns empty result", "[sweep]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(1, 5, 5, rm);
    SweepEngine engine(1);

    auto result = engine.ExecuteSweep(grid, 2, 100.0f);
    REQUIRE(result.cellsSwept == 0);
    REQUIRE_THAT(result.energyCost, Catch::Matchers::WithinAbs(0.0f, 0.01f));
}

TEST_CASE("SweepEngine ExecuteSweep records in grid history", "[sweep]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(3, 5, 5, rm);
    SweepEngine engine(3);

    REQUIRE(grid.GetSweepHistory().empty());
    engine.ExecuteSweep(grid, 0, 100.0f);
    REQUIRE(grid.GetSweepHistory().size() == 1);
    REQUIRE(grid.HasSweptFrequency(0));
}

TEST_CASE("SweepEngine calibration degrades after sweep", "[sweep]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(3, 5, 5, rm);
    SweepEngine engine(3);

    float before = engine.GetCalibrationQuality();
    engine.ExecuteSweep(grid, 0, 100.0f);
    float after = engine.GetCalibrationQuality();

    REQUIRE(after < before);
    REQUIRE_THAT(before - after, Catch::Matchers::WithinAbs(CALIBRATION_DRIFT_PER_SCAN, 0.001f));
}

TEST_CASE("SweepEngine calibration has minimum floor", "[sweep]")
{
    auto rm = MakeTestResourceManager();
    SweepEngine engine(3);

    for (int i = 0; i < 100; i++)
    {
        ProspectingGrid grid(3, 5, 5 + i, rm);
        engine.ExecuteSweep(grid, 0, static_cast<float>(i));
    }

    REQUIRE(engine.GetCalibrationQuality() >= CALIBRATION_MIN_QUALITY);
}

TEST_CASE("SweepEngine StartCalibration and UpdateCalibration restore quality", "[sweep]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(3, 5, 5, rm);
    SweepEngine engine(3);

    engine.ExecuteSweep(grid, 0, 100.0f);
    REQUIRE(engine.GetCalibrationQuality() < 1.0f);

    engine.StartCalibration();
    REQUIRE(engine.IsCalibrating());

    engine.UpdateCalibration(CALIBRATION_DURATION + 1.0f);
    REQUIRE_FALSE(engine.IsCalibrating());
    REQUIRE_THAT(engine.GetCalibrationQuality(), Catch::Matchers::WithinAbs(1.0f, 0.001f));
}

TEST_CASE("SweepEngine calibration progresses incrementally", "[sweep]")
{
    SweepEngine engine(1);
    engine.StartCalibration();

    engine.UpdateCalibration(CALIBRATION_DURATION * 0.5f);
    REQUIRE(engine.IsCalibrating());

    engine.UpdateCalibration(CALIBRATION_DURATION * 0.5f + 1.0f);
    REQUIRE_FALSE(engine.IsCalibrating());
}

TEST_CASE("SweepEngine SetTier clamps to valid range", "[sweep]")
{
    SweepEngine engine(0);
    engine.SetTier(5);
    REQUIRE(engine.GetTier() == 3);
    engine.SetTier(-1);
    REQUIRE(engine.GetTier() == 0);
}

TEST_CASE("SweepEngine multiple sweeps at different bands accumulate confidence", "[sweep]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(3, 8, 8, rm);
    SweepEngine engine(3);

    engine.ExecuteSweep(grid, 0, 100.0f);
    float confAfterFirst = grid.GetSubCell(0, 0).aggregateConfidence;

    engine.ExecuteSweep(grid, 1, 200.0f);
    float confAfterSecond = grid.GetSubCell(0, 0).aggregateConfidence;

    REQUIRE(confAfterSecond >= confAfterFirst);
}

TEST_CASE("SweepEngine max signal is kept across sweeps", "[sweep]")
{
    auto rm = MakeTestResourceManager();
    ProspectingGrid grid(3, 8, 8, rm);
    SweepEngine engine(3);

    engine.ExecuteSweep(grid, 0, 100.0f);

    int size = grid.GetGridSize();
    std::vector<std::vector<float>> signalsAfterFirst(size, std::vector<float>(size));
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++)
            signalsAfterFirst[y][x] = grid.GetSubCell(x, y).sweepSignal;

    engine.ExecuteSweep(grid, 2, 200.0f);

    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++)
            REQUIRE(grid.GetSubCell(x, y).sweepSignal >= signalsAfterFirst[y][x]);
}
