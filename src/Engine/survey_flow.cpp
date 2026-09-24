#include "survey_flow.h"

#include "raymath.h"

#include "lunar_dem_shared.h"
#include "lunar_frame.h"
#include "lunar_globe.h"
#include "rendermanager.h"
#include "site_selection_constants.h"
#include "survey_cursor.h"
#include "terrain_synthesis.h"

#include <algorithm>
#include <cmath>

namespace
{

// The colony whose globe marker is under the pointer, if any.
Colony* MarkerAt(Vector2 screen, std::vector<Colony*>& colonies, int w, int h)
{
    Colony* best = nullptr;
    float bestPx = ORBITAL_MARKER_PICK_PX;
    for (Colony* colony : colonies)
    {
        if (!colony->HasCentre()) continue;
        float x, y;
        if (!OrbitalLatLonToScreen(colony->GetCentre().latDeg, colony->GetCentre().lonDeg,
                                   w, h, &x, &y))
            continue;
        float d = Vector2Distance(screen, Vector2{ x, y });
        if (d <= bestPx)
        {
            best = colony;
            bestPx = d;
        }
    }
    return best;
}

} // namespace

LunarPoint SurveyFlow::WindowCentre() const
{
    const SurveyCursor* c = ctl.Cursor();
    LunarPoint p;
    p.latDeg = c->windowLatDeg;
    p.lonDeg = c->windowLonDeg;
    return p;
}

double SurveyFlow::WindowSpanKm() const
{
    return ctl.Cursor()->windowSpanKm;
}

void SurveyFlow::RefreshStats()
{
    // Only the district rung shows window statistics; the site rung's
    // card is the verdict, measured by the controller itself.
    if (ctl.Level() != 1) { statsValid = false; return; }
    const LolaDem* dem = GetLunarDem();
    if (!dem) { statsValid = false; return; }
    LunarPoint centre = WindowCentre();
    LunarKey key = LunarQuantise(centre);
    if (statsValid && statsLevel == ctl.Level() && key == statsKey) return;
    // As wide as the ground on screen, which a landscape screen widens
    // past the rung's span: the cursor can rest anywhere on it and its
    // statistics must not fall off the edge.
    stats = dem->Window(centre.latDeg, centre.lonDeg, WindowSpanKm() * groundAspect, 160);
    statsValid = true;
    statsLevel = ctl.Level();
    statsKey = key;
}

void SurveyFlow::BeginFrame(SurveyInput in, int w, int h, std::vector<Colony*>& colonies)
{
    screenW = w;
    screenH = h;
    markerHit = nullptr;
    int level = ctl.Level();

    // The globe's drift: off until a right-click at level 1 turns it on,
    // and off again whenever the globe is left, so coming back to it
    // finds it holding still on the place just left. (Right-click also
    // arrives as escape, which at the globe means nothing.)
    if (level != 0 || ctl.FlightActive()) globeSpin = false;
    else if (in.spinToggle && !in.instant) globeSpin = !globeSpin;
    SetLunarGlobeSpin(globeSpin ? LUNAR_GLOBE_SPIN_DEG_PER_SEC : 0.0);

    // The globe turns under a drag and zooms under the wheel, before the
    // pointer is read so the hover lands on this frame's orientation.
    // Not during a flight: the flight owns the camera. A scripted frame
    // has no mouse to read.
    if (level == 0 && !ctl.FlightActive() && !in.instant)
    {
        UpdateLunarGlobeInput(w, h, in.dt);
    }

    layout = ComputeSurveyLayout(w, h, in.pointer, level, ctl.Founded());
    layout.globeSpin = globeSpin;
    in.hintKey = layout.hintKey;
    groundAspect = std::max(1.0f, (float)w / (float)std::max(1, h));
    in.groundAspect = groundAspect;
    in.zoomOutAllowed = true;
    // The game's own controls are resolved here, so the controller only
    // ever sees a click that means the ground.
    if (in.click && layout.pointerOnBack)
    {
        in.click = false;
        in.escape = true;
    }
    in.uiConsumedClick = layout.pointerOnStrip;
    if (level == 0 && in.click && !ctl.FlightActive())
    {
        markerHit = MarkerAt(in.pointer, colonies, w, h);
        if (markerHit) in.click = false;
    }
    // The polar cap: the pictures there are not honest yet (D7), so a
    // claim is refused before the controller sees it. Judged on the
    // pointer's own ground so the strip can say so while hovering.
    if (level == 0 && !ctl.FlightActive())
    {
        double lat = 0.0, lon = 0.0;
        if (OrbitalPickToLatLon(in.pointer.x, in.pointer.y, w, h, &lat, &lon)
            && std::fabs(lat) > SITE_POLAR_FRAME_LAT_DEG)
        {
            layout.polarBlocked = true;
            in.click = false;
        }
    }

    RefreshStats();
    const LolaWindow* sw = (level > 0 && statsValid) ? &stats : nullptr;
    ctl.Update(in, w, h, GetLunarDem(), sw);
    updated = true;
}

