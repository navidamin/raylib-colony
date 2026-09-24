// The site-selection descent as the game draws it.
//
// The state is the controller's (src/SiteSelection); this file only draws
// what it says: the globe with the region under the pointer lit, the
// district and site windows on the terrain chain's own ground, the ladder
// cursor tinted by the verdict, the two cards, the prompt strip. The card
// and cursor drawing is the instrument's (tools/lunarmap), ported so the
// game and the instrument show one design; the ground is the game's.
//
// Design: docs/design/site-selection/site-selection-master-design.md, and
// game-integration-plan.md Part B (D3: the chain draws the rungs, the DEM
// only judges them).

#include "rendermanager.h"

#include "lunar_dem_shared.h"
#include "lunar_globe.h"
#include "lunar_regions.h"
#include "region_identity.h"
#include "site_selection_constants.h"
#include "survey_cursor.h"
#include "survey_hints.h"
#include "terrain_synthesis.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

namespace
{

// The survey layer's palette, from the dark kit (docs/guides/ui-panels.md).
const Color SV_SPACE      = { 6, 7, 12, 255 };
const Color SV_CARD_BG    = { 12, 12, 16, 220 };
const Color SV_LINE       = { 120, 150, 205, 255 };
const Color SV_LINE_FAINT = { 120, 150, 205, 140 };
const Color SV_DIM        = { 205, 210, 220, 255 };
const Color SV_FAINT      = { 128, 134, 146, 255 };
const Color SV_NOTE       = { 96, 104, 118, 255 };
const Color SV_GREEN      = { 60, 235, 120, 255 };
const Color SV_RED        = { 255, 70, 70, 255 };
const Color SV_AMBER      = { 235, 195, 110, 255 };
const Color SV_CURSOR     = { 232, 238, 255, 255 };
const Color SV_HINT       = { 150, 190, 255, 255 };
const Color SV_STRIP_LINE = { 90, 110, 150, 255 };
const Color SV_STRIP_TEXT = { 210, 218, 232, 255 };
const Color SV_MARKER     = { 255, 220, 150, 220 };

// The default font has no en dash ("South Pole–Aitken").
std::string Clean(const char* text)
{
    std::string s = text ? text : "";
    for (size_t at = s.find("\xE2\x80\x93"); at != std::string::npos; at = s.find("\xE2\x80\x93"))
        s.replace(at, 3, "-");
    return s;
}

int WrappedText(const char* text, int x, int y, int width, int fontSize, Color color)
{
    char word[64], line[256];
    line[0] = '\0';
    int lines = 0;
    const char* c = text;
    while (*c)
    {
        int wl = 0;
        while (*c && *c != ' ' && wl < 63) word[wl++] = *c++;
        word[wl] = '\0';
        while (*c == ' ') c++;
        char trial[256];
        std::snprintf(trial, sizeof(trial), "%s%s%s", line, line[0] ? " " : "", word);
        if (MeasureText(trial, fontSize) > width && line[0])
        {
            DrawText(line, x, y + lines * (fontSize + 5), fontSize, color);
            lines++;
            std::snprintf(line, sizeof(line), "%s", word);
        }
        else
        {
            std::snprintf(line, sizeof(line), "%s", trial);
        }
    }
    if (line[0])
    {
        DrawText(line, x, y + lines * (fontSize + 5), fontSize, color);
        lines++;
    }
    return lines;
}

// A dotted underline marks a row as hoverable -- the affordance for
// "there is a hint here".
void HintUnderline(int x, int y, int w, Color color)
{
    for (int i = 0; i < w; i += 5) DrawRectangle(x + i, y, 2, 1, color);
}

void MiniBar(int x, int y, int w, float frac, Color tint)
{
    DrawRectangle(x, y, w, 6, Color{ 34, 36, 42, 255 });
    DrawRectangle(x, y, (int)(w * Clamp(frac, 0.0f, 1.0f)), 6,
                  Color{ tint.r, tint.g, tint.b, 200 });
}

void Crosshair(Vector2 m, Color tint)
{
    DrawCircleLinesV(m, 6.0f, tint);
    DrawLineEx(Vector2{ m.x - 16, m.y }, Vector2{ m.x - 7, m.y }, 2.0f, tint);
    DrawLineEx(Vector2{ m.x + 7, m.y }, Vector2{ m.x + 16, m.y }, 2.0f, tint);
    DrawLineEx(Vector2{ m.x, m.y - 16 }, Vector2{ m.x, m.y - 7 }, 2.0f, tint);
    DrawLineEx(Vector2{ m.x, m.y + 7 }, Vector2{ m.x, m.y + 16 }, 2.0f, tint);
}

// A named region is a circle ON A SPHERE: walk its rim in real bearings
// and project each point, dropping the ones turned away, so a feature
// near the limb foreshortens instead of spilling off the edge.
void GlobeCircleAt(double latDeg, double lonDeg, double radiusKm,
                   int w, int h, Vector2* out, bool* vis, int n)
{
    double angRad = (radiusKm / MOON_KM_PER_DEG) * DEG2RAD;
    double lat = latDeg * DEG2RAD, lon = lonDeg * DEG2RAD;
    for (int i = 0; i < n; i++)
    {
        double bearing = (2.0 * PI * i) / n;
        double sinLat = std::sin(lat) * std::cos(angRad)
                      + std::cos(lat) * std::sin(angRad) * std::cos(bearing);
        sinLat = std::clamp(sinLat, -1.0, 1.0);
        double pLat = std::asin(sinLat);
        double pLon = lon + std::atan2(std::sin(bearing) * std::sin(angRad) * std::cos(lat),
                                       std::cos(angRad) - std::sin(lat) * sinLat);
        float sx = 0.0f, sy = 0.0f;
        vis[i] = OrbitalLatLonToScreen(pLat / DEG2RAD, pLon / DEG2RAD, w, h, &sx, &sy);
        out[i] = Vector2{ sx, sy };
    }
}

void GlobeRing(const Vector2* pts, const bool* vis, int n, Color c, float thick)
{
    for (int i = 0; i < n; i++)
    {
        int j = (i + 1) % n;
        if (!vis[i] || !vis[j]) continue;     // crossing the limb
        DrawLineEx(pts[i], pts[j], thick, c);
    }
}

void GlobeFeatureOutlines(int w, int h, int hoverFeature, Color hoverTint)
{
    const int N = 48;
    Vector2 pts[N]; bool vis[N];
    const std::vector<LunarRegion>& all = GetLunarRegions();
    for (int i = 0; i < (int)all.size(); i++)
    {
        const LunarRegion& f = all[i];
        float cx = 0.0f, cy = 0.0f;
        if (!OrbitalLatLonToScreen(f.latDeg, f.lonDeg, w, h, &cx, &cy)) continue;
        GlobeCircleAt(f.latDeg, f.lonDeg, f.radiusKm, w, h, pts, vis, N);
        if (i == hoverFeature)
        {
            // The one shape that is both real data and pickable, so the
            // only one allowed to lift off the imagery.
            GlobeRing(pts, vis, N, hoverTint, 2.0f);
            GlobeRing(pts, vis, N, Color{ hoverTint.r, hoverTint.g, hoverTint.b, 120 }, 4.0f);
        }
        else
        {
            GlobeRing(pts, vis, N, Color{ 255, 255, 255, 70 }, 1.0f);
        }
    }
}

// The cursor on the globe: the district window the next rung would
// open, a box in km on the sphere.
void GlobeCursorBox(double latDeg, double lonDeg, double spanKm, int w, int h, Color tint)
{
    double halfLat = (spanKm * 0.5) / MOON_KM_PER_DEG;
    double cosLat = std::max(0.2, std::cos(latDeg * DEG2RAD));
    double halfLon = halfLat / cosLat;

    const int PER_EDGE = 10;
    const int N = PER_EDGE * 4;
    Vector2 pts[N]; bool vis[N];
    int k = 0;
    for (int e = 0; e < 4; e++)
    {
        for (int s = 0; s < PER_EDGE; s++)
        {
            double u = (double)s / PER_EDGE;
            double dLat = 0.0, dLon = 0.0;
            if (e == 0) { dLat = -halfLat; dLon = -halfLon + 2 * halfLon * u; }
            if (e == 1) { dLon =  halfLon; dLat = -halfLat + 2 * halfLat * u; }
            if (e == 2) { dLat =  halfLat; dLon =  halfLon - 2 * halfLon * u; }
            if (e == 3) { dLon = -halfLon; dLat =  halfLat - 2 * halfLat * u; }
            float sx = 0.0f, sy = 0.0f;
            vis[k] = OrbitalLatLonToScreen(latDeg + dLat, lonDeg + dLon, w, h, &sx, &sy);
            pts[k] = Vector2{ sx, sy };
            k++;
        }
    }
    GlobeRing(pts, vis, N, tint, 2.0f);
}

// The hover chip: what is under the cursor, named. The whole answer on
// the globe -- no numbers, no rectangle.
void HoverChip(float mx, float my, const char* name, const char* sub, Color tint,
               int screenW, const char* coord)
{
    std::string n = Clean(name), su = Clean(sub);
    int tw = std::max(MeasureText(n.c_str(), 19), MeasureText(su.c_str(), 12));
    if (coord) tw = std::max(tw, MeasureText(coord, 12));
    int bw = tw + 26, bh = coord ? 64 : 46;
    float bx = mx + 22.0f, by = my - 12.0f;
    if (bx + bw > screenW - 8) bx = mx - bw - 22.0f;
    if (bx < 8.0f) bx = 8.0f;
    DrawRectangle((int)bx, (int)by, bw, bh, Color{ 12, 12, 16, 226 });
    DrawRectangleLinesEx(Rectangle{ bx, by, (float)bw, (float)bh }, 2.0f, tint);
    DrawText(n.c_str(), (int)bx + 12, (int)by + 6, 19, WHITE);
    DrawText(su.c_str(), (int)bx + 12, (int)by + 28, 12, Color{ 175, 180, 192, 255 });
    if (coord) DrawText(coord, (int)bx + 12, (int)by + 45, 12, SV_HINT);
}

// Named-region boundaries at the window rungs: arcs of the real feature
// circles crossing the view, over the ground.
void FeatureArcsInWindow(const SurveyCursor& c, const SurveyViewport& vp, int w, int h)
{
    double cLat = c.windowLatDeg, cLon = c.windowLonDeg, spanKm = c.windowSpanKm;
    float pxPerKm = vp.height / (float)spanKm;
    float cx0 = vp.x + vp.width * 0.5f, cy0 = vp.y + vp.height * 0.5f;
    double cosC = Clamp((float)std::cos(cLat * DEG2RAD), 0.05f, 1.0f);
    const char* insideName = nullptr;
    const std::vector<LunarRegion>& all = GetLunarRegions();
    for (int i = 0; i < (int)all.size(); i++)
    {
        const LunarRegion& f = all[i];
        double dist = LunarRegionDistanceKm(f, cLat, cLon);
        if (dist > f.radiusKm + spanKm) continue;
        if (dist < f.radiusKm - spanKm)
        {
            if (!insideName) insideName = f.name.c_str();
            continue;
        }
        double rDeg = f.radiusKm / MOON_KM_PER_DEG;
        Vector2 prev = { 0 };
        bool havePrev = false;
        Vector2 best = { 0 };
        float bestD = 1e9f;
        for (int k = 0; k <= 720; k++)
        {
            double t = k / 720.0 * 2.0 * PI;
            double la = f.latDeg + rDeg * std::cos(t);
            double lo = f.lonDeg + rDeg * std::sin(t) /
                        Clamp((float)std::cos(la * DEG2RAD), 0.2f, 1.0f);
            float x = (float)(cx0 + (lo - cLon) * MOON_KM_PER_DEG * cosC * pxPerKm);
            float y = (float)(cy0 - (la - cLat) * MOON_KM_PER_DEG * pxPerKm);
            Vector2 pt = { x, y };
            bool on = x > -w && x < 2 * w && y > -h && y < 2 * h;
            if (on && havePrev && Vector2Distance(prev, pt) < 60.0f)
                DrawLineEx(prev, pt, 2.0f, Color{ 240, 244, 255, 150 });
            prev = pt;
            havePrev = on;
            float dC = Vector2Distance(pt, Vector2{ w * 0.5f, h * 0.5f });
            if (on && x > 20 && x < w - 20 && y > 80 && y < h - 90 && dC < bestD)
            {
                bestD = dC;
                best = pt;
            }
        }
        if (bestD < h * 0.7f)
        {
            std::string n = Clean(f.name.c_str());
            DrawText(n.c_str(), (int)best.x + 8, (int)best.y + 6, 15, Color{ 240, 244, 255, 200 });
        }
    }
    if (insideName)
    {
        std::string n = Clean(insideName);
        int tw = MeasureText(n.c_str(), 17);
        DrawText(n.c_str(), (w - tw) / 2, 58, 17, Color{ 255, 255, 255, 110 });
    }
}

// A line of text riding just above the cursor, in the cursor's own
// colour: the answer where the question is.
void CursorCallout(Rectangle r, const char* text, Color tint)
{
    const int fs = 15;
    int tw = MeasureText(text, fs);
    float bw = (float)tw + 18.0f, bh = 24.0f;
    float bx = r.x + r.width * 0.5f - bw * 0.5f;
    float by = r.y - bh - 8.0f;
    if (by < 4.0f) by = r.y + r.height + 8.0f;
    DrawRectangleRec(Rectangle{ bx, by, bw, bh }, Color{ 10, 11, 16, 226 });
    DrawRectangleLinesEx(Rectangle{ bx, by, bw, bh }, 1.5f, tint);
    DrawText(text, (int)(bx + 9.0f), (int)(by + 5.0f), fs, tint);
}

void LadderCursor(const SurveyCursor& cursor, const SurveyViewport& viewport, Color tint)
{
    Rectangle r = SurveyCursorRect(cursor, viewport);
    DrawRectangleRec(r, Color{ tint.r, tint.g, tint.b, 30 });
    DrawRectangleLinesEx(r, 2.5f, tint);
    float t = r.width * 0.5f * 0.35f;
    for (int i = 0; i < 4; i++)
    {
        float ox = (i & 1) ? r.x + r.width : r.x;
        float oy = (i & 2) ? r.y + r.height : r.y;
        float sx = (i & 1) ? -1.0f : 1.0f;
        float sy = (i & 2) ? -1.0f : 1.0f;
        DrawLineEx(Vector2{ ox, oy }, Vector2{ ox + sx * t, oy }, 4.0f, tint);
        DrawLineEx(Vector2{ ox, oy }, Vector2{ ox, oy + sy * t }, 4.0f, tint);
    }
    DrawCircleV(Vector2{ r.x + r.width * 0.5f, r.y + r.height * 0.5f }, 3.0f, tint);
}

const char* CoordText(double lat, double lon)
{
    return TextFormat("%.1f%c  %.1f%c",
                      std::fabs(lat), (lat >= 0.0) ? 'N' : 'S',
                      std::fabs(lon), (lon >= 0.0) ? 'E' : 'W');
}

} // namespace

