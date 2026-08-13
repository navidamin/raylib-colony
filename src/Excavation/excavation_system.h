#pragma once

#include "site_view.h"
#include "estimate_engine.h"
#include "resource_types.h"

class ProspectingSystem;

// Excavation module facade.
//
// Owns the engines and the panel's UI state. The renderer is immediate-mode
// and keeps nothing between frames, so anything that must persist across
// frames lives here -- which is also what lets the preview and playtest
// harnesses drive the module into any state without a renderer.
//
// Phase 1 is read-only: this reports what excavation can see. It does not
// yet dig. ProcessExtraction() is untouched.
class ExcavationSystem
{
public:
    explicit ExcavationSystem(int tier = 0);

    void SetTier(int tier);
    int  GetTier() const;

    SiteView&       GetSite();
    const SiteView& GetSite() const;

    const EstimateEngine& GetEstimator() const;

    // Convenience for the currently selected spot.
    SpotView DescribeSelected(const ProspectingSystem& prospecting) const;

    // What the player is TOLD is in the selected spot, as opposed to what is
    // actually there. The panel reads this; the dig engine reads the truth.
    SpotEstimate EstimateSelected(const ProspectingSystem& prospecting) const;

    // Move the selection to the best spot this tier can reach, by true value.
    // Used to give a fresh unit a sensible starting spot rather than (0,0),
    // which may not even be in reach.
    void SelectBestReachableSpot(const ProspectingSystem& prospecting);

    // Clamp the selection back inside reach after a tier change.
    void EnsureSelectionInReach();

    // --- UI state ---
    int selectedSpotX = -1;
    int selectedSpotY = -1;
    DepthLayer selectedDepth = DepthLayer::SURFACE;
    ResourceType targetResource = ResourceType::Fe;

private:
    int tier;
    SiteView site;
    EstimateEngine estimator;
};
