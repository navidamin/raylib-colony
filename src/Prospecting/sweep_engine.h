#pragma once

#include <cstdint>
#include "prospecting_grid.h"
#include "prospecting_constants.h"

struct SweepResult
{
    float energyCost = 0.0f;
    int cellsSwept = 0;
    int anomaliesDetected = 0;
    float avgSignal = 0.0f;
};

class SweepEngine
{
public:
    SweepEngine(int tier = 0);

    bool CanSweep(const ProspectingGrid& grid, int frequencyBand) const;
    float GetSweepCost(int frequencyBand) const;
    SweepResult ExecuteSweep(ProspectingGrid& grid, int frequencyBand, float gameTime);

    float GetCalibrationQuality() const;
    bool IsCalibrating() const;
    void StartCalibration();
    void UpdateCalibration(float deltaTime);

    void SetTier(int tier);
    int GetTier() const;

private:
    int tier;
    float calibrationQuality;
    bool calibrating;
    float calibrationTimer;

    float CalculateRawSignal(const ProspectingGrid& grid, int subX, int subY,
                              int frequencyBand) const;
    float CalculateNoiseFactor(int frequencyBand) const;
    float CalculateConfidenceGain(float signalStrength) const;

    static uint32_t HashNoise(int x, int y, int band, int px, int py);
};
