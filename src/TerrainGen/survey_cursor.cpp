#include "survey_cursor.h"

#include <algorithm>
#include <cmath>

// Pure geometry — no raylib drawing, no game state. See the header for
// the design this implements.

namespace
{

// pi * 1737.4 km / 180. Matches MOON_KM_PER_DEG (terrain_synthesis.h)
// and LOLA_M_PER_DEG (lola_dem.h); repeated here so the cursor module
// stays independent of both.
const double SURVEY_KM_PER_DEG = 30.32268;

const SurveyLevelDef LADDER[SURVEY_LEVEL_COUNT] =
{
    // name          window span                  footprint  snap
    //
    // Three levels: disc -> 200 -> 25, zooming 15x, 8x and (inside the
    // site level) 5x. Two rungs were dropped from the original five,
    // each for the same reason -- it answered no question of its own. The 500 km "REGIONAL" rung went first (the region card
    // freezes at level 1, so there was no new mix to read at 500 km).
    // LOCALITY went next: a 25 km window choosing a 5 km cell, followed
    // by a 5 km window placing inside it, is one decision wearing two
    // framings. The site level now spans both -- the window holds at
    // 25 km and the cursor is the 1.5 km build footprint directly, so
    // the player places the base instead of clicking down to it.
    //
    // The district widened from 100 to 200 km on 2026-09-02 so the three
    // zooms read as 15x / 8x / 5x; it no longer matches the game's
    // 100 km PLANET playfield, and its 25 km cursor sits at 12.5% of the
    // window -- which is why the band floor is 12%, not 15%.
    // Every rung's footprint is the next rung's window, which is what
    // makes the descent register: the rectangle you aim with becomes the
    // view you land in.
    { "ORBITAL",     SURVEY_ORBITAL_USABLE_KM,    200.0,     true  },
    { "DISTRICT",    200.0,                        25.0,     true  },
    // The base's own footprint, and free-moving. This rung does not
    // choose between cells, it places the base, so the rectangle is the
    // answer's real size. At 6% of the window that is under the band
    // deliberately: the band keeps a cursor you are choosing BETWEEN
    // cells with legible, and drawing a placement bigger than it is
    // would misreport the ground the verdict is measured over.
    { "SITE",         25.0,  SURVEY_BUILD_FOOTPRINT_KM,      false },
};

// The grid's identity comes from spanKm -- cell size and whether a cell
// sits on the centre -- while limitKm says how far out the player can
// actually reach on this axis. They are the same number on a square
// window and different on a wide one, and conflating them is what kept
// the cursor in the middle of the screen.
double SnapOffset(double offsetKm, double spanKm, double footprintKm,
                  double limitKm)
{
    // The level's grid is the set of footprints of the level below, laid
    // out so they tile the window: an odd count puts a cell on the
    // centre, an even count straddles it.
    int cells = (int)std::floor(spanKm / footprintKm + 0.5);
    if (cells < 1) cells = 1;
    double phase = (cells % 2 == 0) ? 0.5 : 0.0;

    // The index range is asymmetric when the grid straddles the centre:
    // with a 200 km window and a 25 km cursor the cell centres are at
    // -87.5 ... -12.5, +12.5 ... +87.5, i.e. indices -4..+3. Clamping to
    // a symmetric +-3 would refuse the westmost/southmost cell and snap
    // the cursor a whole cell away from where the player is pointing.
    double limit = (limitKm - footprintKm) * 0.5;
    if (limit <= 0.0) return 0.0;
    double maxIndex = std::floor(limit / footprintKm - phase + 1e-9);
    double minIndex = std::ceil(-limit / footprintKm - phase - 1e-9);
    if (maxIndex < minIndex) return 0.0;

    double index = std::floor(offsetKm / footprintKm - phase + 0.5);
    if (index > maxIndex) index = maxIndex;
    if (index < minIndex) index = minIndex;
    return (index + phase) * footprintKm;
}

// spanKm here is the REACHABLE extent on this axis, not the rung's
// nominal window: on a wide screen they differ, and clamping both axes to
// the nominal one is what confined the cursor to a square in the middle
// of the screen.
double ClampOffset(double offsetKm, double spanKm, double footprintKm)
{
    double limit = (spanKm - footprintKm) * 0.5;
    if (limit <= 0.0) return 0.0;
    if (offsetKm > limit) return limit;
    if (offsetKm < -limit) return -limit;
    return offsetKm;
}

} // namespace

// ---------------------------------------------------------------------------

const SurveyLevelDef* GetSurveyLadder()
{
    return LADDER;
}

