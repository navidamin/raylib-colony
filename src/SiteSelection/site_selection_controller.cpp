#include "site_selection_controller.h"

#include "raymath.h"

#include "lola_dem.h"
#include "site_selection_constants.h"
#include "terrain_synthesis.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

SurveyViewport SurveyLadderViewport(int screenW, int screenH)
{
    SurveyViewport v;
    v.x = (screenW - screenH) * 0.5f;
    v.y = 0.0f;
    v.width = (float)screenH;
    v.height = (float)screenH;
    return v;
}

namespace
{

// The same viewport after zooming in by zoomK about a camera centred
// camXKm east / camYKm north of the window centre. Everything that maps
// between screen and ground goes through the viewport, so scaling and
// shifting it is all continuous zoom needs.
SurveyViewport LadderViewportZoomed(int screenW, int screenH, float zoomK,
                                    double camXKm, double camYKm,
                                    double windowSpanKm)
{
    SurveyViewport v;
    v.width = (float)screenH * zoomK;
    v.height = (float)screenH * zoomK;
    if (windowSpanKm <= 0.0) { v.x = 0.0f; v.y = 0.0f; return v; }
    float pxPerKm = v.height / (float)windowSpanKm;
    // SurveyOffsetKmToScreen puts north at smaller y, hence the signs.
    float centreX = screenW * 0.5f - (float)camXKm * pxPerKm;
    float centreY = screenH * 0.5f + (float)camYKm * pxPerKm;
    v.x = centreX - v.width * 0.5f;
    v.y = centreY - v.height * 0.5f;
    return v;
}

// How far the camera has travelled toward the cursor at a given zoom.
// Driving the centre from the zoom rather than a clock keeps the approach
// straight: 0 at zoomK = 1, 1 at zoomK = ratio.
float ZoomApproach(float zoomK, float ratio)
{
    if (ratio <= 1.0f || zoomK <= 1.0f) return 0.0f;
    float e = std::log(zoomK) / std::log(ratio);
    if (e > 1.0f) e = 1.0f;
    return 1.0f - (1.0f - e) / zoomK;
}

float FlightSeconds(double zoomRatio)
{
    float octaves = std::log2((float)std::max(1.001, zoomRatio));
    return std::clamp(octaves / SITE_FLIGHT_OCTAVES_PER_SEC,
                      SITE_FLIGHT_MIN_SECONDS, SITE_FLIGHT_MAX_SECONDS);
}

} // namespace

// ---------------------------------------------------------------------------

const char* SurveyRegionCardHintAt(Vector2 m, int px, int py, int pw)
{
    if (m.x < px || m.x > px + pw) return nullptr;
    int count = 0;
    const SurveyCardRow* rows = GetRegionCardRows(&count);
    for (int i = 0; i < count; i++)
    {
        int y = py + rows[i].yOffset;
        if (m.y >= y && m.y <= y + SURVEY_CARD_ROW_HEIGHT) return rows[i].hintKey;
    }
    return nullptr;
}