// ---------------------------------------------------------------------------

void RenderManager::DrawSurveyView(const SiteSelectionController& ctl, const SurveyLayout& layout,
                                   Planet* planet, std::vector<Colony*>& colonies,
                                   const Colony* current)
{
    (void)planet;
    int w = GetScreenWidth();
    int h = GetScreenHeight();
    pendingHintRowY = -1;
    pendingHint = { nullptr, nullptr };

    if (ctl.FlightActive())
    {
        SurveyDrawFlight(ctl, colonies, current, w, h);
        return;
    }
    if (ctl.Level() == 0) SurveyDrawGlobeRung(ctl, layout, colonies, current, w, h);
    else                  SurveyDrawWindowRung(ctl, layout, colonies, current, w, h);
    SurveyDrawHintTooltip();
    SurveyDrawStrip(ctl, layout, w, h);
}

// ---------------------------------------------------------------------------
// Rung 0: the globe. The region under the pointer lights up and names
// itself; the card previews it; a click claims it.

void RenderManager::SurveyDrawGlobeRung(const SiteSelectionController& ctl,
                                        const SurveyLayout& layout,
                                        std::vector<Colony*>& colonies, const Colony* current,
                                        int w, int h)
{
    ClearBackground(SV_SPACE);
    if (LunarGlobeReady())
    {
        DrawLunarGlobe(w, h);
    }
    else if (orbitalNearTexture.id != 0)
    {
        DrawTexture(orbitalNearTexture, (w - orbitalNearTexture.width) / 2,
                    (h - orbitalNearTexture.height) / 2, WHITE);
    }

    const SurveyLevelDef* ladder = GetSurveyLadder();
    const RegionIdentity& hoverId = ctl.Hover();
    Color tint = GetSiteArchetypeDescriptor(hoverId.archetype).tint;
    bool onGround = ctl.OnGround();
    Vector2 m = layout.pointer;

    GlobeFeatureOutlines(w, h, hoverId.featureIndex, tint);
    if (onGround)
    {
        // The cursor is what you are about to take, not a note about it.
        GlobeCursorBox(ctl.HoverLat(), ctl.HoverLon(), ladder[0].footprintKm, w, h, tint);
    }

    int hoverIndex = 0;
    SurveyDrawGlobeMarkers(colonies, current, m, w, h, &hoverIndex);

    // Header: where the globe is turned.
    const OrbitalCamera& cam = GetOrbitalCamera();
    DrawText("LUNAR ORBIT", 14, 12, 19, Color{ 232, 236, 245, 255 });
    DrawText(TextFormat("sub-point %s      x%.1f", CoordText(cam.subLatDeg, cam.subLonDeg), cam.zoom),
             14, 36, 13, Color{ 150, 160, 180, 255 });

    if (hoverIndex > 0)
    {
        HoverChip(m.x, m.y, TextFormat("COLONY %d", hoverIndex), "click to open it",
                  GOLD, w, nullptr);
    }
    else if (onGround)
    {
        Crosshair(m, tint);
        if (!layout.hintKey)
        {
            HoverChip(m.x, m.y, hoverId.name, hoverId.terrane, tint, w,
                      CoordText(ctl.HoverLat(), ctl.HoverLon()));
        }
    }
    if (onGround)
    {
        float psrKm = (std::fabs(ctl.HoverLat()) > 80.0) ? 4.0f : 999.0f;
        SurveyDrawRegionCard(ctl.Shown(), 0, layout.regionX, layout.regionY, layout.cardW,
                             layout.hintKey, psrKm);
    }
}

