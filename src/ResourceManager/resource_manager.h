#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

// What is in the ground, anywhere on the Moon.
//
// Ground truth is a function of location, not a cell in an array. Ask for
// a point and the manager answers with the resources under one sect
// footprint there, generated deterministically from the point itself:
// the region's composition (a named feature, its terrane, mare or
// highland -- SiteSelection/region_identity) sets the baseline for each
// element, a smooth variation with a 10-30 km wavelength makes
// neighbouring sects differ and nearby ones correlate, and the depth-bias
// table scales the deposit per layer (iron richer deep down, hydrogen at
// the surface; the layers are views of one deposit, not a partition of
// it). Nothing is stored until it is asked for; the same point always
// answers the same, from any process, for one world seed. Depletion is
// remembered per point.
//
// Design: docs/design/site-selection/site-selection-master-design.md SS4.6
// ("resources belong to the region") and game-integration-plan.md A-D3.

#include <vector>
#include <map>
#include <utility>
#include "raylib.h"

#include "game_constants.h"
#include "game_enums.h"
#include "game_structs.h"
#include "region_identity.h"
#include "lunar_frame.h"

class ResourceManager {
public:
    // What an orbital survey says about a place: the region's composition
    // (one value per region, never refining) and measured terrain.
    struct OrbitalSurveyData {
        // Elemental composition, weight fraction (wt% / 100)
        float fePercent = 0.0f;
        float tiPercent = 0.0f;
        float siPercent = 0.0f;
        float alPercent = 0.0f;
        float caPercent = 0.0f;
        float thPpm = 0.0f;      // Thorium in ppm (0-20)
        float kPpm = 0.0f;       // Potassium in ppm (0-2000)
        // Neutron spectrometer
        float hydrogenSignal = 0.0f;    // 0-1, water proxy
        // Thermal mapper
        float solarIllumination = 0.0f; // 0-1, fraction of lunar day with sun
        // Terrain
        float terrainSlope = 0.0f;      // degrees (0-45)
        // Communications
        float earthVisibility = 0.0f;   // 0-1, line-of-sight fraction
    };

    // The ground under one sect footprint (TERRAIN_CELL_KM across).
    struct Ground {
        LunarPoint point;
        RegionIdentity region;
        std::map<ResourceType, float> resources;   // absolute quantities, all depths
        bool isExploited = false;
    };

    // 0 seeds from random_device: a different Moon every run. Tools and
    // tests pass a fixed seed so their numbers are reproducible.
    explicit ResourceManager(unsigned int worldSeed = 0);

    // Re-seed and forget every ground generated so far.
    void SetWorldSeed(unsigned int worldSeed);
    unsigned int GetWorldSeed() const { return worldSeed; }

    // The ground at a point, generated on first ask and remembered.
    const Ground& GroundAt(const LunarPoint& point) const;

    // Absolute quantities of the deposit, entries above zero only.
    std::vector<std::pair<ResourceType, float>> GetResourcesAt(const LunarPoint& point) const;
    // The deposit as one depth layer sees it: each element scaled by its
    // bias for that depth (shallow = 1.0).
    std::vector<std::pair<ResourceType, float>> GetResourcesAtLayer(const LunarPoint& point,
                                                                    DepthLayer layer) const;
    void Deplete(const LunarPoint& point, ResourceType type, float amount);
    // The founding floor: a first sect never starts on empty ground.
    void EnsureBasicResources(const LunarPoint& point);

    // Orbital survey of a point: composition from the region, terrain rows
    // from real elevation where the model is present.
    OrbitalSurveyData SurveyAt(const LunarPoint& point) const;
    SiteArchetype ArchetypeAt(const LunarPoint& point) const;

    // Surface (0-10 cm), shallow (10-30), mid (30-100), deep (100-300 cm):
    // the multiplier a layer applies to an element, shallow being 1.0.
    static float DepthBias(ResourceType type, DepthLayer layer);

private:
    Ground Generate(const LunarPoint& point) const;

    unsigned int worldSeed;
    mutable std::map<LunarKey, Ground> grounds;
    mutable std::map<LunarKey, OrbitalSurveyData> surveys;
};

#endif // RESOURCE_MANAGER_H