SurveyLayout ComputeSurveyLayout(int screenW, int screenH, Vector2 pointer,
                                 int level, bool founded)
{
    SurveyLayout l;
    l.pointer = pointer;
    l.narrow = screenW < SURVEY_NARROW_SCREEN_W;
    int cardX = l.narrow ? 8 : 16;
    l.cardW = l.narrow ? screenW - 16 : SURVEY_CARD_W;
    // The region card wins the globe (it is the decision there); the
    // level card wins the descent (the ground is).
    l.fullRegionCard = (level == 0) || !l.narrow;
    l.regionX = l.narrow ? cardX : 16;
    l.regionY = l.narrow ? 56 : SURVEY_CARD_TOP;
    if (l.narrow && level == 0)
    {
        // Full width means the card covers a third of the moon. Put it in
        // whichever half the pointer is not in, so the ground being read
        // is never the ground hidden.
        l.regionY = (pointer.y < screenH * 0.5f)
            ? (screenH - SURVEY_STRIP_H - SURVEY_REGION_CARD_H - 8) : 56;
    }
    l.hintKey = l.fullRegionCard
        ? SurveyRegionCardHintAt(pointer, l.regionX, l.regionY, l.cardW) : nullptr;
    l.levelX = l.narrow ? cardX : screenW - SURVEY_CARD_W - 16;
    l.levelY = (l.narrow && level > 0) ? l.regionY + 30 + 8 : SURVEY_CARD_TOP;
    l.backBtn = Rectangle{ 8.0f, (float)screenH - 32.0f, 66.0f, 24.0f };
    l.backShown = (level > 0) || founded;
    l.strip = Rectangle{ 0.0f, (float)(screenH - SURVEY_STRIP_H), (float)screenW,
                         (float)SURVEY_STRIP_H };
    l.pointerOnStrip = pointer.y >= screenH - SURVEY_STRIP_H;
    l.pointerOnBack = l.backShown && CheckCollisionPointRec(pointer, l.backBtn);
    return l;
}

// ---------------------------------------------------------------------------

SiteSelectionController::SiteSelectionController()
{
    descent = MakeSurveyDescent(0.0, 0.0);
}

const SurveyCursor* SiteSelectionController::Cursor() const
{
    return SurveyCurrent(&descent);
}

SurveyCursor* SiteSelectionController::CursorMut()
{
    return SurveyCurrent(&descent);
}

