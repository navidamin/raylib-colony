#ifndef SURVEY_INPUT_H
#define SURVEY_INPUT_H

// The seam between whoever owns the mouse and the site-selection
// controller. The game feeds it from InputManager, lunar_map from raylib
// directly, and the headless harnesses from a script -- which is what
// makes the harness verify the shipping state machine rather than a copy
// of it. Nothing in here reads raylib input itself.

#include "raylib.h"

#include "site_selection_constants.h"

#include <cmath>

struct SurveyInput
{
    Vector2 pointer = { 0.0f, 0.0f };
    // A completed click: a release whose press did not travel, or the
    // harness's scripted click. Never a raw press.
    bool click = false;
    // Back out one rung: Esc, right-click below the globe, the BACK button.
    bool escape = false;
    float wheel = 0.0f;              // notches this frame
    float dt = 1.0f / 60.0f;         // seconds since the last frame
    // The caller can show ground wider than the rung's own window, so
    // zooming out is allowed as far as SurveyZoomMin.
    bool zoomOutAllowed = false;
    // How much wider than the rung's span the caller's ground is (a
    // landscape screen builds a wider window so the width is covered).
    float groundAspect = 1.0f;
    // The region-card row under the pointer, or nullptr. A click on a
    // card row opens its hint and is not a move.
    const char* hintKey = nullptr;
    // The click landed on the caller's own controls (a toggle, the prompt
    // strip): consumed there, not a navigation.
    bool uiConsumedClick = false;
    // No flights: a transition lands the same frame it is asked for.
    // The headless step harness runs this way, and a build with no clock
    // could too.
    bool instant = false;
    // With instant, print where each flight WOULD have ended (GLOBECHK /
    // ZOOMCHK lines on stderr) so the geometry stays checkable headlessly.
    bool reportGeometry = false;
};

// A press that travels is a drag, not a click. Fed the raw button state
// each frame by whoever owns the mouse; answers whether the release that
// just happened was a click.
struct SurveyPressGesture
{
    bool down = false;
    bool moved = false;          // travelled past the threshold this press
    Vector2 from = { 0.0f, 0.0f };

    void Update(Vector2 pointer, bool pressed, bool held, bool released)
    {
        if (pressed)
        {
            down = true;
            moved = false;   // cleared on press, so it survives the release
            from = pointer;
        }
        if (down)
        {
            float dx = pointer.x - from.x, dy = pointer.y - from.y;
            if (std::sqrt(dx * dx + dy * dy) > SURVEY_DRAG_THRESHOLD_PX) moved = true;
        }
        if (released) down = false;
        (void)held;
    }

    bool Dragged() const { return moved; }

    // Commit on RELEASE, and only if the press stayed put.
    bool Click(bool released) const { return released && !moved; }
};

#endif // SURVEY_INPUT_H
