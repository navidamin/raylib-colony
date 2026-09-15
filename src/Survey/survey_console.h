#pragma once

#include "survey_block.h"
#include "survey_ground.h"
#include "dash_knowledge.h"
#include "survey_rig.h"
#include "survey_sprites.h"

#include <vector>

/* =====================================================================
   THE SURVEY CONSOLE -- facade
   ---------------------------------------------------------------------
   Owns the ground, what is known about it, and the camera it is seen
   through, and rebuilds the ground when either of the two things that
   move it moves: a hole landing (which re-fits every interface) or a
   scour deepening (which is the surface itself).

   Rebuilding walks five height fields over a 29x29 lattice and is far
   too expensive per frame, so it is stepped on a REVISION rather than a
   clock. A cut in flight steps its scour in about ten stages, which the
   drill's own animation runs smooth on top of.

   Both extraction's first two modules read this: prospecting drills it,
   excavation digs it, and they must never disagree about the ground.
   ===================================================================== */
class SurveyConsole
{
public:
    SurveyConsole();

    void Step(float dt);

    // A finished hole: where it stood and how deep it got. The column is
    // real at the moment the string clears the collar, not when it was aimed
    // and not one metre deeper than it drilled.
    void RecordHole(float i, float j, float depthM);
    // The bowl a cut is scouring, 0..1 as it goes down. Quantised on the way
    // in, so a cut costs about ten ground rebuilds rather than six hundred.
    void SetScour(int index, float i, float j, float progress);
    int  AddScour(float i, float j);
    void Clear();

    // Which bay of the rack is selected. The rack picks it; TOOL STATS shows
    // that tool's controls; nothing else in the console cares.
    /* The resource statement is the module's SCORE -- read between decisions
       rather than during one -- so it folds behind a button and opens as an
       overlay. That is what freed the left column for the rack. */
    bool resourceOverlay = false;

    SurveyTool SelectedTool() const { return selectedTool; }
    void SelectTool(SurveyTool t) { selectedTool = t; }

    SurveyRig&       Rig() { return rig; }
    const SurveyRig& Rig() const { return rig; }

    const SurveyGround&   Ground() const { return ground; }

    /* THE MODEL IS NOT OWNED HERE. The console's own state owns it -- it is
       what the player's drill writes into -- and this borrows it, so the
       block and the drill cannot be looking at different holes. Points at
       `fallback` until UseKnowledge is called, so a SurveyConsole standing
       on its own still works. */
    void UseKnowledge(DashKnowledge* k) { know = k ? k : &fallback; }

    /* Bumped every time the ground is regenerated. The block resamples on
       this rather than every frame -- a rebuild is 1190 samples and the
       ground only moves when a hole lands or a scour deepens. */
    int GroundRevision() const { return groundRevision; }
    const DashKnowledge& Knowledge() const { return *know; }
    SurveyBlockState&      Block() { return block; }
    const SurveyBlockState& Block() const { return block; }

    const std::vector<SurveyScour>& Scours() const { return scours; }

    float Delineation() const { return DashKnow_Delineation(know, ground.Lattice(), ground.ColumnM()); }
    const char* Tier() const { return DashKnow_Tier(know, ground.Lattice(), ground.ColumnM()); }
    bool IsMeasured() const { return DashKnow_IsMeasured(know, ground.Lattice(), ground.ColumnM()); }

    // Drag-to-turn, which needs an origin to measure against and therefore
    // cannot live in the renderer.
    bool  dragging = false;
    bool  dragMoved = false;
    Vector2 dragFrom = {0.0f, 0.0f};
    float dragYaw = 0.0f;
    float dragPitch = 0.0f;

private:
    void Rebuild();

    SurveyGround ground;
    DashKnowledge  fallback = {};
    DashKnowledge* know = &fallback;
    SurveyBlockState block;
    SurveyRig rig;
    SurveyTool selectedTool = SurveyTool::DRILL;
    std::vector<SurveyScour> scours;

    int groundRevision = 0;
    int builtKnowledgeRevision = -1;
    int scourRevision = 0;
    int builtScourRevision = -1;
};