void SiteSelectionController::Update(const SurveyInput& in, int screenW, int screenH,
                                     const LolaDem* dem, const LolaWindow* statsWindow)
{
    landed = false;
    rungChanged = false;
    pending = Pending::NONE;
    pendingInstant = in.instant;
    pendingReport = in.reportGeometry;

    viewport = SurveyLadderViewport(screenW, screenH);
    const SurveyLevelDef* ladder = GetSurveyLadder();
    bool siteRung = AtSiteRung();

    // ---------- zoom within the rung ----------
    //
    // Zooming reads the ground; it does not change rung. Both ends are
    // the rung's own geometry, so no amount of scrolling can arrive at a
    // neighbouring level's view: crossing a rung is a click.
    float zoomMax = 1.0f, zoomMin = 1.0f;
    if (level > 0)
    {
        zoomMax = (float)SurveyZoomMax(level);
        if (in.zoomOutAllowed) zoomMin = (float)SurveyZoomMin(level);
    }
    bool zoomable = (level > 0) && !flight.active && !founded;
    float wheel = zoomable ? in.wheel : 0.0f;
    if (wheel != 0.0f && zoomMax > zoomMin)
    {
        zoomK = std::clamp(zoomK * std::pow((float)SURVEY_ZOOM_NOTCH, wheel),
                           zoomMin, zoomMax);
        leanHeld = false;           // a zoom leans onto the cursor again
    }
    // The caller can withdraw zoom-out between frames (its wider ground is
    // rebuilt or dropped); the view then comes back to the window at once.
    if (zoomK < zoomMin) zoomK = zoomMin;

    // ---------- dragging the ground ----------
    //
    // "The player may pan at any level" (master design SS2): nothing locks
    // until the base is founded, and the ground is a function of lat/lon,
    // so a window has no edge to enforce. A press on a window rung that
    // travels slides the view with the pointer -- the ground under it
    // stays under it -- and the release moves the window itself there, so
    // the rung's ground is built about the new place and the region card
    // re-labels if a border was crossed. The globe turns itself.
    bool pannable = (level > 0) && !flight.active && !founded;
    if (pannable && in.held)
    {
        if (!pressOnGround)
        {
            // A press on the caller's controls or a card row is theirs.
            if (!in.uiConsumedClick && !in.hintKey)
            {
                pressOnGround = true;
                pressAt = in.pointer;
            }
        }
        else if (!panning &&
                 Vector2Distance(in.pointer, pressAt) > SURVEY_DRAG_THRESHOLD_PX)
        {
            panning = true;
        }
        if (panning)
        {
            double pxPerKm = (double)screenH * zoomK / ladder[level].windowSpanKm;
            panXKm = -(in.pointer.x - pressAt.x) / pxPerKm;
            panYKm = (in.pointer.y - pressAt.y) / pxPerKm;
            // Not into the polar cap, whose windows are not drawn
            // truthfully yet (D7): the view stops at its edge.
            const SurveyCursor* c = Cursor();
            double dx = 0.0, northKm = 0.0, southKm = 0.0;
            SurveyLatLonToOffsetKm(*c, SITE_POLAR_FRAME_LAT_DEG, c->windowLonDeg, &dx, &northKm);
            SurveyLatLonToOffsetKm(*c, -SITE_POLAR_FRAME_LAT_DEG, c->windowLonDeg, &dx, &southKm);
            panYKm = std::clamp(panYKm, std::min(southKm, 0.0), std::max(northKm, 0.0));
        }
    }
    else
    {
        if (panning && pannable) MoveWindowByPan(dem);
        pressOnGround = false;
        panning = false;
        panXKm = 0.0;
        panYKm = 0.0;
    }

    // Any zoom that is not 1, in either direction, moves the cursor's
    // frame too, or the rectangle stops following the mouse the moment
    // the ground gets wider than the window; so does a drag. This frame's
    // viewport uses last frame's camera: one frame of lag, invisible at
    // 60 Hz.
    if (level > 0 && (std::fabs(zoomK - 1.0f) > 1e-4f || panning))
    {
        viewport = LadderViewportZoomed(screenW, screenH, zoomK,
                                        camXKm + panXKm, camYKm + panYKm,
                                        ladder[level].windowSpanKm);
    }

    // ---------- the pointer ----------
    //
    // A touch screen has no hover: the finger arrives and clicks in the
    // same frame, which would claim whatever it landed on before the
    // player ever saw the card. So a click only counts once the pointer
    // has settled -- a mouse always has, a tap needs a second tap.
    float travel = havePointer ? Vector2Distance(in.pointer, lastPointer) : 1e9f;
    pointerJumped = !havePointer || travel > SURVEY_POINTER_JUMP_PX;
    pointerMoved = !havePointer || travel > SURVEY_POINTER_MOVE_PX;
    lastPointer = in.pointer;
    havePointer = true;

    // ---------- a descent in flight owns the frame ----------
    if (flight.active)
    {
        AdvanceFlight(in.dt);
        if (!flight.active) LandFlight();
        return;
    }

    bool descend = in.click;
    if (descend && pointerJumped) { descend = false; touchStyle = true; }
    bool ascend = in.escape;

    // ---------- state that depends on the pointer ----------
    hover = region;
    onGround = false;
    if (level == 0)
    {
        // The same projection the globe's shader draws with, inverted --
        // so the region named is the region under the pointer.
        if (OrbitalPickToLatLon(in.pointer.x, in.pointer.y, screenW, screenH,
                                &hoverLat, &hoverLon))
        {
            onGround = true;
            hover = IdentifyRegion(dem, hoverLat, hoverLon);
        }
    }
    else
    {
        SurveyCursor* c = CursorMut();
        // What is on screen right now, which zoom changes: the widened
        // window across and the rung's own span down, both opened up by
        // however far the view has zoomed out.
        c->reachAcrossKm = c->windowSpanKm * (double)in.groundAspect / zoomK;
        c->reachDownKm = c->windowSpanKm / zoomK;
        // During a drag the cursor rides the ground like a pin on a map,
        // and the camera holds still under the hand; the pointer takes the
        // cursor back on release.
        if (!panning)
        {
            SurveyCursorTrack(c, viewport, in.pointer.x, in.pointer.y);

            // Lean the camera onto the cursor as the zoom deepens,
            // reaching it at this rung's own limit -- unless a drag has
            // just put the view where the player wants it, in which case
            // it holds until the next zoom.
            if (!leanHeld)
            {
                float approach = ZoomApproach(zoomK, zoomMax);
                camXKm = c->offsetXKm * approach;
                camYKm = c->offsetYKm * approach;
            }
        }
        SurveyCursorLatLon(*c, &hoverLat, &hoverLon);
        onGround = true;
    }

    // ---------- measured ground ----------
    ground = GroundStats();
    haveVerdict = false;
    if (level > 0)
    {
        const SurveyCursor* c = Cursor();
        if (statsWindow)
        {
            ground = CursorGroundStats(*statsWindow, c->offsetXKm, c->offsetYKm,
                                       c->footprintKm);
        }
        if (siteRung && dem)
        {
            buildability = dem->EvaluateSite(hoverLat, hoverLon, c->footprintKm,
                                             SITE_VERDICT_HORIZON_KM);
            verdict = JudgeSite(buildability);
            haveVerdict = true;
        }
    }

    // ---------- what the input asks for; Commit applies it ----------
    if (founded)
    {
        if (ascend) pending = Pending::RESET;
        return;
    }
    if (ascend)
    {
        if (level > 0) pending = Pending::ASCEND;
        return;
    }
    if (descend && onGround && !in.uiConsumedClick)
    {
        if (in.hintKey) return;             // clicking a card row is not a move
        if (level == 0) pending = Pending::CLAIM;
        else if (siteRung)
        {
            // The cursor here IS the base's footprint, so a click is
            // always "build here" and the verdict it commits to is
            // measured over exactly the ground the rectangle covers.
            if (haveVerdict && verdict.allowed) pending = Pending::FOUND;
        }
        else pending = Pending::DESCEND;
    }
}

