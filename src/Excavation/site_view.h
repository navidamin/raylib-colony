#pragma once

#include "excavation_types.h"

class ProspectingGrid;
class SampleTray;

// Read-only view of the ground, as excavation sees it.
//
// Pure logic: it takes prospecting's DATA -- the grid and the sample tray --
// not prospecting's facade, so it can be driven by any harness that can build
// a grid. It never writes, never renders, never takes input. Phase 4 adds the
// one write-back path (dug spots become known), and it will not live here.
//
// The tier held here is the EXCAVATION module's tier. Reach is evaluated
// against it, so excavation can reach further -- or less far -- than
// prospecting can survey.
class SiteView
{
public:
    explicit SiteView(int tier = 0);

    void SetTier(int tier);
    int  GetTier() const;

    // Lattice geometry (fixed 8x8, shared with prospecting)
    int  GetGridSize() const;
    int  GetReach() const;
    bool IsInReach(int x, int y) const;

    // Deepest layer this tier may work
    bool CanWorkDepth(DepthLayer depth) const;

    // Everything excavation knows about one spot at one depth.
    SpotView Describe(const ProspectingGrid& grid, const SampleTray& tray,
                      int x, int y, DepthLayer depth,
                      ResourceType target) const;

    // Per-spot, per-depth confidence, derived from what prospecting stores.
    //
    // Prospecting keeps ONE confidence per sub-cell, so this reconstructs a
    // per-depth view from sweep evidence (attenuated by depth, and only for
    // layers the swept band penetrated) combined with sample evidence (samples
    // taken at this exact spot and depth). If prospecting ever stores
    // confidence per depth directly, this is the only function to change.
    float GetConfidence(const ProspectingGrid& grid, const SampleTray& tray,
                        int x, int y, DepthLayer depth) const;

    // Yield of one resource at a spot: absolute quantity x composition
    // fraction. This is the value worth choosing between spots on -- total
    // quantity is much flatter, because each resource clusters separately.
    float GetTargetYield(const ProspectingGrid& grid,
                         int x, int y, DepthLayer depth,
                         ResourceType target) const;

    // Best reachable spot for a target at a depth, by TRUE value. Used by the
    // inspect dump to show what a perfectly-informed player could reach; the
    // AI in Phase 6 uses the known-value equivalent instead.
    bool FindBestReachableSpot(const ProspectingGrid& grid,
                               DepthLayer depth, ResourceType target,
                               int& outX, int& outY) const;

private:
    int tier;
};