// ---------------------------------------------------------------------------
// Rungs 1 and 2: a window on the ground, the cursor the next rung's
// footprint (or, at the site, the base's own).

void RenderManager::SurveyDrawWindowRung(const SiteSelectionController& ctl,
                                         const SurveyLayout& layout,
                                         std::vector<Colony*>& colonies, const Colony* current,
                                         int w, int h)
{
    ClearBackground(BLACK);
    const SurveyCursor* c = ctl.Cursor();
    const SurveyViewport& vp = ctl.Viewport();
    const SurveyLevelDef* ladder = GetSurveyLadder();
    bool siteRung = ctl.AtSiteRung();
    bool haveVerdict = ctl.HaveVerdict();
    const PlacementVerdict& verdict = ctl.Verdict();

    SurveyDrawGround(*c, vp, ctl.ZoomK(), w, h);
    if (c->windowSpanKm >= 25.0) FeatureArcsInWindow(*c, vp, w, h);
    SurveyDrawWindowMarkers(*c, vp, colonies, current);

    Color tint = haveVerdict ? (verdict.allowed ? SV_GREEN : SV_RED) : SV_CURSOR;
    LadderCursor(*c, vp, tint);
    if (siteRung && !ctl.Founded())
    {
        const char* say = (haveVerdict && verdict.allowed)
            ? "Click to found the colony here"
            : "Refused - move to better ground";
        CursorCallout(SurveyCursorRect(*c, vp), say, tint);
    }

    // Header: the rung, its window, where it is.
    double lat = 0.0, lon = 0.0;
    SurveyCursorLatLon(*c, &lat, &lon);
    DrawText(TextFormat("LEVEL %d  %s", ctl.Level() + 1, ladder[ctl.Level()].name),
             14, 12, 19, Color{ 232, 236, 245, 255 });
    DrawText(TextFormat("%.0f km window     cursor %.1f km     %s",
                        c->windowSpanKm, c->footprintKm, CoordText(lat, lon)),
             14, 36, 13, Color{ 150, 160, 180, 255 });

    // Cards. The region is fixed from the claim; the level card is the
    // rung's own measured ground.
    const RegionIdentity& shown = ctl.Shown();
    float psrKm = (std::fabs(lat) > 80.0) ? 4.0f : 999.0f;
    int levelY = layout.levelY;
    if (layout.fullRegionCard)
    {
        SurveyDrawRegionCard(shown, ctl.Level(), layout.regionX, layout.regionY, layout.cardW,
                             layout.hintKey, psrKm);
    }
    else
    {
        // Narrow: the region reduced to its name and archetype -- claimed,
        // fixed, and no longer the question being asked.
        int sh = 30;
        const SiteArchetypeDescriptor& arch = GetSiteArchetypeDescriptor(shown.archetype);
        DrawRectangle(layout.regionX, layout.regionY, layout.cardW, sh, SV_CARD_BG);
        DrawRectangleLinesEx(Rectangle{ (float)layout.regionX, (float)layout.regionY,
                                        (float)layout.cardW, (float)sh }, 1.0f, SV_LINE);
        std::string n = Clean(shown.name);
        DrawText(n.c_str(), layout.regionX + 10, layout.regionY + 8, 15, WHITE);
        int aw = MeasureText(arch.name, 12) + 12;
        DrawRectangleLinesEx(Rectangle{ (float)(layout.regionX + layout.cardW - aw - 8),
                                        (float)(layout.regionY + 5), (float)aw, 20.0f },
                             1.0f, arch.tint);
        DrawText(arch.name, layout.regionX + layout.cardW - aw - 2, layout.regionY + 9, 12,
                 arch.tint);
    }
    SurveyDrawLevelCard(ctl.Level(), ctl.Ground(),
                        haveVerdict ? &ctl.Buildability() : nullptr,
                        haveVerdict ? &verdict : nullptr,
                        layout.levelX, levelY, layout.cardW);
}

