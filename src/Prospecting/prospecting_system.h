#pragma once

#include "prospecting_grid.h"
#include "sample_tray.h"
#include "sweep_engine.h"
#include "sampling_engine.h"
#include "lab_engine.h"
#include "survey_progress_engine.h"

enum class ProspectingTab { SWEEP, SAMPLES, LAB };

// ---------------------------------------------------------------------------
// The prescribed line: ONE hole drawn from a collar cell on the surface
// toward a chosen cell at a chosen layer, then drilled over game time by the
// auger. The visual contract is the drill-dock prototype
// (docs/design/prospecting/prototypes/drill-dock.html, variant b); the
// knowledge contract is RecordCore at every layer the line crosses.
// ---------------------------------------------------------------------------
// NONE -> AIMING -> DRILLING -> RETRACTING -> DONE.
// RETRACTING is the hoist after the bit reaches the bottom: nothing advances,
// the string comes back to the collar, and only when it is out does the hole
// read as DONE. The two end states differ in exactly one way that matters to
// the rest of the game: in DONE the string is OUT OF THE GROUND, so the
// prescribed line stops being drawn over the block model. What the hole
// produced -- the cored cells, the core log, the specimen -- outlives both.
enum class LineHoleState { NONE, AIMING, DRILLING, RETRACTING, DONE };

struct LineHole
{
    LineHoleState state = LineHoleState::NONE;
    int   collarX = 0, collarY = 0;   // cell on layer 0
    float dirX = 0.0f, dirY = 0.0f;   // cells drifted per metre of depth
    int   targetLayer = 0;            // the layer the hole is prescribed into
    float depthM = 0.0f;              // bit depth now
    float endM = 0.0f;                // bottom of the target layer
    bool  cored[4]     = { false, false, false, false };
    float coredTime[4] = { -100.0f, -100.0f, -100.0f, -100.0f };
    float doneTime = -100.0f;
    float heat = 0.0f;                // 0..1, bit temperature
    bool  dwelling = false;           // auto-peck: off the face, cooling
    float rpm = 0.0f;                 // spindle speed, idle..max
    float wear = 0.0f;                // 0..1, bit spent; at 1.0 it fractures
    bool  tripping = false;           // string out rod by rod, and back
    float tripT = 0.0f;               // seconds into the trip
    float tripDur = 0.0f;             // BIT_TRIP_BASE_S + depth * PER_M
    float pullT = 0.0f;               // seconds into the end-of-hole hoist
    float pullDur = 0.0f;             // DrillPullSeconds(depth at the bottom)
    float fracturedTime = -100.0f;    // when the bit last let go
    int   trips = 0;                  // fractures this hole has cost
    // The fine core log: one grade per 5 m stick.
    // 0 = uncut, 1 = LOST (the bit fractured in it), 2 = PARTIAL (smoked),
    // 3 = INTACT. Derived from the thermal dose accumulated per metre cut.
    unsigned char logQ[PROS_LOG_INTERVALS] = {};
    float logDose[PROS_LOG_INTERVALS] = {};   // sum of x^2 * metres, x = heat excess
    float logLen[PROS_LOG_INTERVALS] = {};    // metres cut in the stick
};

class ProspectingSystem
{
public:
    ProspectingSystem(int tier, int parentGridX, int parentGridY,
                      ResourceManager& resourceManager);

    float GetSurveyProgress() const;
    bool IsMarkedSite() const;

    void SetTier(int tier);
    int GetTier() const;

    ProspectingGrid& GetGrid();
    const ProspectingGrid& GetGrid() const;
    SampleTray& GetTray();
    const SampleTray& GetTray() const;
    SweepEngine& GetSweep();
    const SweepEngine& GetSweep() const;
    SamplingEngine& GetSampler();
    const SamplingEngine& GetSampler() const;
    LabEngine& GetLab();
    const LabEngine& GetLab() const;

    // ---- Line hole (the prescribed line) --------------------------------
    // Aim with StartAim/AimAt while the pointer drags across the block
    // model; Commit starts the string turning (the CALLER charges energy);
    // UpdateLineHole advances it and cores each crossing as the bit passes
    // its layer centre. Returns true on the frame the hole completes.
    void StartAim(int collarX, int collarY);
    void AimAt(int layer, int cellX, int cellY);
    void CancelAim();
    bool CommitHole();
    void KickString();                // a click on the borehole: spike the rpm
    bool UpdateLineHole(float dt);
    // The line's fractional cell at depth m, and the cell it cores on a layer
    void GetLineCell(float m, float& gx, float& gy) const;
    void GetCrossingCell(int layer, int& gx, int& gy) const;

    LineHole lineHole;

    // UI state
    ProspectingTab activeTab = ProspectingTab::SWEEP;
    int selectedCellX = -1;
    int selectedCellY = -1;
    int selectedSampleIndex = -1;
    int selectedFrequencyBand = 0;
    DepthLayer selectedDepth = DepthLayer::SURFACE;
    float gameTime = 0.0f;

    // Lab button feedback: which action fired last, for a brief flash
    // (kind: -1 none, 0 analysis tool, 1 separation)
    int lastLabActionKind = -1;
    int lastLabActionIndex = -1;
    float lastLabActionTime = -100.0f;

private:
    int tier;
    ResourceManager& resourceManager;

    ProspectingGrid grid;
    SampleTray tray;
    SweepEngine sweep;
    SamplingEngine sampler;
    LabEngine lab;

    mutable CellSurveyResult cachedResult;
    mutable bool cacheValid = false;

    void InvalidateCache();
    void EnsureCache() const;
};
