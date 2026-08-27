#include "prospecting_system.h"

#include <algorithm>
#include <cmath>

ProspectingSystem::ProspectingSystem(int tier, int parentGridX, int parentGridY,
                                     ResourceManager& resourceManager)
    : tier(tier)
    , resourceManager(resourceManager)
    , grid(tier, parentGridX, parentGridY, resourceManager)
    , tray(tier)
    , sweep(tier)
    , sampler(tier)
    , lab(tier)
{
    // Start on a cell the instruments can actually reach, so the cell readout
    // is meaningful before the player clicks anything.
    int centre = GetGridSizeForTier(tier) / 2;
    selectedCellX = centre;
    selectedCellY = centre;
}

float ProspectingSystem::GetSurveyProgress() const
{
    EnsureCache();
    return cachedResult.surveyProgress;
}

bool ProspectingSystem::IsMarkedSite() const
{
    return SurveyProgressEngine::QualifiesAsMarkedSite(GetSurveyProgress());
}

void ProspectingSystem::SetTier(int newTier)
{
    tier = newTier;
    grid.ResizeForTier(newTier);
    tray.SetTier(newTier);
    sweep.SetTier(newTier);
    sampler.SetTier(newTier);
    lab.SetTier(newTier);
    InvalidateCache();
}

int ProspectingSystem::GetTier() const
{
    return tier;
}

ProspectingGrid& ProspectingSystem::GetGrid()
{
    InvalidateCache();
    return grid;
}

const ProspectingGrid& ProspectingSystem::GetGrid() const
{
    return grid;
}

SampleTray& ProspectingSystem::GetTray()
{
    InvalidateCache();
    return tray;
}

const SampleTray& ProspectingSystem::GetTray() const
{
    return tray;
}

SweepEngine& ProspectingSystem::GetSweep()
{
    InvalidateCache();
    return sweep;
}

const SweepEngine& ProspectingSystem::GetSweep() const
{
    return sweep;
}

SamplingEngine& ProspectingSystem::GetSampler()
{
    InvalidateCache();
    return sampler;
}

const SamplingEngine& ProspectingSystem::GetSampler() const
{
    return sampler;
}

LabEngine& ProspectingSystem::GetLab()
{
    InvalidateCache();
    return lab;
}

const LabEngine& ProspectingSystem::GetLab() const
{
    return lab;
}

void ProspectingSystem::InvalidateCache()
{
    cacheValid = false;
}

void ProspectingSystem::EnsureCache() const
{
    if (!cacheValid)
    {
        cachedResult = SurveyProgressEngine::Calculate(grid, tray);
        cacheValid = true;
    }
}

// ---------------------------------------------------------------------------
// The prescribed line
// ---------------------------------------------------------------------------

void ProspectingSystem::StartAim(int collarX, int collarY)
{
    if (lineHole.state == LineHoleState::DRILLING) return;  // string is down
    lineHole = LineHole{};
    lineHole.state = LineHoleState::AIMING;
    lineHole.collarX = collarX;
    lineHole.collarY = collarY;
    lineHole.targetLayer = 0;
    lineHole.endM = LayerBottomM(0);
}

void ProspectingSystem::AimAt(int layer, int cellX, int cellY)
{
    if (lineHole.state != LineHoleState::AIMING) return;
    layer = std::clamp(layer, 0, 3);
    lineHole.targetLayer = layer;
    lineHole.endM = LayerBottomM(layer);
    if (layer == 0)
    {
        // A surface target is the vertical degenerate case
        lineHole.dirX = 0.0f;
        lineHole.dirY = 0.0f;
        return;
    }
    // Aim so the line passes through the released cell AT that layer's centre
    float c = LAYER_CENTRE_M[layer];
    lineHole.dirX = (static_cast<float>(cellX) - lineHole.collarX) / c;
    lineHole.dirY = (static_cast<float>(cellY) - lineHole.collarY) / c;
}

void ProspectingSystem::CancelAim()
{
    if (lineHole.state == LineHoleState::AIMING)
        lineHole.state = LineHoleState::NONE;
}

bool ProspectingSystem::CommitHole()
{
    if (lineHole.state != LineHoleState::AIMING) return false;
    lineHole.state = LineHoleState::DRILLING;
    lineHole.depthM = 0.0f;
    return true;
}

void ProspectingSystem::GetLineCell(float m, float& gx, float& gy) const
{
    gx = lineHole.collarX + lineHole.dirX * m;
    gy = lineHole.collarY + lineHole.dirY * m;
}

void ProspectingSystem::GetCrossingCell(int layer, int& gx, int& gy) const
{
    float fx = 0.0f, fy = 0.0f;
    GetLineCell(LAYER_CENTRE_M[std::clamp(layer, 0, 3)], fx, fy);
    int size = grid.GetGridSize();
    gx = std::clamp(static_cast<int>(std::lround(fx)), 0, size - 1);
    gy = std::clamp(static_cast<int>(std::lround(fy)), 0, size - 1);
}

bool ProspectingSystem::UpdateLineHole(float dt)
{
    if (lineHole.state != LineHoleState::DRILLING)
    {
        lineHole.heat = std::max(0.0f, lineHole.heat - DRILL_HEAT_COOL * dt);
        return false;
    }

    // Heat: climbs with the hardness being cut, bleeds at a flat rate. Past
    // the ceiling the string auto-pecks -- dwells off the face, no advance --
    // until it has cooled enough to bite again. Hard ground costs time.
    float hard = LAYER_HARDNESS[LayerOfDepthM(lineHole.depthM)];
    if (lineHole.dwelling)
    {
        lineHole.heat -= DRILL_HEAT_COOL * dt;
        if (lineHole.heat <= DRILL_HEAT_RESUME) lineHole.dwelling = false;
        return false;
    }
    lineHole.heat += ((0.35f + hard * 0.75f) * DRILL_HEAT_GAIN - DRILL_HEAT_BLEED) * dt;
    lineHole.heat = std::clamp(lineHole.heat, 0.0f, DRILL_HEAT_MAX);
    if (lineHole.heat >= DRILL_HEAT_MAX)
    {
        lineHole.dwelling = true;
        return false;
    }

    lineHole.depthM = std::min(lineHole.endM,
        lineHole.depthM + DRILL_ADVANCE_MPS[LayerOfDepthM(lineHole.depthM)] * dt);

    // Core each crossing as the bit passes its layer centre -- knowledge
    // lands DURING the hole, which is what the block model animates.
    for (int L = 0; L <= lineHole.targetLayer; L++)
    {
        if (lineHole.cored[L] || lineHole.depthM < LAYER_CENTRE_M[L]) continue;
        int cx = 0, cy = 0;
        GetCrossingCell(L, cx, cy);
        grid.RecordCore(cx, cy, static_cast<DepthLayer>(L));
        lineHole.cored[L] = true;
        lineHole.coredTime[L] = gameTime;
        InvalidateCache();
    }

    if (lineHole.depthM >= lineHole.endM)
    {
        lineHole.state = LineHoleState::DONE;
        lineHole.doneTime = gameTime;
        // One specimen per hole -- the deepest interval, the interesting one
        int cx = 0, cy = 0;
        GetCrossingCell(lineHole.targetLayer, cx, cy);
        sampler.AddSpecimen(grid, tray, cx, cy,
                            static_cast<DepthLayer>(lineHole.targetLayer));
        InvalidateCache();
        return true;
    }
    return false;
}