bool SiteSelectionController::Commit()
{
    Pending p = pending;
    pending = Pending::NONE;
    switch (p)
    {
    case Pending::NONE:
        return false;

    case Pending::RESET:
        ResetToOrbit();
        return true;

    case Pending::ASCEND:
    {
        // Where the rung being left was looking. Undragged, that is the
        // parent's cursor, and the parent comes back exactly as it was
        // left; dragged, the parent comes back centred on it instead of
        // where the descent had been.
        double childLat = Cursor()->windowLatDeg;
        double childLon = Cursor()->windowLonDeg;
        SurveyAscend(&descent);
        level--;
        if (level == 0)
        {
            claimed = false;
            OrbitalCamera cam = GetOrbitalCamera();
            cam.subLatDeg = childLat;
            cam.subLonDeg = childLon;
            SetOrbitalCamera(cam);
            PullGlobeOut();
        }
        else
        {
            SurveyCursor* parent = CursorMut();
            double xKm = 0.0, yKm = 0.0;
            SurveyLatLonToOffsetKm(*parent, childLat, childLon, &xKm, &yKm);
            if (std::fabs(xKm - parent->offsetXKm) > 0.1 ||
                std::fabs(yKm - parent->offsetYKm) > 0.1)
            {
                parent->windowLatDeg = childLat;
                parent->windowLonDeg = childLon;
                parent->offsetXKm = 0.0;
                parent->offsetYKm = 0.0;
            }
        }
        ArriveAtRung();
        return true;
    }

    case Pending::FOUND:
        founded = true;
        foundLat = hoverLat;
        foundLon = hoverLon;
        foundB = buildability;
        return false;

    case Pending::CLAIM:
    {
        flight.region = hover;
        SurveyCursor next = MakeSurveyCursor(1, hoverLat, hoverLon);
        // Off the globe, not across a flat map: turn the sub-point to
        // what was clicked while the orbital zoom climbs, landing at the
        // framing level 2 opens on.
        BeginGlobeDescent(hoverLat, hoverLon, next.windowSpanKm);
        if (!flight.active)                 // instant: act now
        {
            LandFlight();
            return true;
        }
        return false;
    }

    case Pending::DESCEND:
    {
        const SurveyCursor* c = Cursor();
        BeginDescentZoom(c->offsetXKm, c->offsetYKm, c->windowSpanKm, c->footprintKm);
        if (!flight.active)
        {
            LandFlight();
            return true;
        }
        return false;
    }
    }
    return false;
}

