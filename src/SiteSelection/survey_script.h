#ifndef SURVEY_SCRIPT_H
#define SURVEY_SCRIPT_H

// Drive the descent headlessly: the same controller the game runs, fed
// scripted pointers with instant flights. What the preview tool, the
// game-walk harness (colony_viewtest) and the tests use to reach a rung
// without a hand on the mouse -- so what they verify is the shipping
// state machine.
//
// Every action is two frames, because a pointer that arrives and clicks
// in the same frame is a touch, and the touch rule turns it into "aim
// only": one frame to settle, one to act.

#include "raylib.h"

#include "site_selection_controller.h"
#include "survey_cursor.h"
#include "terrain_synthesis.h"

class LolaDem;

namespace SurveyScript
{

inline SurveyInput Base(Vector2 pointer)
{
    SurveyInput in;
    in.pointer = pointer;
    in.instant = true;
    return in;
}

// One frame with the pointer resting here and nothing pressed.
inline void Settle(SiteSelectionController& c, int w, int h, const LolaDem* dem, Vector2 p)
{
    c.Update(Base(p), w, h, dem, nullptr);
    c.Commit();
}

// Settle, then click. True if the rung changed or a colony was founded.
inline bool Click(SiteSelectionController& c, int w, int h, const LolaDem* dem, Vector2 p)
{
    Settle(c, w, h, dem, p);
    SurveyInput in = Base(p);
    in.click = true;
    c.Update(in, w, h, dem, nullptr);
    bool changed = c.Commit();
    return changed || c.Founded();
}

// Back out one rung (or, once founded, back to the globe).
inline bool Escape(SiteSelectionController& c, int w, int h, const LolaDem* dem)
{
    SurveyInput in = Base(Vector2{ w * 0.5f, h * 0.5f });
    in.escape = true;
    c.Update(in, w, h, dem, nullptr);
    return c.Commit();
}

// Where on the screen a place is, at the controller's current rung: on
// the globe through the orbital projection (false if turned away), on a
// window rung through the cursor geometry at the rung's own framing.
inline bool PointerFor(const SiteSelectionController& c, int w, int h,
                       double latDeg, double lonDeg, Vector2* out)
{
    if (c.Level() == 0)
    {
        return OrbitalLatLonToScreen(latDeg, lonDeg, w, h, &out->x, &out->y);
    }
    const SurveyCursor* cur = c.Cursor();
    double dx = 0.0, dy = 0.0;
    SurveyLatLonToOffsetKm(*cur, latDeg, lonDeg, &dx, &dy);
    SurveyViewport vp = SurveyLadderViewport(w, h);
    SurveyOffsetKmToScreen(vp, cur->windowSpanKm, dx, dy, &out->x, &out->y);
    return true;
}

// Rest the pointer on a place (the cursor tracks it; the card and the
// verdict describe it).
inline bool Aim(SiteSelectionController& c, int w, int h, const LolaDem* dem,
                double latDeg, double lonDeg)
{
    Vector2 p;
    if (!PointerFor(c, w, h, latDeg, lonDeg, &p)) return false;
    Settle(c, w, h, dem, p);
    return true;
}

// Click on a place: claim it from the globe, descend into it from the
// district, found on it at the site rung.
inline bool ClickAt(SiteSelectionController& c, int w, int h, const LolaDem* dem,
                    double latDeg, double lonDeg)
{
    Vector2 p;
    if (!PointerFor(c, w, h, latDeg, lonDeg, &p)) return false;
    return Click(c, w, h, dem, p);
}

// Turn the globe to face a place, so it can be pointed at.
inline void FaceGlobe(double latDeg, double lonDeg)
{
    OrbitalCamera cam = GetOrbitalCamera();
    cam.subLatDeg = latDeg;
    cam.subLonDeg = lonDeg;
    SetOrbitalCamera(cam);
}

} // namespace SurveyScript

#endif // SURVEY_SCRIPT_H
