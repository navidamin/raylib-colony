#include "survey_cursor.h"

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
    { "ORBITAL",     SURVEY_ORBITAL_USABLE_KM,    500.0,     true  },
    { "REGIONAL",    500.0,                       100.0,     true  },
    { "DISTRICT",    100.0,                        25.0,     true  },
    { "LOCALITY",     25.0,                         5.0,     true  },
    // The site level is where a physical object is being placed, not a
    // region chosen: free-moving, and a wider cursor because the choice
    // is now "where within this cell", not "which cell".
    { "SITE",          5.0,                         1.5,     false },
};

int SnapIndexLimit(double spanKm, double footprintKm, double phase)
{
    double limit = (spanKm - footprintKm) * 0.5;
    if (limit <= 0.0) return -1;
    return (int)std::floor(limit / footprintKm - phase + 1e-9);
}

double SnapOffset(double offsetKm, double spanKm, double footprintKm)
{
    // The level's grid is the set of footprints of the level below, laid
    // out so they tile the window: an odd count puts a cell on the
    // centre, an even count straddles it.
    int cells = (int)std::floor(spanKm / footprintKm + 0.5);
    if (cells < 1) cells = 1;
    double phase = (cells % 2 == 0) ? 0.5 : 0.0;

    double index = std::floor(offsetKm / footprintKm - phase + 0.5);
    int limit = SnapIndexLimit(spanKm, footprintKm, phase);
    if (limit < 0) return 0.0;
    if (index > (double)limit) index = (double)limit;
    if (index < (double)(-limit)) index = (double)(-limit);
    return (index + phase) * footprintKm;
}

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

double SurveyFootprintForSpan(double spanKm)
{
    if (spanKm <= 0.0) return 0.0;

    // Prefer a footprint the ladder already uses, so free zooming still
    // lands on sizes the player has learned to read. Largest that fits
    // the band wins: a bigger cursor carries a more legible readout.
    double best = 0.0;
    for (int i = 0; i < SURVEY_LEVEL_COUNT; i++)
    {
        double ratio = LADDER[i].footprintKm / spanKm;
        if (ratio >= SURVEY_CURSOR_MIN_RATIO &&
            ratio <= SURVEY_CURSOR_MAX_RATIO &&
            LADDER[i].footprintKm > best)
        {
            best = LADDER[i].footprintKm;
        }
    }
    if (best > 0.0) return best;

    // Nothing on the ladder fits this span: sit in the middle of the band.
    return spanKm * 0.20;
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

    if (cursor->snapToGrid)
    {
        cursor->offsetXKm = SnapOffset(dx, cursor->windowSpanKm,
                                       cursor->footprintKm);
        cursor->offsetYKm = SnapOffset(dy, cursor->windowSpanKm,
                                       cursor->footprintKm);
    }
    else
    {
        cursor->offsetXKm = ClampOffset(dx, cursor->windowSpanKm,
                                        cursor->footprintKm);
        cursor->offsetYKm = ClampOffset(dy, cursor->windowSpanKm,
                                        cursor->footprintKm);
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