void SurveyFlow::Draw(RenderManager& rm, Planet* planet, std::vector<Colony*>& colonies,
                      const Colony* current)
{
    rm.DrawSurveyView(ctl, layout, planet, colonies, current);
}

SurveyFlow::Frame SurveyFlow::EndFrame(RenderManager& rm)
{
    Frame f;
    if (!updated) return f;
    updated = false;

    if (markerHit)
    {
        f.openedColony = markerHit;
        markerHit = nullptr;
        return f;
    }

    // A flight that ended inside Update changed the rung underneath it.
    if (ctl.LandedThisFrame()) f.rungChanged = true;

    // Where a flight begun by this commit will land, captured before the
    // commit moves anything.
    double hoverLat = ctl.HoverLat(), hoverLon = ctl.HoverLon();
    int levelBefore = ctl.Level();
    double cursorLat = 0.0, cursorLon = 0.0;
    SurveyCursorLatLon(*ctl.Cursor(), &cursorLat, &cursorLon);

    if (ctl.Commit()) f.rungChanged = true;

    if (ctl.FlightActive())
    {
        // Have the destination's ground building while the flight hides
        // the cost.
        const SurveyLevelDef* ladder = GetSurveyLadder();
        LunarPoint target;
        int nextLevel = std::min(levelBefore + 1, SURVEY_LEVEL_COUNT - 1);
        if (ctl.FlightKind() == 1) { target.latDeg = hoverLat; target.lonDeg = hoverLon; }
        else                       { target.latDeg = cursorLat; target.lonDeg = cursorLon; }
        rm.RequestGround(target, rm.WindowTextureSpanKm(ladder[nextLevel].windowSpanKm));
    }

    if (ctl.Founded())
    {
        f.founded = true;
        f.foundPoint.latDeg = ctl.FoundLat();
        f.foundPoint.lonDeg = ctl.FoundLon();
        f.windowCentre = WindowCentre();
        f.region = ctl.Region();
        f.buildability = ctl.FoundBuildability();
        // The descent is done: the colony takes over, and the next one
        // starts from the globe.
        ctl.ResetToOrbit();
        statsValid = false;
        f.rungChanged = true;
    }
    return f;
}

bool SurveyFlow::Escape(int w, int h)
{
    SurveyInput in;
    in.pointer = Vector2{ w * 0.5f, h * 0.5f };
    in.escape = true;
    ctl.Update(in, w, h, GetLunarDem(), nullptr);
    bool moved = ctl.Commit();
    if (moved) statsValid = false;
    return moved;
}

void SurveyFlow::ClaimAtCentre(int w, int h)
{
    // The sub-point projects to the screen centre. Two frames: one to
    // settle the pointer there (a jumped pointer only aims), one to
    // click. The click's commit is the frame's, like any other.
    SurveyInput settle;
    settle.pointer = Vector2{ w * 0.5f, h * 0.5f };
    ctl.Update(settle, w, h, GetLunarDem(), nullptr);
    ctl.Commit();
    SurveyInput click = settle;
    click.click = true;
    layout = ComputeSurveyLayout(w, h, click.pointer, ctl.Level(), ctl.Founded());
    ctl.Update(click, w, h, GetLunarDem(), nullptr);
    updated = true;
    screenW = w;
    screenH = h;
}

void SurveyFlow::Reset()
{
    ctl.ResetToOrbit();
    statsValid = false;
    updated = false;
    markerHit = nullptr;
}