// The rung's ground: the window's chain texture, widened past the rung's
// span to cover the screen, drawn through the controller's viewport so
// zoom and camera come for free. Zoomed out past the window, the picture
// is the same cache's 1/zoomMin window.
void RenderManager::SurveyDrawGround(const SurveyCursor& cursor, const SurveyViewport& vp,
                                     float zoomK, int w, int h)
{
    (void)w; (void)h;
    double spanKm = cursor.windowSpanKm;
    if (zoomK < 1.0f - 1e-4f)
    {
        double zoomMin = SurveyZoomMin(cursor.level);
        if (zoomMin > 0.0 && zoomMin < 1.0) spanKm = cursor.windowSpanKm / zoomMin;
    }
    float texSpan = WindowTextureSpanKm(spanKm);
    LunarPoint centre;
    centre.latDeg = cursor.windowLatDeg;
    centre.lonDeg = cursor.windowLonDeg;
    EnsureTerrainAt(centre, texSpan);
    const Texture2D* tex = BoundTerrainWindow();
    if (!tex) return;

    float pxPerKm = vp.height / (float)cursor.windowSpanKm;
    float side = texSpan * pxPerKm;
    Vector2 vc = { vp.x + vp.width * 0.5f, vp.y + vp.height * 0.5f };
    Rectangle src = { 0, 0, (float)tex->width, (float)tex->height };
    Rectangle dst = { vc.x - side * 0.5f, vc.y - side * 0.5f, side, side };
    DrawTexturePro(*tex, src, dst, Vector2{ 0, 0 }, 0.0f, WHITE);
}

