#include "sweep_engine.h"
#include "game_constants.h"
#include <cmath>
#include <algorithm>
#include <vector>

SweepEngine::SweepEngine(int tier)
    : tier(tier)
    , calibrationQuality(1.0f)
    , calibrating(false)
    , calibrationTimer(0.0f)
{
}

bool SweepEngine::CanSweep(const ProspectingGrid& grid, int frequencyBand) const
{
    if (frequencyBand < 0 || frequencyBand >= SWEEP_FREQUENCY_BANDS)
        return false;

    int maxBand = MAX_SWEEP_BAND_PER_TIER[tier];
    if (frequencyBand > maxBand)
        return false;

    if (grid.HasSweptFrequency(frequencyBand))
        return false;

    return true;
}

float SweepEngine::GetSweepCost(int frequencyBand) const
{
    if (frequencyBand < 0 || frequencyBand >= SWEEP_FREQUENCY_BANDS)
        return 0.0f;
    return SWEEP_ENERGY_COST[frequencyBand];
}

SweepResult SweepEngine::ExecuteSweep(ProspectingGrid& grid, int frequencyBand,
                                       float gameTime)
{
    if (!CanSweep(grid, frequencyBand))
        return {};

    int size = grid.GetGridSize();
    int totalCells = size*size;

    // Step 1: raw signal per sub-cell
    std::vector<std::vector<float>> rawSignals(size, std::vector<float>(size, 0.0f));
    float maxSignal = 0.0f;

    for (int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++)
        {
            rawSignals[y][x] = CalculateRawSignal(grid, x, y, frequencyBand);
            if (rawSignals[y][x] > maxSignal)
                maxSignal = rawSignals[y][x];
        }
    }

    // Step 2: normalize to 0-1
    if (maxSignal > 0.0f)
    {
        for (int y = 0; y < size; y++)
            for (int x = 0; x < size; x++)
                rawSignals[y][x] /= maxSignal;
    }

    // Step 3: spatial blur for low-frequency bands
    float blurWeight = frequencyBand * SWEEP_BLUR_PER_BAND;
    std::vector<std::vector<float>> blurred = rawSignals;

    if (blurWeight > 0.0f)
    {
        for (int y = 0; y < size; y++)
        {
            for (int x = 0; x < size; x++)
            {
                float neighborSum = 0.0f;
                int neighborCount = 0;

                for (int dy = -1; dy <= 1; dy++)
                {
                    for (int dx = -1; dx <= 1; dx++)
                    {
                        if (dx == 0 && dy == 0) continue;
                        int nx = x + dx, ny = y + dy;
                        if (nx >= 0 && nx < size && ny >= 0 && ny < size)
                        {
                            neighborSum += rawSignals[ny][nx];
                            neighborCount++;
                        }
                    }
                }

                float neighborAvg = neighborCount > 0 ? neighborSum / neighborCount : 0.0f;
                blurred[y][x] = rawSignals[y][x] * (1.0f - blurWeight)
                              + neighborAvg * blurWeight;
            }
        }
    }

    // Step 4: anomaly threshold (mean + k * stddev on blurred signals)
    float sum = 0.0f;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++)
            sum += blurred[y][x];
    float mean = sum / totalCells;

    float variance = 0.0f;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++)
        {
            float diff = blurred[y][x] - mean;
            variance += diff*diff;
        }
    variance /= totalCells;
    float stdDev = std::sqrt(variance);
    float anomalyThresh = mean + SWEEP_ANOMALY_THRESHOLD * stdDev;

    // Step 5: apply noise, update sub-cells, detect anomalies
    float noiseFactor = CalculateNoiseFactor(frequencyBand);
    int anomalyCount = 0;

    for (int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++)
        {
            uint32_t seed = HashNoise(x, y, frequencyBand,
                                       grid.GetParentGridX(), grid.GetParentGridY());
            float noise = ((seed % 2001) - 1000) / 1000.0f * noiseFactor;
            float finalSignal = std::clamp(blurred[y][x] + noise, 0.0f, 1.0f);

            SubCell& cell = grid.GetSubCellMut(x, y);
            if (finalSignal > cell.sweepSignal)
                cell.sweepSignal = finalSignal;
            cell.hasBeenSwept = true;
            cell.sweepFrequencyBand = frequencyBand;

            float confGain = CalculateConfidenceGain(finalSignal);
            cell.aggregateConfidence = std::min(1.0f, cell.aggregateConfidence + confGain);

            if (blurred[y][x] > anomalyThresh)
                anomalyCount++;
        }
    }

    // Step 6: record sweep and degrade calibration
    grid.RecordSweep(frequencyBand, SWEEP_ENERGY_COST[frequencyBand], gameTime);

    calibrationQuality = std::max(CALIBRATION_MIN_QUALITY,
                                   calibrationQuality - CALIBRATION_DRIFT_PER_SCAN);

    return { SWEEP_ENERGY_COST[frequencyBand], totalCells, anomalyCount, mean };
}