double SurveyZoomMax(int level)
{
    if (level < 0 || level >= SURVEY_LEVEL_COUNT) return 1.0;
    const SurveyLevelDef& d = LADDER[level];
    double z;
    if (level == SURVEY_LEVEL_COUNT - 1)
    {
        // The site level does not zoom at all. Its window is already the
        // ground you are choosing within and its cursor is already the
        // footprint the base occupies, so zooming could only take the
        // surroundings away from the decision.
        z = 1.0;
    }
    else
    {
        // A fixed cursor: the limit is where it fills the band's ceiling.
        z = SURVEY_CURSOR_MAX_RATIO * d.windowSpanKm / d.footprintKm;
    }
    return (z < 1.0) ? 1.0 : z;
}

SurveyCursor MakeSurveyCursor(int level, double windowLatDeg,
                              double windowLonDeg)
{
    if (level < 0) level = 0;
    if (level >= SURVEY_LEVEL_COUNT) level = SURVEY_LEVEL_COUNT - 1;

    SurveyCursor cursor;
    cursor.level = level;
    cursor.windowSpanKm = LADDER[level].windowSpanKm;
    cursor.windowLatDeg = windowLatDeg;
    cursor.windowLonDeg = windowLonDeg;
    cursor.footprintKm = LADDER[level].footprintKm;
    cursor.snapToGrid = LADDER[level].snapToGrid;
    cursor.offsetXKm = 0.0;
    cursor.offsetYKm = 0.0;
    return cursor;
}

void SurveyCursorLatLon(const SurveyCursor& cursor,
                        double* latDeg, double* lonDeg)
{
    double lat = cursor.windowLatDeg + cursor.offsetYKm / SURVEY_KM_PER_DEG;

    // Longitude is measured at the WINDOW centre's latitude, not the
    // cursor's, so the mapping stays invertible: the same 1/cos(lat)
    // widening the terrain chain uses for a square-in-km window.
    double cosLat = std::cos(cursor.windowLatDeg * 3.14159265358979323846 / 180.0);
    if (cosLat < 0.05) cosLat = 0.05;
    double lon = cursor.windowLonDeg +
                 cursor.offsetXKm / (SURVEY_KM_PER_DEG * cosLat);

    if (lat > 90.0) lat = 90.0;
    if (lat < -90.0) lat = -90.0;
    while (lon > 180.0) lon -= 360.0;
    while (lon < -180.0) lon += 360.0;

    if (latDeg) *latDeg = lat;
    if (lonDeg) *lonDeg = lon;
}

void SurveyLatLonToOffsetKm(const SurveyCursor& cursor,
                            double latDeg, double lonDeg,
                            double* offsetXKm, double* offsetYKm)
{
    double cosLat = std::cos(cursor.windowLatDeg * 3.14159265358979323846 / 180.0);
    if (cosLat < 0.05) cosLat = 0.05;

    double dLon = lonDeg - cursor.windowLonDeg;
    while (dLon > 180.0) dLon -= 360.0;
    while (dLon < -180.0) dLon += 360.0;

    if (offsetXKm) *offsetXKm = dLon * SURVEY_KM_PER_DEG * cosLat;
    if (offsetYKm) *offsetYKm = (latDeg - cursor.windowLatDeg) * SURVEY_KM_PER_DEG;
}

// ---------------------------------------------------------------------------

float SurveyPixelsPerKm(const SurveyViewport& viewport, double spanKm)
{
    if (spanKm <= 0.0) return 0.0f;
    float smaller = (viewport.width < viewport.height) ? viewport.width
                                                       : viewport.height;
    return smaller / (float)spanKm;
}

void SurveyScreenToOffsetKm(const SurveyViewport& viewport, double spanKm,
                            float screenX, float screenY,
                            double* offsetXKm, double* offsetYKm)
{
    float pxPerKm = SurveyPixelsPerKm(viewport, spanKm);
    if (pxPerKm <= 0.0f)
    {
        if (offsetXKm) *offsetXKm = 0.0;
        if (offsetYKm) *offsetYKm = 0.0;
        return;
    }
    float cx = viewport.x + viewport.width * 0.5f;
    float cy = viewport.y + viewport.height * 0.5f;

    // Screen y grows down, north is up.
    if (offsetXKm) *offsetXKm = (double)((screenX - cx) / pxPerKm);
    if (offsetYKm) *offsetYKm = (double)((cy - screenY) / pxPerKm);
}

