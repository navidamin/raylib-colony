#ifndef SITE_SELECTION_CONTROLLER_H
#define SITE_SELECTION_CONTROLLER_H

// The descent's state machine: which rung, what is under the pointer,
// what the verdict is, what a click does, and the flights between rungs.
//
// No GL and no drawing. The game and lunar_map each draw what this says
// in their own way, and the headless harnesses drive it with a scripted
// SurveyInput. Per frame:
//
//     controller.Update(input, screenW, screenH, dem, statsWindow);
//     if (controller.FlightActive() || controller.LandedThisFrame()) draw the flight / rebuild ground
//     else                               draw the rung
//     controller.Commit();               apply the click / escape decided in Update
//
// Update reads; Commit changes rung. Drawing sits between them so a frame
// shows the state the input was judged against, the IMGUI way.
//
// Design: docs/design/site-selection/site-selection-master-design.md

#include "raylib.h"

#include "region_identity.h"
#include "site_verdict.h"
#include "survey_cursor.h"
#include "survey_input.h"

class LolaDem;
struct LolaWindow;

// Which region-card row a pointer is over, given where the card is drawn.
// Layout constants come from site_selection_constants.h.
const char* SurveyRegionCardHintAt(Vector2 pointer, int cardX, int cardY, int cardW);

// The rect a rung's window is drawn into: the centred square of side
// screenH, because the span maps onto the screen HEIGHT.
SurveyViewport SurveyLadderViewport(int screenW, int screenH);

class SiteSelectionController
{
public:
    SiteSelectionController();

    // dem may be nullptr (no verdict, highland identity). statsWindow is
    // an elevation window covering the current rung, for the level card's
    // ground statistics; nullptr leaves them zero.
    void Update(const SurveyInput& input, int screenW, int screenH,
                const LolaDem* dem, const LolaWindow* statsWindow);

    // Apply what Update decided the click or escape does. Returns true if
    // the rung changed (the caller rebuilds its ground).
    bool Commit();

    // Back to the globe, unclaimed. What Esc does after founding.
    void ResetToOrbit();

    // ---- where we are -------------------------------------------------
    int Level() const { return level; }                 // 0 globe .. SITE_LEVELS-1
    bool AtSiteRung() const { return level == SURVEY_LEVEL_COUNT - 1; }
    bool Claimed() const { return claimed; }
    const RegionIdentity& Region() const { return region; }
    bool Founded() const { return founded; }
    // Where the base was founded, valid once Founded().
    double FoundLat() const { return foundLat; }
    double FoundLon() const { return foundLon; }
    const TerrainBuildability& FoundBuildability() const { return foundB; }

    const SurveyDescent& Descent() const { return descent; }
    const SurveyCursor* Cursor() const;
    SurveyCursor* CursorMut();

    // ---- zoom within the rung -----------------------------------------
    float ZoomK() const { return zoomK; }
    double CamXKm() const { return camXKm; }
    double CamYKm() const { return camYKm; }
    const SurveyViewport& Viewport() const { return viewport; }

    // ---- under the pointer, this frame ----------------------------------
    bool OnGround() const { return onGround; }
    double HoverLat() const { return hoverLat; }
    double HoverLon() const { return hoverLon; }
    const RegionIdentity& Hover() const { return hover; }
    // The card is fixed from the claim; before claiming it previews.
    const RegionIdentity& Shown() const { return claimed ? region : hover; }
    bool TouchStyle() const { return touchStyle; }
    bool PointerJumped() const { return pointerJumped; }
    bool PointerMovedThisFrame() const { return pointerMoved; }

    // ---- measured, this frame -------------------------------------------
    const GroundStats& Ground() const { return ground; }
    bool HaveVerdict() const { return haveVerdict; }
    const TerrainBuildability& Buildability() const { return buildability; }
    const PlacementVerdict& Verdict() const { return verdict; }

    // ---- flights ----------------------------------------------------------
    bool FlightActive() const { return flight.active; }
    // 1 = leaving the globe (the orbital camera is driven directly),
    // 2 = diving into the cursor on a window rung.
    int FlightKind() const { return flight.kind; }
    float FlightT() const { return flight.t; }
    // For kind 2: how far past the rung's window the camera has pushed
    // (1 = the whole window), and where its centre is, km east/north.
    float FlightZoomK() const { return flight.zoomK; }
    double FlightCamXKm() const { return flight.camXKm; }
    double FlightCamYKm() const { return flight.camYKm; }
    // The flight ended this frame and the rung changed underneath it.
    bool LandedThisFrame() const { return landed; }
    // Commit changed the rung this frame (including a reset).
    bool RungChangedThisFrame() const { return rungChanged; }

private:
    struct Flight
    {
        bool active = false;
        int kind = 0;
        float t = 0.0f;
        float seconds = 1.0f;
        // kind 1
        RegionIdentity region;
        double toLat = 0.0, toLon = 0.0;
        double fromLat = 0.0, fromLon = 0.0;
        double fromGZoom = 1.0, toGZoom = 1.0;
        // kind 2
        float ratio = 1.0f;                 // fromSpan / toSpan
        double targetXKm = 0.0, targetYKm = 0.0;
        float zoomK = 1.0f;
        double camXKm = 0.0, camYKm = 0.0;
    };

    enum class Pending { NONE, CLAIM, DESCEND, FOUND, ASCEND, RESET };

    void BeginGlobeDescent(double targetLat, double targetLon, double districtKm);
    void BeginDescentZoom(double targetXKm, double targetYKm,
                          double fromSpanKm, double toSpanKm);
    void AdvanceFlight(float dt);
    void LandFlight();
    void ArriveAtRung();

    int level = 0;
    bool claimed = false;
    RegionIdentity region;
    SurveyDescent descent;
    bool founded = false;
    double foundLat = 0.0, foundLon = 0.0;
    TerrainBuildability foundB;

    float zoomK = 1.0f;
    double camXKm = 0.0, camYKm = 0.0;
    SurveyViewport viewport;

    bool havePointer = false;
    Vector2 lastPointer = { 0.0f, 0.0f };
    bool pointerJumped = false;
    bool pointerMoved = false;
    bool touchStyle = false;

    bool onGround = false;
    double hoverLat = 0.0, hoverLon = 0.0;
    RegionIdentity hover;

    GroundStats ground;
    bool haveVerdict = false;
    TerrainBuildability buildability;
    PlacementVerdict verdict;

    Flight flight;
    bool landed = false;
    bool rungChanged = false;

    Pending pending = Pending::NONE;
    bool pendingInstant = false;
    bool pendingReport = false;
};

#endif // SITE_SELECTION_CONTROLLER_H