// A descent in flight: leaving the globe, the globe under the moving
// camera; diving into a window, the rung being left under the zoom.
void RenderManager::SurveyDrawFlight(const SiteSelectionController& ctl,
                                     std::vector<Colony*>& colonies, const Colony* current,
                                     int w, int h)
{
    if (ctl.FlightKind() == 1)
    {
        ClearBackground(SV_SPACE);
        if (LunarGlobeReady()) DrawLunarGlobe(w, h);
        int hoverIndex = 0;
        SurveyDrawGlobeMarkers(colonies, current, Vector2{ -1000.0f, -1000.0f }, w, h, &hoverIndex);
    }
    else
    {
        ClearBackground(BLACK);
        const SurveyCursor* c = ctl.Cursor();
        float zoomK = ctl.FlightZoomK();
        float pxPerKm = (float)h * zoomK / (float)c->windowSpanKm;
        SurveyViewport vp;
        vp.width = (float)h * zoomK;
        vp.height = (float)h * zoomK;
        float centreX = w * 0.5f - (float)ctl.FlightCamXKm() * pxPerKm;
        float centreY = h * 0.5f + (float)ctl.FlightCamYKm() * pxPerKm;
        vp.x = centreX - vp.width * 0.5f;
        vp.y = centreY - vp.height * 0.5f;
        SurveyDrawGround(*c, vp, zoomK, w, h);
    }
    // The strip stays put so the frame does not read as a different UI.
    int sh = SURVEY_STRIP_H, y = h - sh;
    DrawRectangle(0, y, w, sh, SV_CARD_BG);
    DrawRectangle(0, y, w, 1, SV_STRIP_LINE);
    DrawText("Descending...", 16, y + 12, 16, SV_HINT);
}

// ---------------------------------------------------------------------------
// Cards