void SurveyOffsetKmToScreen(const SurveyViewport& viewport, double spanKm,
                            double offsetXKm, double offsetYKm,
                            float* screenX, float* screenY)
{
    float pxPerKm = SurveyPixelsPerKm(viewport, spanKm);
    float cx = viewport.x + viewport.width * 0.5f;
    float cy = viewport.y + viewport.height * 0.5f;

    if (screenX) *screenX = cx + (float)offsetXKm * pxPerKm;
    if (screenY) *screenY = cy - (float)offsetYKm * pxPerKm;
}

Rectangle SurveyCursorRect(const SurveyCursor& cursor,
                           const SurveyViewport& viewport)
{
    float cx = 0.0f, cy = 0.0f;
    SurveyOffsetKmToScreen(viewport, cursor.windowSpanKm,
                           cursor.offsetXKm, cursor.offsetYKm, &cx, &cy);
    float half = (float)(cursor.footprintKm * 0.5) *
                 SurveyPixelsPerKm(viewport, cursor.windowSpanKm);

    Rectangle rect;
    rect.x = cx - half;
    rect.y = cy - half;
    rect.width = half * 2.0f;
    rect.height = half * 2.0f;
    return rect;
}

void SurveyCursorTrack(SurveyCursor* cursor, const SurveyViewport& viewport,
                       float screenX, float screenY)
{
    if (!cursor) return;

    double dx = 0.0, dy = 0.0;
    SurveyScreenToOffsetKm(viewport, cursor->windowSpanKm,
                           screenX, screenY, &dx, &dy);

    // How far the cursor may go on each axis: what the viewport shows,
    // but never past the ground the window actually holds.
    float pxPerKm = SurveyPixelsPerKm(viewport, cursor->windowSpanKm);
    double ground = (cursor->groundSpanKm > 0.0) ? cursor->groundSpanKm
                                                 : cursor->windowSpanKm;
    double reachX = cursor->windowSpanKm, reachY = cursor->windowSpanKm;
    if (pxPerKm > 0.0f)
    {
        reachX = std::min((double)(viewport.width / pxPerKm), ground);
        reachY = std::min((double)(viewport.height / pxPerKm), ground);
    }

    if (cursor->snapToGrid)
    {
        cursor->offsetXKm = SnapOffset(dx, cursor->windowSpanKm,
                                       cursor->footprintKm, reachX);
        cursor->offsetYKm = SnapOffset(dy, cursor->windowSpanKm,
                                       cursor->footprintKm, reachY);
    }
    else
    {
        cursor->offsetXKm = ClampOffset(dx, reachX, cursor->footprintKm);
        cursor->offsetYKm = ClampOffset(dy, reachY, cursor->footprintKm);
    }
}

// ---------------------------------------------------------------------------

SurveyDescent MakeSurveyDescent(double latDeg, double lonDeg)
{
    SurveyDescent descent;
    descent.depth = 1;
    descent.levels[0] = MakeSurveyCursor(0, latDeg, lonDeg);
    for (int i = 1; i < SURVEY_LEVEL_COUNT; i++)
    {
        descent.levels[i] = MakeSurveyCursor(i, latDeg, lonDeg);
    }
    return descent;
}

SurveyCursor* SurveyCurrent(SurveyDescent* descent)
{
    if (!descent) return nullptr;
    int index = descent->depth - 1;
    if (index < 0) index = 0;
    if (index >= SURVEY_LEVEL_COUNT) index = SURVEY_LEVEL_COUNT - 1;
    return &descent->levels[index];
}

const SurveyCursor* SurveyCurrent(const SurveyDescent* descent)
{
    return SurveyCurrent(const_cast<SurveyDescent*>(descent));
}

bool SurveyDescend(SurveyDescent* descent)
{
    if (!descent) return false;
    if (descent->depth >= SURVEY_LEVEL_COUNT) return false;

    const SurveyCursor* parent = SurveyCurrent(descent);
    double lat = 0.0, lon = 0.0;
    SurveyCursorLatLon(*parent, &lat, &lon);

    SurveyCursor& child = descent->levels[descent->depth];

    // Re-entering the same region keeps the cursor the player left
    // there; entering a different one starts centred. The tolerance is
    // a tenth of the child's own footprint, well under any real move.
    double tolerance = child.footprintKm * 0.1 / SURVEY_KM_PER_DEG;
    bool sameGround = std::fabs(child.windowLatDeg - lat) < tolerance &&
                      std::fabs(child.windowLonDeg - lon) < tolerance;

    if (!sameGround)
    {
        child = MakeSurveyCursor(descent->depth, lat, lon);
    }
    descent->depth++;
    return true;
}

bool SurveyAscend(SurveyDescent* descent)
{
    if (!descent) return false;
    if (descent->depth <= 1) return false;
    descent->depth--;
    return true;
}
