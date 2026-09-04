#pragma once

#include "prospecting_types.h"
#include "sample_tray.h"
#include "prospecting_grid.h"
#include "resource_manager.h"
#include "prospecting_system.h"

inline Sample MakeDummySample(DepthLayer depth = DepthLayer::SURFACE,
                               float richness = 0.5f)
{
    Sample s;
    s.subCellX = 0;
    s.subCellY = 0;
    s.depthLayer = depth;
    s.richness = richness;
    s.trueComposition[ResourceType::Fe] = 0.40f;
    s.trueComposition[ResourceType::Si] = 0.25f;
    s.trueComposition[ResourceType::Ti] = 0.10f;
    s.state = SampleState::IN_TRAY;
    return s;
}

inline Sample MakeFullCompositionSample(DepthLayer depth = DepthLayer::SURFACE,
                                         float richness = 0.5f)
{
    Sample s;
    s.subCellX = 0;
    s.subCellY = 0;
    s.depthLayer = depth;
    s.richness = richness;
    s.trueComposition[ResourceType::Fe] = 0.40f;
    s.trueComposition[ResourceType::Si] = 0.25f;
    s.trueComposition[ResourceType::Ti] = 0.10f;
    s.trueComposition[ResourceType::H2] = 0.08f;
    s.trueComposition[ResourceType::O2] = 0.06f;
    s.state = SampleState::IN_TRAY;
    return s;
}

inline Sample MakeSampleWithConfidence(float feConf, float siConf)
{
    Sample s = MakeDummySample();
    s.elementConfidence[ResourceType::Fe] = feConf;
    s.elementConfidence[ResourceType::Si] = siConf;
    return s;
}

inline ResourceManager MakeTestResourceManager()
{
    ResourceManager rm(20, 100.0f);
    rm.GenerateResourceMap(42);
    rm.GenerateOrbitalSurveyData();
    return rm;
}

// The lattice is a fixed 8x8; a tier only widens a centred reach window, so
// sweeps and samples outside that window are rejected. These helpers give
// tests coordinates that are valid for the tier under test.
inline std::pair<int, int> InReachCoord(int tier, int index = 0)
{
    int reach = GetReachForTier(tier);
    int offset = (PROSPECTING_GRID_SIZE - reach) / 2;
    return { offset + (index % reach), offset + ((index / reach) % reach) };
}

inline int InReachCellCount(int tier)
{
    int reach = GetReachForTier(tier);
    return reach * reach;
}