// The frozen region card. Drawn IDENTICALLY at every rung -- the whole
// design in one visual fact: this panel never changes below the globe.
void RenderManager::SurveyDrawRegionCard(const RegionIdentity& id, int level, int px, int py,
                                         int pw, const char* hoverKey, float psrKm)
{
    int hintRowY = -1;
    SurveyHintResult hint = { nullptr, nullptr };
    int ph = SURVEY_REGION_CARD_H;
    const SiteArchetypeDescriptor& arch = GetSiteArchetypeDescriptor(id.archetype);
    std::string name = Clean(id.name);
    std::string sub = Clean(TextFormat("%s  -  %s", id.terrane, id.rock));

    DrawRectangle(px, py, pw, ph, SV_CARD_BG);
    DrawRectangleLinesEx(Rectangle{ (float)px, (float)py, (float)pw, (float)ph }, 2.0f, SV_LINE);
    DrawText(level == 0 ? "REGION - CLAIMING" : "REGION - FIXED AT LEVEL 1",
             px + 12, py + 10, 15, SV_LINE);
    DrawText(name.c_str(), px + 12, py + 32, 25, WHITE);
    DrawText(sub.c_str(), px + 12, py + 62, 13, SV_FAINT);
    HintUnderline(px + 12, py + 77, MeasureText(sub.c_str(), 13), SV_LINE_FAINT);
    if (hoverKey && std::strcmp(hoverKey, "rock") == 0)
    {
        hintRowY = py + 62;
        hint = GetRockHint(id.rock);
    }

    // archetype chip
    int chipW = MeasureText(arch.name, 14) + 16;
    DrawRectangle(px + 12, py + 82, chipW, 22, Color{ arch.tint.r, arch.tint.g, arch.tint.b, 40 });
    DrawRectangleLinesEx(Rectangle{ (float)(px + 12), (float)(py + 82), (float)chipW, 22.0f },
                         1.0f, arch.tint);
    DrawText(arch.name, px + 20, py + 86, 14, arch.tint);

    int rowY = py + 116;
    struct Row { const char* label; const char* key; float value; float scale; const char* unit; };
    const Row rows[3] = {
        { "iron",     "iron",     id.fePct, 22.0f, "wt%%" },
        { "titanium", "titanium", id.tiPct, 13.0f, "wt%%" },
        { "thorium",  "thorium",  id.thPpm, 12.0f, "ppm" },
    };
    for (int i = 0; i < 3; i++)
    {
        const Row& r = rows[i];
        DrawText(r.label, px + 12, rowY, 14, SV_DIM);
        HintUnderline(px + 12, rowY + 15, MeasureText(r.label, 14), SV_LINE_FAINT);
        DrawText(TextFormat(TextFormat("%%.1f %s", r.unit), r.value), px + pw - 88, rowY, 14, SV_DIM);
        MiniBar(px + 12, rowY + 17, pw - 24, r.value / r.scale, SV_LINE);
        if (hoverKey && std::strcmp(hoverKey, r.key) == 0)
        {
            hintRowY = rowY;
            hint = GetSurveyHint(r.key, r.value);
        }
        rowY += (i == 2) ? 34 : 32;
    }
    DrawText(id.latitudeNote, px + 12, rowY, 13, SV_FAINT);
    HintUnderline(px + 12, rowY + 15, MeasureText(id.latitudeNote, 13), SV_LINE_FAINT);
    if (hoverKey && std::strcmp(hoverKey, "psr") == 0)
    {
        hintRowY = rowY;
        hint = GetPsrHint(psrKm);
    }
    DrawText("orbital survey - one value per region, never refines",
             px + 12, py + ph - 20, 12, SV_NOTE);

    if (hintRowY >= 0)
    {
        // Queued, not drawn: the tooltip must sit on top of every card,
        // so the pass flushes it last.
        pendingHint = hint;
        pendingHintRowY = hintRowY;
        pendingHintRight = px + pw;
    }
}

void RenderManager::SurveyDrawHintTooltip()
{
    if (pendingHintRowY < 0 || !pendingHint.title) { pendingHintRowY = -1; return; }
    int rowY = pendingHintRowY, x = pendingHintRight + 14, pad = 12, w = 306;
    int lines = WrappedText(pendingHint.text, -4000, -4000, w - 2 * pad, 13, BLANK);
    int h = 34 + lines * 18 + pad;
    DrawLineEx(Vector2{ (float)pendingHintRight, (float)rowY + 7.0f },
               Vector2{ (float)x, (float)rowY + 7.0f }, 1.0f,
               Color{ SV_HINT.r, SV_HINT.g, SV_HINT.b, 130 });
    DrawRectangle(x, rowY - 10, w, h, Color{ 16, 17, 22, 236 });
    DrawRectangleLinesEx(Rectangle{ (float)x, (float)(rowY - 10), (float)w, (float)h }, 1.0f, SV_HINT);
    DrawText(pendingHint.title, x + pad, rowY - 10 + 10, 14, SV_HINT);
    WrappedText(pendingHint.text, x + pad, rowY - 10 + 32, w - 2 * pad, 13,
                Color{ 208, 212, 222, 255 });
    pendingHintRowY = -1;
    pendingHint.title = nullptr;
}

