#ifndef SURVEY_CURSOR_H
#define SURVEY_CURSOR_H

// The survey cursor and the descent ladder it walks.
//
// Design: docs/design/site-selection/site-selection-master-design.md
//
// One idea carries the whole interaction: at every zoom level the
// cursor is the FOOTPRINT OF THE LEVEL BELOW. It is always "the thing
// you are about to enter", so descending is picking, not just moving
// the camera. Five levels, from the whole disc down to the 1.5 km patch
// the base actually sits on.
//
// This module is deliberately free of game and render code — it is pure
// geometry (screen <-> km <-> lat/lon, grid snapping, descent stack), so
// the game and the lunar_map instrument can share one implementation
// instead of drifting apart.

#include "raylib.h"

// ---------------------------------------------------------------------------
// The ladder
// ---------------------------------------------------------------------------

// Level 1 is the orbital disc. It is projected (OrbitalPickToLatLon),
// not a top-down km window, so its "span" is nominal: the width of the
// disc that can actually be picked on. Ground within ~15% of the limb is
// too foreshortened to aim at, so the usable width is ~86% of the
// diameter -- and that is what the cursor ratio should be measured
// against. Rounded to 3000 km so the 500 km cursor tiles it exactly:
// every level's span must be a whole number of its own cursors, or the
// snap grid leaves ground the player can see but cannot select.
// The site level hands over at TERRAIN_CELL_KM, so the ladder lands
// exactly on the game's existing 5 km sect grid.
//
// Three levels, not four. LOCALITY (a 25 km window with a 5 km cursor)
// and SITE (a 5 km window with a 1.5 km cursor) used to be separate
// rungs, which made the player click through two framings to reach one
// decision. They are now one level: the window stays at 25 km and the
// CURSOR refines as the view zooms in, from a snapped 5 km cell while
// navigating down to the free 1.5 km build footprint while placing --
// which is what the design already asked for ("the cursor snaps to the
// 5 km cell grid while navigating and is free-moving at the placement
// step"). SurveyFootprintForSpan does the refining; it keeps the cursor
// inside the 15-30% band at every zoom.
const int SURVEY_LEVEL_COUNT = 3;

// The base's own footprint: the smallest the site cursor ever refines
// to, and the size the buildable verdict is measured over.
const double SURVEY_BUILD_FOOTPRINT_KM = 1.5;
// Zooming inside the site level stops once the view is this wide, which
// puts the build footprint at 30% of it -- the top of the band.
const double SURVEY_SITE_VIEW_KM = 5.0;
const double SURVEY_MOON_DIAMETER_KM = 3474.8;
const double SURVEY_ORBITAL_USABLE_KM = 3000.0;

struct SurveyLevelDef
{
    const char* name;
    double windowSpanKm;     // the view's extent
    double footprintKm;      // the cursor = the next level's window
    bool snapToGrid;         // navigation levels snap; the site level does not
};

// Table of the four levels, index 0..SURVEY_LEVEL_COUNT-1.
const SurveyLevelDef* GetSurveyLadder();

// The cursor:window ratio band from the design. Below the floor the
// cursor is a dot and its readout is unreadable; above the ceiling
// there is nothing left to choose between.
const double SURVEY_CURSOR_MIN_RATIO = 0.15;
const double SURVEY_CURSOR_MAX_RATIO = 0.30;

// Footprint for an arbitrary window span, honouring the band. Prefers
// the ladder's own footprints so free zooming still snaps to familiar
// sizes; falls back to a clamped fraction of the span.
double SurveyFootprintForSpan(double spanKm);

// ---------------------------------------------------------------------------
// Cursor state
// ---------------------------------------------------------------------------

// A window on the moon plus the cursor inside it. The cursor offset is
// stored in km relative to the window centre (not in pixels), so it
// survives a resize and is meaningful without a viewport.
struct SurveyCursor
{
    int level = 0;                  // 0-based index into the ladder
    double windowSpanKm = 0.0;
    double windowLatDeg = 0.0;      // window centre on the moon
    double windowLonDeg = 0.0;
    double footprintKm = 0.0;
    double offsetXKm = 0.0;         // cursor centre, east of window centre
    double offsetYKm = 0.0;         // cursor centre, north of window centre
    bool snapToGrid = true;
};

// Build the cursor for a ladder level, centred on the given ground.
SurveyCursor MakeSurveyCursor(int level, double windowLatDeg,
                              double windowLonDeg);

// Real coordinates of the cursor centre.
void SurveyCursorLatLon(const SurveyCursor& cursor,
                        double* latDeg, double* lonDeg);

// ---------------------------------------------------------------------------
// Screen mapping
// ---------------------------------------------------------------------------

// The rect the window span is drawn into: north up, window centre at
// the rect centre, and the span mapped onto the rect's SMALLER
// dimension. In practice callers pass a square -- the centred square of
// side min(screenW, screenH) -- which is what keeps the whole window
// visible at any aspect ratio. Passing a non-square rect still behaves:
// the span fills the short axis and the long axis simply shows more
// ground, but the cursor is clamped to the square the span occupies.
struct SurveyViewport
{
    float x = 0.0f;         // viewport origin in screen pixels
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

// Pixels per km for a window drawn in this viewport.
float SurveyPixelsPerKm(const SurveyViewport& viewport, double spanKm);

// Screen point -> km offset from the window centre (east, north).
void SurveyScreenToOffsetKm(const SurveyViewport& viewport, double spanKm,
                            float screenX, float screenY,
                            double* offsetXKm, double* offsetYKm);

// The inverse.
void SurveyOffsetKmToScreen(const SurveyViewport& viewport, double spanKm,
                            double offsetXKm, double offsetYKm,
                            float* screenX, float* screenY);

// Real coordinates -> km offset in this cursor's window. The inverse of
// SurveyCursorLatLon, for aiming the cursor at a known place (a target
// site, an existing colony) rather than at the mouse.
void SurveyLatLonToOffsetKm(const SurveyCursor& cursor,
                            double latDeg, double lonDeg,
                            double* offsetXKm, double* offsetYKm);

// Where the cursor is drawn.
Rectangle SurveyCursorRect(const SurveyCursor& cursor,
                           const SurveyViewport& viewport);

// Move the cursor to follow the mouse: maps the screen point into the
// window, snaps it to the level's grid if the level snaps, and clamps
// it so the footprint stays wholly inside the window.
void SurveyCursorTrack(SurveyCursor* cursor, const SurveyViewport& viewport,
                       float screenX, float screenY);

// ---------------------------------------------------------------------------
// Descent stack
// ---------------------------------------------------------------------------

// Descent is reversible and free, so the path is a STACK rather than a
// single position: ascending restores the parent view with its cursor
// exactly where the player left it. Backing out of one region to try its
// neighbour must not reset the whole descent.
struct SurveyDescent
{
    SurveyCursor levels[SURVEY_LEVEL_COUNT];
    int depth = 1;          // number of levels on the stack, >= 1
};

// Start at the orbital level, looking at the given ground.
SurveyDescent MakeSurveyDescent(double latDeg, double lonDeg);

SurveyCursor* SurveyCurrent(SurveyDescent* descent);
const SurveyCursor* SurveyCurrent(const SurveyDescent* descent);

// Enter the region under the cursor. The child window is centred on the
// cursor centre, so the ground under the cursor expands to fill the
// frame. Returns false at the deepest level (there is nothing below the
// site level but the build itself).
bool SurveyDescend(SurveyDescent* descent);

// Back out one level. Returns false at the orbital level.
bool SurveyAscend(SurveyDescent* descent);

#endif // SURVEY_CURSOR_H
