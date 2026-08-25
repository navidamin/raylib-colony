// Geometry self-test for the survey cursor (src/TerrainGen/survey_cursor.*).
//
// The cursor's whole job is to say "this exact ground is what you are
// about to enter", so its arithmetic has to be exact: a snap that drifts
// half a cell, or a screen mapping that disagrees with the terrain
// window, would make the descent land somewhere other than what the
// player aimed at. Runs headless, needs no GL and no DEM.
//
//   cmake --build build --target survey_cursor_test
//   ./build/src/survey_cursor_test
#include "survey_cursor.h"
#include <cmath>
#include <cstdio>

static int failures = 0;
static void Check(bool ok, const char* what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

int main()
{
    const SurveyLevelDef* ladder = GetSurveyLadder();

    // 1. Every ladder level sits inside the 15-30% band.
    for (int i = 0; i < SURVEY_LEVEL_COUNT; i++)
    {
        double ratio = ladder[i].footprintKm / ladder[i].windowSpanKm;
        printf("   level %d %-9s span %8.1f km  cursor %6.1f km  ratio %.3f\n",
               i + 1, ladder[i].name, ladder[i].windowSpanKm,
               ladder[i].footprintKm, ratio);
        Check(ratio >= SURVEY_CURSOR_MIN_RATIO - 0.01 &&
              ratio <= SURVEY_CURSOR_MAX_RATIO + 0.001, "ratio in band");
    }

    // 2. Each level's cursor is the next level's window.
    for (int i = 0; i + 1 < SURVEY_LEVEL_COUNT; i++)
    {
        Check(std::fabs(ladder[i].footprintKm - ladder[i + 1].windowSpanKm) < 1e-9,
              "cursor == child window");
    }

    // 3. Screen <-> km round trip, non-square viewport.
    SurveyViewport vp = { 100.0f, 40.0f, 1600.0f, 900.0f };
    double dx = 0.0, dy = 0.0;
    float sx = 0.0f, sy = 0.0f;
    SurveyScreenToOffsetKm(vp, 25.0, 100.0f + 1600.0f * 0.5f + 90.0f,
                           40.0f + 900.0f * 0.5f - 36.0f, &dx, &dy);
    SurveyOffsetKmToScreen(vp, 25.0, dx, dy, &sx, &sy);
    Check(std::fabs(sx - (100.0f + 800.0f + 90.0f)) < 0.01f &&
          std::fabs(sy - (40.0f + 450.0f - 36.0f)) < 0.01f,
          "screen -> km -> screen round trip");
    // 900 px over 25 km = 36 px/km, so 90 px east = 2.5 km, 36 px up = 1 km.
    Check(std::fabs(dx - 2.5) < 1e-6 && std::fabs(dy - 1.0) < 1e-6,
          "km offsets use the smaller viewport dimension");

    // 4. Snapping: the LOCALITY level (25 km window, 5 km cursor) has an
    //    odd cell count, so a cell sits on the centre.
    SurveyViewport square = { 0.0f, 0.0f, 1000.0f, 1000.0f };
    SurveyCursor local = MakeSurveyCursor(3, 0.0, 0.0);
    SurveyCursorTrack(&local, square, 500.0f + 40.0f * 5.6f, 500.0f);
    Check(std::fabs(local.offsetXKm - 5.0) < 1e-9, "snap to 5 km grid (odd)");

    // 5. Snapping: DISTRICT (100 km / 25 km) has an even cell count, so
    //    cells straddle the centre at +-12.5 km.
    SurveyCursor district = MakeSurveyCursor(2, 0.0, 0.0);
    SurveyCursorTrack(&district, square, 500.0f + 10.0f * 3.0f, 500.0f);
    Check(std::fabs(district.offsetXKm - 12.5) < 1e-9, "snap straddles centre (even)");

    // 6. The footprint never leaves the window, however far the mouse goes.
    for (int i = 0; i < SURVEY_LEVEL_COUNT; i++)
    {
        SurveyCursor c = MakeSurveyCursor(i, 12.0, -30.0);
        SurveyCursorTrack(&c, square, 1e6f, -1e6f);
        double limit = (c.windowSpanKm - c.footprintKm) * 0.5 + 1e-9;
        bool inside = std::fabs(c.offsetXKm) <= limit &&
                      std::fabs(c.offsetYKm) <= limit;
        Rectangle r = SurveyCursorRect(c, square);
        bool onScreen = r.x >= -0.01f && r.y >= -0.01f &&
                        r.x + r.width <= 1000.01f &&
                        r.y + r.height <= 1000.01f;
        Check(inside && onScreen, "cursor clamped inside the window");
    }

    // 7. The site level moves freely (no snap).
    SurveyCursor site = MakeSurveyCursor(4, 0.0, 0.0);
    SurveyCursorTrack(&site, square, 500.0f + 37.0f, 500.0f);
    Check(std::fabs(site.offsetXKm - 37.0 / 200.0) < 1e-6, "site level is free-moving");

    // 8. Cursor centre -> lat/lon, and back through the child window.
    SurveyCursor c3 = MakeSurveyCursor(3, 32.8, -15.6);
    c3.offsetXKm = 5.0;
    c3.offsetYKm = -5.0;
    double lat = 0.0, lon = 0.0;
    SurveyCursorLatLon(c3, &lat, &lon);
    double expectLat = 32.8 - 5.0 / 30.32268;
    double expectLon = -15.6 + 5.0 / (30.32268 * std::cos(32.8 * M_PI / 180.0));
    Check(std::fabs(lat - expectLat) < 1e-9 && std::fabs(lon - expectLon) < 1e-9,
          "cursor centre -> real lat/lon");

    // 8b. ...and straight back out again.
    SurveyCursor c3b = MakeSurveyCursor(3, 32.8, -15.6);
    double bx = 0.0, by = 0.0;
    SurveyLatLonToOffsetKm(c3b, lat, lon, &bx, &by);
    Check(std::fabs(bx - 5.0) < 1e-9 && std::fabs(by + 5.0) < 1e-9,
          "lat/lon -> km offset round trip");

    // 9. Descent stack: descending centres the child on the cursor.
    SurveyDescent d = MakeSurveyDescent(32.8, -15.6);
    Check(SurveyCurrent(&d)->level == 0 && d.depth == 1, "descent starts orbital");
    SurveyCurrent(&d)->offsetXKm = 500.0;
    SurveyCursorLatLon(*SurveyCurrent(&d), &lat, &lon);
    Check(SurveyDescend(&d), "descend from orbital");
    Check(SurveyCurrent(&d)->level == 1 &&
          std::fabs(SurveyCurrent(&d)->windowLatDeg - lat) < 1e-9 &&
          std::fabs(SurveyCurrent(&d)->windowLonDeg - lon) < 1e-9,
          "child window centred on the parent cursor");

    // 10. Reversible: ascending restores the parent cursor where it was.
    Check(SurveyAscend(&d), "ascend");
    Check(std::fabs(SurveyCurrent(&d)->offsetXKm - 500.0) < 1e-9,
          "parent cursor preserved on ascend");

    // 11. Re-entering the same region keeps the child cursor too.
    SurveyDescend(&d);
    SurveyCurrent(&d)->offsetXKm = 100.0;
    SurveyAscend(&d);
    SurveyDescend(&d);
    Check(std::fabs(SurveyCurrent(&d)->offsetXKm - 100.0) < 1e-9,
          "re-entering the same region keeps its cursor");

    // 12. ...but moving the parent cursor elsewhere resets the child.
    SurveyAscend(&d);
    SurveyCurrent(&d)->offsetXKm = -500.0;
    SurveyDescend(&d);
    Check(std::fabs(SurveyCurrent(&d)->offsetXKm) < 1e-9,
          "a different region starts centred");

    // 13. Stack bounds.
    d = MakeSurveyDescent(0.0, 0.0);
    Check(!SurveyAscend(&d), "cannot ascend past orbital");
    for (int i = 0; i + 1 < SURVEY_LEVEL_COUNT; i++) SurveyDescend(&d);
    Check(SurveyCurrent(&d)->level == SURVEY_LEVEL_COUNT - 1 &&
          !SurveyDescend(&d), "cannot descend past the site level");

    // 14. Free-zoom footprint sizing stays in the band.
    for (double span = 2.0; span < 4000.0; span *= 1.17)
    {
        double fp = SurveyFootprintForSpan(span);
        double ratio = fp / span;
        if (ratio < SURVEY_CURSOR_MIN_RATIO - 1e-9 ||
            ratio > SURVEY_CURSOR_MAX_RATIO + 1e-9)
        {
            printf("   span %.1f -> footprint %.2f (ratio %.3f)\n", span, fp, ratio);
            Check(false, "free-zoom footprint in band");
            break;
        }
    }
    Check(true, "free-zoom footprint in band across 2..4000 km");

    printf("\n%s (%d failures)\n", failures ? "FAILED" : "ALL PASS", failures);
    return failures ? 1 : 0;
}
