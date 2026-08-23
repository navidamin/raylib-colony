#pragma once

#include <vector>
#include <map>
#include <string>
#include "raylib.h"
#include "game_enums.h"
#include "resource_types.h"
#include "prospecting_constants.h"

// Shape families for crystal visual encoding (mapped to depth layers)
enum class ShapeFamily
{
    ANGULAR_CHUNKS,      // Family A: Regolith — rough, fractured surface debris
    CRYSTALLINE_SHARDS,  // Family B: Intact Bedrock — sharp, geometric, prismatic
    ROUNDED_NODULES,     // Family C: Megaregolith — smooth, weathered, compacted
    LAYERED_SLABS        // Family D: Fractured Bedrock — flat, stacked, sedimentary
};

// Analysis tools applied to samples in the lab
enum class AnalysisTool
{
    VISUAL_INSPECTION,       // T0: free, instant, rock-type categories only
    XRF,                     // T1: heavy elements (Fe, Si, Al, Ca, Ti), 0% for H/C/O
    LIBS_PULSE,              // T2: all elements including light (H, C, O)
    FIRE_ASSAY,              // T3: 100% confidence for ONE element, destructive
    OPTICAL_MICROSCOPY,      // T1: rock type, texture identification
    MAGNETIC_SUSCEPTIBILITY  // T1: ferromagnetic mineral identification
};

// Separation methods (pre-analysis processing step)
enum class SeparationMethod
{
    NONE,                // T0: no separation
    MAGNETIC,            // T1: Fe, Ti extraction
    HEAVY_MINERAL,       // T2: rare/dense elements
    VOLATILE_EXTRACTION  // T2: H2, water, gases
};

// Sample lifecycle state
enum class SampleState
{
    IN_TRAY,
    PROCESSING,
    COMPLETED
};

// Confidence display level derived from 0.0-1.0 internal value
enum class ConfidenceLevel
{
    VERY_LOW,   // 0.00 - 0.20
    LOW,        // 0.21 - 0.40
    MODERATE,   // 0.41 - 0.60
    HIGH,       // 0.61 - 0.80
    CERTAIN     // 0.81 - 1.00
};

// Crystal visual encoding for pre-rendered sample sprites
struct CrystalVisual
{
    ShapeFamily shapeFamily = ShapeFamily::ANGULAR_CHUNKS;
    int templateIndex = 0;        // 0-4 within family
    int glowLevel = 0;            // 0-4 (maps to confidence level)
    int sizeLevel = 1;            // 1-4 (maps to richness quartile)
    Color elementColor = GRAY;    // Tint from dominant element
};

// Record of one processing step applied to a sample
struct ProcessingStep
{
    AnalysisTool tool;
    float timestamp;
    std::map<ResourceType, float> confidenceAdded;
};

// Core sample collected from a sub-cell at a specific depth
struct Sample
{
    int id = 0;
    int subCellX = 0;
    int subCellY = 0;
    DepthLayer depthLayer = DepthLayer::SURFACE;
    float richness = 0.0f;

    std::map<ResourceType, float> trueComposition;
    std::map<ResourceType, float> elementConfidence;

    SampleState state = SampleState::IN_TRAY;
    SeparationMethod separationApplied = SeparationMethod::NONE;
    std::vector<ProcessingStep> analysisHistory;

    CrystalVisual visual;

    bool IsElementRevealed(ResourceType type) const;
    float GetRevealedValue(ResourceType type) const;
    float GetAggregateConfidence() const;
};

// Sub-cell in the prospecting grid
struct SubCell
{
    float sweepSignal = 0.0f;
    bool hasBeenSwept = false;
    int sweepFrequencyBand = -1;
    std::vector<int> sampleIds;
    float aggregateConfidence = 0.0f;
};

// Record of a GPR sweep performed on the grid
struct SweepRecord
{
    int frequencyBand = 0;
    float energyCost = 0.0f;
    float timestamp = 0.0f;
};

// Depth layer metadata (name + geological description)
struct DepthLayerInfo
{
    const char* name;
    const char* geology;
    const char* characteristicResources;
};

ShapeFamily GetPrimaryShapeFamily(DepthLayer layer);
ConfidenceLevel GetConfidenceLevel(float confidence);
int GetGlowLevel(float confidence);
int GetSizeLevel(float richness);
Color GetElementColor(ResourceType element);
const DepthLayerInfo& GetDepthLayerInfo(DepthLayer layer);
bool IsLayerAccessible(int tier, DepthLayer layer);
// Grid dimensions are fixed; tier only changes reach. Kept as a function so
// call sites read the same as before.
int GetGridSizeForTier(int tier);

// Side length of the reachable square at this tier (centred in the grid).
int GetReachForTier(int tier);

// Is this sub-cell within instrument reach at the given tier? Cells outside
// reach exist and hold real data, but cannot be swept or drilled yet.
bool IsSubCellInReach(int subX, int subY, int tier);

// Lowest tier that brings this sub-cell into reach; -1 if always reachable.
int TierRequiredForSubCell(int subX, int subY);
int GetTrayCapacityForTier(int tier);