float SweepEngine::GetCalibrationQuality() const { return calibrationQuality; }
bool SweepEngine::IsCalibrating() const { return calibrating; }

void SweepEngine::StartCalibration()
{
    calibrating = true;
    calibrationTimer = CALIBRATION_DURATION;
}

void SweepEngine::UpdateCalibration(float deltaTime)
{
    if (!calibrating) return;

    calibrationTimer -= deltaTime;
    if (calibrationTimer <= 0.0f)
    {
        calibrationQuality = 1.0f;
        calibrating = false;
        calibrationTimer = 0.0f;
    }
}

void SweepEngine::SetTier(int tier)
{
    this->tier = std::clamp(tier, 0, 3);
}

int SweepEngine::GetTier() const { return tier; }

float SweepEngine::CalculateRawSignal(const ProspectingGrid& grid, int subX, int subY,
                                       int frequencyBand) const
{
    float signal = 0.0f;
    int penetration = SWEEP_DEPTH_PENETRATION[frequencyBand];

    for (int d = 0; d < penetration; d++)
    {
        DepthLayer depth = static_cast<DepthLayer>(d);

        // Absolute quantity, not composition: GPR responds to how much
        // material is present, and composition fractions sum to 1 everywhere
        // (which would flatten the heat map).
        float layerSignal = grid.GetQuantity(subX, subY, depth);

        float attenuation = 1.0f / (1.0f + d * SWEEP_DEPTH_ATTENUATION);
        signal += layerSignal * attenuation;
    }

    return signal;
}

float SweepEngine::CalculateNoiseFactor(int frequencyBand) const
{
    float baseNoise = SWEEP_BASE_NOISE - tier * SWEEP_NOISE_PER_TIER;
    float freqNoise = frequencyBand * SWEEP_NOISE_PER_BAND;
    float calNoise = (1.0f - calibrationQuality) * SWEEP_CALIBRATION_NOISE_WEIGHT;
    return baseNoise + freqNoise + calNoise;
}

float SweepEngine::CalculateConfidenceGain(float signalStrength) const
{
    float gain = CONFIDENCE_GPR_MIN
               + (CONFIDENCE_GPR_MAX - CONFIDENCE_GPR_MIN) * signalStrength;
    return gain * calibrationQuality;
}

uint32_t SweepEngine::HashNoise(int x, int y, int band, int px, int py)
{
    uint32_t h = 2166136261u;
    h ^= static_cast<uint32_t>(x);     h *= 16777619u;
    h ^= static_cast<uint32_t>(y);     h *= 16777619u;
    h ^= static_cast<uint32_t>(band);  h *= 16777619u;
    h ^= static_cast<uint32_t>(px);    h *= 16777619u;
    h ^= static_cast<uint32_t>(py);    h *= 16777619u;
    return h;
}
