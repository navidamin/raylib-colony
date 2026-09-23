#ifndef SURVEY_FLOW_H
#define SURVEY_FLOW_H

// The descent as the game runs it, one frame at a time.
//
// Owns the SiteSelectionController and does around it what every caller
// would otherwise repeat: run the globe's own drag and zoom before the
// pointer is read, resolve clicks on the game's own controls (a colony's
// marker, the BACK button, the prompt strip, a card row) so the
// controller only sees clicks that mean the ground, keep the elevation
// window the level card's statistics come from, apply the decision after
// the frame is drawn, and have the next rung's ground building before
// the flight lands. The Engine, the game-walk harness and the preview
// tool all drive this, with a live or a scripted SurveyInput.
//
//     flow.BeginFrame(in, w, h, colonies);        // Update
//     flow.Draw(renderManager, planet, colonies, current);
//     SurveyFlow::Frame f = flow.EndFrame(renderManager);   // Commit
//     ... found a colony / switch view from f ...

#include "raylib.h"

#include "colony.h"
#include "game_structs.h"
#include "lola_dem.h"
#include "region_identity.h"
#include "site_selection_controller.h"
#include "survey_input.h"

#include <vector>

class Planet;
class RenderManager;

class SurveyFlow
{
public:
    struct Frame
    {
        bool rungChanged = false;        // sync the view to Controller().Level()
        bool founded = false;            // found a colony from the fields below
        LunarPoint foundPoint;
        LunarPoint windowCentre;         // the site window: the colony's centre
        RegionIdentity region;           // the card the player read
        TerrainBuildability buildability;
        Colony* openedColony = nullptr;  // a marker was clicked: open this one
    };

    void BeginFrame(SurveyInput in, int screenW, int screenH, std::vector<Colony*>& colonies);
    void Draw(RenderManager& rm, Planet* planet, std::vector<Colony*>& colonies,
              const Colony* current);
    Frame EndFrame(RenderManager& rm);

    // Back out one rung from outside the survey's own views (Esc in a
    // colony the descent landed on). Applies at once; true if it moved.
    bool Escape(int screenW, int screenH);
    // ENTER on the globe: claim the region at the sub-point. Applies at
    // the frame's end like a click would.
    void ClaimAtCentre(int screenW, int screenH);
    // Forget the descent and stand on the globe again.
    void Reset();

    SiteSelectionController& Controller() { return ctl; }
    const SiteSelectionController& Controller() const { return ctl; }
    const SurveyLayout& Layout() const { return layout; }

    // The current rung's window (level > 0).
    LunarPoint WindowCentre() const;
    double WindowSpanKm() const;

private:
    void RefreshStats();

    SiteSelectionController ctl;
    SurveyLayout layout;
    int screenW = 0;
    int screenH = 0;
    float groundAspect = 1.0f;       // how much wider than the rung the ground is
    bool updated = false;
    Colony* markerHit = nullptr;

    // The level card's ground statistics come from an elevation window
    // covering the rung; rebuilt when the rung's window changes.
    LolaWindow stats;
    bool statsValid = false;
    int statsLevel = -1;
    LunarKey statsKey;
};

#endif // SURVEY_FLOW_H