void SiteSelectionController::ResetToOrbit()
{
    founded = false;
    claimed = false;
    level = 0;
    descent.depth = 1;
    PullGlobeOut();
    ArriveAtRung();
}

void SiteSelectionController::PullGlobeOut()
{
    // Only ever out: a player already further out keeps their view.
    OrbitalCamera cam = GetOrbitalCamera();
    if (cam.zoom > SITE_GLOBE_RETURN_ZOOM)
    {
        cam.zoom = SITE_GLOBE_RETURN_ZOOM;
        SetOrbitalCamera(cam);
    }
}

void SiteSelectionController::MoveWindowByPan(const LolaDem* dem)
{
    // The window moves to what is on screen: the zoom's lean and the drag
    // both, so the view does not move at the release and the ground is
    // rebuilt centred on it. The lean then holds at zero until the next
    // zoom; recomputed at once from the cursor, it would slide the view
    // away from where the drag left it.
    double moveXKm = camXKm + panXKm;
    double moveYKm = camYKm + panYKm;
    SurveyCursor* c = CursorMut();
    SurveyCursor there = *c;
    there.offsetXKm = moveXKm;
    there.offsetYKm = moveYKm;
    double lat = 0.0, lon = 0.0;
    SurveyCursorLatLon(there, &lat, &lon);
    // The drag stops at the polar cap; the lean it carries must not push
    // the window over.
    lat = std::clamp(lat, -SITE_POLAR_FRAME_LAT_DEG, SITE_POLAR_FRAME_LAT_DEG);
    c->windowLatDeg = lat;
    c->windowLonDeg = lon;
    // The cursor stays on the ground it rode in on; the pointer takes it
    // from there this same frame.
    c->offsetXKm -= moveXKm;
    c->offsetYKm -= moveYKm;
    camXKm = 0.0;
    camYKm = 0.0;
    leanHeld = true;
    // Different ground, different numbers: the card re-labels (master
    // design SS2, "The player may pan at any level").
    region = IdentifyRegion(dem, lat, lon);
}

void SiteSelectionController::ViewCentreLatLon(double* latDeg, double* lonDeg) const
{
    SurveyCursor view = *Cursor();
    view.offsetXKm = (level > 0) ? camXKm + panXKm : 0.0;
    view.offsetYKm = (level > 0) ? camYKm + panYKm : 0.0;
    SurveyCursorLatLon(view, latDeg, lonDeg);
}

void SiteSelectionController::ArriveAtRung()
{
    zoomK = 1.0f;
    camXKm = 0.0;
    camYKm = 0.0;
    pressOnGround = false;
    panning = false;
    panXKm = 0.0;
    panYKm = 0.0;
    leanHeld = false;
    rungChanged = true;
}

// ---------------------------------------------------------------------------
// Flights

void SiteSelectionController::BeginGlobeDescent(double targetLat, double targetLon,
                                                double districtKm)
{
    const OrbitalCamera& cam = GetOrbitalCamera();
    flight.kind = 1;
    flight.fromLat = cam.subLatDeg;
    flight.fromLon = cam.subLonDeg;
    flight.fromGZoom = cam.zoom;
    flight.toGZoom = OrbitalZoomForSpan(districtKm);
    flight.toLat = targetLat;
    flight.toLon = targetLon;

    if (pendingInstant)
    {
        // The step harness does not fly, but it still reports where the
        // flight WOULD land, so the geometry stays checkable headlessly.
        if (pendingReport)
        {
            std::fprintf(stderr, "GLOBECHK sub=(%.2f,%.2f) -> (%.2f,%.2f) "
                                 "zoom %.2f -> %.2f (district %.0f km)\n",
                         flight.fromLat, flight.fromLon, targetLat, targetLon,
                         flight.fromGZoom, flight.toGZoom, districtKm);
        }
        flight.active = false;
        return;
    }
    flight.t = 0.0f;
    flight.seconds = FlightSeconds(flight.toGZoom / flight.fromGZoom);
    flight.active = true;
}