// The per-rung question card: the rung's own measured geometry.
void RenderManager::SurveyDrawLevelCard(int level, const GroundStats& g,
                                        const TerrainBuildability* siteB,
                                        const PlacementVerdict* verdict, int px, int py, int pw)
{
    int ph = SurveyLevelCardHeight(level);
    Color line = (level == SITE_LEVELS - 1 && verdict)
        ? (verdict->allowed ? SV_GREEN : SV_RED) : SV_CURSOR;
    DrawRectangle(px, py, pw, ph, SV_CARD_BG);
    DrawRectangleLinesEx(Rectangle{ (float)px, (float)py, (float)pw, (float)ph }, 2.0f, line);
    DrawText(TextFormat("LEVEL %d / %d", level + 1, SITE_LEVELS), px + 12, py + 10, 15, SV_FAINT);
    DrawText(SiteLevelQuestion(level), px + 12, py + 30, 21, line);

    int rowY = py + 64;
    if (level == 0)
    {
        DrawText("terrane, rock and latitude decide the", px + 12, rowY, 14, SV_DIM);
        DrawText("economy. Chemistry locks HERE - the",   px + 12, rowY + 19, 14, SV_DIM);
        DrawText("region card never changes below this.", px + 12, rowY + 38, 14, SV_DIM);
        DrawText("click = claim the region, fly to its district", px + 12, rowY + 62, 13, SV_FAINT);
        return;
    }

    if (level <= SITE_LEVELS - 2)
    {
        DrawText("window mean slope", px + 12, rowY, 14, SV_DIM);
        DrawText(TextFormat("%.1f deg", g.meanSlope), px + pw - 92, rowY, 14, SV_DIM);
        rowY += 21;
        DrawText("buildable ground", px + 12, rowY, 14, SV_DIM);
        Color bTint = g.buildableFrac > 0.7f ? Color{ 120, 220, 140, 255 }
                    : g.buildableFrac > 0.3f ? SV_AMBER : Color{ 240, 120, 100, 255 };
        DrawText(TextFormat("%.0f %%", g.buildableFrac * 100.0f), px + pw - 92, rowY, 14, bTint);
        rowY += 21;
        DrawText("relief", px + 12, rowY, 14, SV_DIM);
        DrawText(TextFormat("%.0f m", g.reliefM), px + pw - 92, rowY, 14, SV_DIM);
        rowY += 25;
        DrawText("click = descend into the cursor", px + 12, rowY, 13, SV_FAINT);
    }
    else if (siteB)
    {
        // Same source as the verdict below it, or the panel argues with
        // itself.
        Color bad = Color{ 240, 120, 100, 255 };
        DrawText("mean slope", px + 12, rowY, 14, SV_DIM);
        DrawText(TextFormat("%.1f deg", siteB->meanSlopeDeg), px + pw - 92, rowY, 14,
                 siteB->meanSlopeDeg > SITE_MAX_MEAN_SLOPE_DEG ? bad : SV_DIM);
        rowY += 21;
        DrawText("peak slope", px + 12, rowY, 14, SV_DIM);
        DrawText(TextFormat("%.1f deg", siteB->maxSlopeDeg), px + pw - 92, rowY, 14,
                 siteB->maxSlopeDeg > SITE_MAX_PEAK_SLOPE_DEG ? bad : SV_DIM);
        rowY += 21;
        DrawText("roughness", px + 12, rowY, 14, SV_DIM);
        DrawText(TextFormat("%.0f m", siteB->roughnessM), px + pw - 92, rowY, 14,
                 siteB->roughnessM > SITE_MAX_ROUGHNESS_M ? bad : SV_DIM);
        rowY += 21;
        DrawText("relief", px + 12, rowY, 14, SV_DIM);
        DrawText(TextFormat("%.0f m", siteB->reliefM), px + pw - 92, rowY, 14, SV_DIM);
        rowY += 25;
    }
    else
    {
        DrawText("no elevation model: the ground cannot be judged", px + 12, rowY, 13, SV_FAINT);
        return;
    }

    if (level == SITE_LEVELS - 1 && siteB && verdict)
    {
        DrawText(TextFormat("illumination   %.0f %%", siteB->illumination * 100.0f),
                 px + 12, rowY, 14, SV_DIM);
        rowY += 21;
        DrawText(TextFormat("perm. shadow   %s", siteB->isPsr ? "YES" : "no"),
                 px + 12, rowY, 14, siteB->isPsr ? SV_HINT : SV_DIM);
        rowY += 21;
        DrawText(TextFormat("earth link     %.0f %%", siteB->earthVisibility * 100.0f),
                 px + 12, rowY, 14, SV_DIM);
        rowY += 27;
        DrawText(verdict->reason, px + 12, rowY, 16, line);
    }
}

// ---------------------------------------------------------------------------
// The prompt strip: what a click does here, and the way back.

