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
    if (layer == 0)
    {
        // A surface target is the vertical degenerate case
        lineHole.endM = CellRowDepthM(0, cellX, cellY, grid.GetGridSize());
        lineHole.dirX = 0.0f;
        lineHole.dirY = 0.0f;
        return;
    }
    // The clicked cell's iso row is a depth within its stratum -- the line
    // ends exactly there, passing through that cell at that depth.
    float endDepth = CellRowDepthM(layer, cellX, cellY, grid.GetGridSize());
    lineHole.endM = endDepth;
    lineHole.dirX = (static_cast<float>(cellX) - lineHole.collarX) / endDepth;
    lineHole.dirY = (static_cast<float>(cellY) - lineHole.collarY) / endDepth;
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
    lineHole.rpm = DRILL_RPM_IDLE;
    return true;
}

void ProspectingSystem::KickString()
{
    if (lineHole.state != LineHoleState::DRILLING) return;
    if (lineHole.tripping) return;   // the string is out of the hole
    lineHole.rpm = std::min(DRILL_RPM_MAX, lineHole.rpm + DRILL_RPM_KICK);
}

void ProspectingSystem::GetLineCell(float m, float& gx, float& gy) const
{
    gx = lineHole.collarX + lineHole.dirX * m;
    gy = lineHole.collarY + lineHole.dirY * m;
}

void ProspectingSystem::GetCrossingCell(int layer, int& gx, int& gy) const
{
    layer = std::clamp(layer, 0, 3);
    int size = grid.GetGridSize();
    float fx = 0.0f, fy = 0.0f;
    GetLineCell(std::min(LAYER_CENTRE_M[layer], lineHole.endM), fx, fy);
    gx = std::clamp(static_cast<int>(std::lround(fx)), 0, size - 1);
    gy = std::clamp(static_cast<int>(std::lround(fy)), 0, size - 1);
    // one refinement: the cell's own row depth is where the line truly meets
    // this plate, so re-read the line there and re-snap
    float rowM = std::min(CellRowDepthM(layer, gx, gy, size), lineHole.endM);
    GetLineCell(rowM, fx, fy);
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

    // The spindle sags back to its idle crawl; clicks are the only drive.
    lineHole.rpm = DRILL_RPM_IDLE +
        (lineHole.rpm - DRILL_RPM_IDLE) * std::exp(-dt / DRILL_RPM_TAU);

    // A fractured bit trips: the string comes out rod by rod and goes back,
    // nothing advances, and the bit cools fast in the open. Time is the
    // WHOLE price (drilling-procedure.md Rule 1), and it scales with depth.
    if (lineHole.tripping)
    {
        lineHole.tripT += dt;
        lineHole.heat = std::max(0.0f, lineHole.heat - DRILL_HEAT_COOL * 1.6f * dt);
        if (lineHole.tripT >= lineHole.tripDur)
        {
            lineHole.tripping = false;
            lineHole.wear = 0.0f;        // a fresh bit
            lineHole.dwelling = false;
        }
        return false;
    }

    // Thermal fatigue: time at temperature, quadratic above the onset.
    // Cracks grow while the bit is hot whether or not it is advancing, so
    // this accrues through auto-peck dwells too -- hot is hot.
    if (lineHole.heat > BIT_FATIGUE_ONSET)
    {
        float x = (lineHole.heat - BIT_FATIGUE_ONSET) / (1.0f - BIT_FATIGUE_ONSET);
        lineHole.wear += x * x * BIT_FATIGUE_RATE * dt;
    }
    if (lineHole.wear >= 1.0f)
    {
        lineHole.tripping = true;
        lineHole.tripT = 0.0f;
        lineHole.tripDur = BIT_TRIP_BASE_S + lineHole.depthM * BIT_TRIP_S_PER_M;
        lineHole.fracturedTime = gameTime;
        lineHole.trips++;
        // The stick the bit let go in is rubble: LOST, whatever its dose.
        int ivF = std::min(PROS_LOG_INTERVALS - 1,
                           static_cast<int>(lineHole.depthM / PROS_LOG_INTERVAL_M));
        lineHole.logQ[ivF] = 1;
        return false;
    }

    // Heat: climbs with rpm x the hardness being cut, bleeds at a flat rate.
    // Past the ceiling the string auto-pecks -- dwells off the face, no
    // advance -- until it has cooled enough to bite again. Driving hard
    // through hard rock is exactly what cooks it.
    float hard = DrillHardnessAtM(lineHole.depthM);
    if (lineHole.dwelling)
    {
        lineHole.heat -= DRILL_HEAT_COOL * dt;
        if (lineHole.heat <= DRILL_HEAT_RESUME) lineHole.dwelling = false;
        return false;
    }
    lineHole.heat += (lineHole.rpm * (0.35f + hard * 0.75f) * DRILL_HEAT_GAIN
                      - DRILL_HEAT_BLEED) * dt;
    lineHole.heat = std::clamp(lineHole.heat, 0.0f, DRILL_HEAT_MAX);
    if (lineHole.heat >= DRILL_HEAT_MAX)
    {
        lineHole.dwelling = true;
        return false;
    }

    float beforeM = lineHole.depthM;
    lineHole.depthM = std::min(lineHole.endM,
        lineHole.depthM + DrillAdvanceAtM(lineHole.depthM)
                        * lineHole.rpm * dt);

    // Abrasion: metres cut, harder rock cuts the bit back.
    lineHole.wear += (lineHole.depthM - beforeM) * hard * BIT_WEAR_PER_M;

    // The fine core log: the metres just cut are dosed with the heat they
    // were cut at (squared excess above the fatigue onset), stick by stick.
    // Grade follows the dose per metre -- a sustained level, not an instant
    // -- so the auto-peck sawtooth reads as one smoked run, not a flicker.
    if (lineHole.depthM > beforeM)
    {
        float x = std::max(0.0f, (lineHole.heat - BIT_FATIGUE_ONSET)
                                 / (1.0f - BIT_FATIGUE_ONSET));
        int iv0 = static_cast<int>(beforeM / PROS_LOG_INTERVAL_M);
        int iv1 = std::min(PROS_LOG_INTERVALS - 1,
                           static_cast<int>(lineHole.depthM / PROS_LOG_INTERVAL_M));
        for (int iv = iv0; iv <= iv1; iv++)
        {
            float s0 = std::max(beforeM, iv * PROS_LOG_INTERVAL_M);
            float s1 = std::min(lineHole.depthM, (iv + 1) * PROS_LOG_INTERVAL_M);
            float len = s1 - s0;
            if (len <= 0.0f) continue;
            lineHole.logDose[iv] += x * x * len;
            lineHole.logLen[iv]  += len;
            if (lineHole.logQ[iv] == 1) continue;          // lost stays lost
            float dose = lineHole.logDose[iv] / lineHole.logLen[iv];
            lineHole.logQ[iv] = dose >= PROS_LOG_SMOKE_DOSE ? 2 : 3;
        }
    }

    // Core each crossing as the bit passes the crossed cell's own row depth
    // -- knowledge lands DURING the hole, where the line actually is.
    for (int L = 0; L <= lineHole.targetLayer; L++)
    {
        if (lineHole.cored[L]) continue;
        int cx = 0, cy = 0;
        GetCrossingCell(L, cx, cy);
        float rowM = std::min(CellRowDepthM(L, cx, cy, grid.GetGridSize()),
                              lineHole.endM);
        if (lineHole.depthM < rowM) continue;
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