void SiteSelectionController::BeginDescentZoom(double targetXKm, double targetYKm,
                                               double fromSpanKm, double toSpanKm)
{
    flight.kind = 2;
    flight.ratio = (float)(fromSpanKm / toSpanKm);
    flight.targetXKm = targetXKm;
    flight.targetYKm = targetYKm;
    flight.zoomK = 1.0f;
    flight.camXKm = 0.0;
    flight.camYKm = 0.0;

    if (pendingInstant)
    {
        if (pendingReport)
        {
            std::fprintf(stderr, "ZOOMCHK from=%.0fkm to=%.0fkm endVisible=%.2fkm "
                                 "targetKm=(%.2f,%.2f)\n",
                         fromSpanKm, toSpanKm, toSpanKm, targetXKm, targetYKm);
            std::fprintf(stderr, "        zoom=%.1fx flight=%.2fs\n",
                         flight.ratio, FlightSeconds(flight.ratio));
        }
        flight.active = false;
        return;
    }
    flight.t = 0.0f;
    flight.seconds = FlightSeconds(flight.ratio);
    flight.active = true;
}

void SiteSelectionController::AdvanceFlight(float dt)
{
    if (dt <= 0.0f || dt > 0.25f) dt = 1.0f / 60.0f;   // first frame, or a stall
    flight.t += dt / flight.seconds;
    float t = std::clamp(flight.t, 0.0f, 1.0f);
    float e = t * t * (3.0f - 2.0f * t);               // smoothstep

    if (flight.kind == 1)
    {
        // Leaving the globe: turn and zoom rather than pan and zoom, and
        // drive the rotation from the zoom so the target falls straight to
        // the centre instead of swinging out and coming back.
        double zoom = flight.fromGZoom *
                      std::pow(flight.toGZoom / flight.fromGZoom, (double)e);
        double k = 1.0 - (1.0 - e) * (flight.fromGZoom / zoom);
        double dLon = flight.toLon - flight.fromLon;
        while (dLon > 180.0) dLon -= 360.0;            // take the short way round
        while (dLon < -180.0) dLon += 360.0;

        OrbitalCamera cam;
        cam.subLatDeg = flight.fromLat + (flight.toLat - flight.fromLat) * k;
        cam.subLonDeg = flight.fromLon + dLon * k;
        cam.zoom = zoom;
        SetOrbitalCamera(cam);
    }
    else
    {
        // Zoom interpolates in log space: a linear ramp between 1x and 8x
        // spends most of its time already deep and reads as a lurch. The
        // camera centre follows the zoom, not the clock, so the
        // destination's screen offset falls straight to zero.
        float zoom = std::pow(flight.ratio, e);
        float k = 1.0f - (1.0f - e) / zoom;
        flight.zoomK = zoom;
        flight.camXKm = flight.targetXKm * k;
        flight.camYKm = flight.targetYKm * k;
    }

    if (flight.t >= 1.0f) flight.active = false;
}

void SiteSelectionController::LandFlight()
{
    if (flight.kind == 1)
    {
        region = flight.region;
        claimed = true;
        descent = MakeSurveyDescent(flight.toLat, flight.toLon);
        descent.levels[1] = MakeSurveyCursor(1, flight.toLat, flight.toLon);
        descent.depth = 2;
        level = 1;
    }
    else
    {
        SurveyDescend(&descent);
        level++;
    }
    flight.kind = 0;
    flight.active = false;
    landed = true;
    ArriveAtRung();
}