void RenderManager::SurveyDrawStrip(const SiteSelectionController& ctl, const SurveyLayout& layout,
                                    int w, int h)
{
    bool narrow = layout.narrow;
    bool siteRung = ctl.AtSiteRung();
    const char* msg;
    if (ctl.Founded())
        msg = "COLONY FOUNDED.";
    else if (ctl.Level() == 0 && layout.polarBlocked)
        msg = narrow ? "Polar cap: not drawn yet."
                     : "Inside the polar cap: the window there is not drawn truthfully yet, so it cannot be claimed.";
    else if (ctl.Level() == 0)
        msg = ctl.TouchStyle()
            ? (narrow ? "Tap to aim, tap again to claim."
                      : "Tap to aim - the region under the mark names itself.  Tap again to claim it.")
            : (narrow ? "Click a region to claim it."
                      : layout.globeSpin
                        ? "The region under the cursor names itself.  Click to claim it.  A colony's marker opens it.  Right-click: spin off."
                        : "The region under the cursor names itself.  Click to claim it.  A colony's marker opens it.  Right-click: spin on.");
    else if (siteRung)
    {
        if (ctl.HaveVerdict() && ctl.Verdict().allowed)
            msg = narrow ? "Found the colony here." : "Click to found the colony here.  Esc to back out.";
        else if (ctl.HaveVerdict())
            msg = narrow ? "Refused - move to better ground."
                         : "Red: this ground is refused. Move to better ground.  Esc to back out.";
        else
            msg = "No elevation model: this ground cannot be judged.  Esc to back out.";
    }
    else
        msg = ctl.TouchStyle()
            ? (narrow ? "Tap to aim, tap again to descend."
                      : "Tap to aim, tap again to descend into the cursor.  Esc to back out.")
            : (narrow ? "Click to descend." : "Click to descend into the cursor.  Esc to back out.");

    int sh = SURVEY_STRIP_H, y = h - sh;
    DrawRectangle(0, y, w, sh, SV_CARD_BG);
    DrawRectangle(0, y, w, 1, SV_STRIP_LINE);
    int msgX = 16;
    if (layout.backShown)
    {
        const Rectangle& b = layout.backBtn;
        bool over = layout.pointerOnBack;
        DrawRectangleRec(b, over ? Color{ 44, 52, 70, 255 } : Color{ 30, 34, 44, 255 });
        DrawRectangleLinesEx(b, 1.0f, Color{ 120, 145, 190, 255 });
        DrawText("< BACK", (int)b.x + 9, (int)b.y + 5, 14, Color{ 190, 205, 230, 255 });
        msgX = (int)(b.x + b.width) + 14;
    }
    DrawText(msg, msgX, y + 12, narrow ? 14 : 16, SV_STRIP_TEXT);
    if (!narrow)
    {
        const char* lvl = TextFormat("LEVEL %d / %d", ctl.Level() + 1, SITE_LEVELS);
        DrawText(lvl, w - MeasureText(lvl, 16) - 16, y + 12, 16, SV_HINT);
    }
}

// ---------------------------------------------------------------------------
// Colonies, wherever they fall in the picture.

void RenderManager::SurveyDrawGlobeMarkers(std::vector<Colony*>& colonies, const Colony* current,
                                           Vector2 pointer, int w, int h, int* hoverIndex)
{
    if (hoverIndex) *hoverIndex = 0;
    int index = 0;
    for (const Colony* colony : colonies)
    {
        index++;
        if (!colony->HasCentre()) continue;
        float x, y;
        if (!OrbitalLatLonToScreen(colony->GetCentre().latDeg, colony->GetCentre().lonDeg,
                                   w, h, &x, &y))
            continue;
        bool near = Vector2Distance(pointer, Vector2{ x, y }) <= ORBITAL_MARKER_PICK_PX;
        if (near && hoverIndex) *hoverIndex = index;
        Color c = (colony == current) ? GOLD : SV_MARKER;
        float r = ORBITAL_MARKER_RADIUS_PX + (near ? 3.0f : 0.0f);
        DrawCircleV(Vector2{ x, y }, r, ColorAlpha(c, 0.25f));
        DrawRing(Vector2{ x, y }, r - 2.0f, r, 0.0f, 360.0f, 24, c);
        DrawCircleV(Vector2{ x, y }, 2.5f, c);
        DrawText(TextFormat("COLONY %d", index), (int)x + 12, (int)y - 8, 14, c);
    }
}

void RenderManager::SurveyDrawWindowMarkers(const SurveyCursor& cursor, const SurveyViewport& vp,
                                            std::vector<Colony*>& colonies, const Colony* current)
{
    float pxPerKm = vp.height / (float)cursor.windowSpanKm;
    int index = 0;
    for (const Colony* colony : colonies)
    {
        index++;
        if (!colony->HasCentre()) continue;
        double dx = 0.0, dy = 0.0;
        SurveyLatLonToOffsetKm(cursor, colony->GetCentre().latDeg, colony->GetCentre().lonDeg,
                               &dx, &dy);
        float x = 0.0f, y = 0.0f;
        SurveyOffsetKmToScreen(vp, cursor.windowSpanKm, dx, dy, &x, &y);
        float rPx = std::max(8.0f, (float)(colony->GetRadiusKm() * pxPerKm));
        if (x < -rPx || y < -rPx || x > GetScreenWidth() + rPx || y > GetScreenHeight() + rPx)
            continue;
        Color c = (colony == current) ? GOLD : SV_MARKER;
        DrawCircleV(Vector2{ x, y }, rPx, ColorAlpha(c, 0.18f));
        DrawRing(Vector2{ x, y }, rPx - 2.0f, rPx, 0.0f, 360.0f, 48, c);
        DrawCircleV(Vector2{ x, y }, 3.0f, c);
        DrawText(TextFormat("COLONY %d", index), (int)(x + rPx + 6.0f), (int)y - 8, 14, c);
    }
}
