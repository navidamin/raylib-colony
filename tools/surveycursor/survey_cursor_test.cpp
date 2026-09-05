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
        // Only the snapping levels need to tile: the site level moves
        // freely, so its cursor size is a presentation choice.
        if (ladder[i].snapToGrid)
        {
            double cells = ladder[i].windowSpanKm / ladder[i].footprintKm;
            Check(std::fabs(cells - std::floor(cells + 0.5)) < 1e-9,
                  "window is a whole number of cursors");
        }
        printf("   level %d %-9s span %8.1f km  cursor %6.1f km  ratio %.3f\n",
               i + 1, ladder[i].name, ladder[i].windowSpanKm,
               ladder[i].footprintKm, ratio);
        // The band exists so a cursor RECTANGLE is a legible fraction of
        // its view. Level 1 draws no rectangle: the orbital disc is
        // picked by hovering named regions, and the ladder renderer
        // skips it entirely (it belongs to the game's render path). Its
        // 100 km footprint is only "what the next window will be".
        // ...and only where a cell is being chosen. The site rung's
        // cursor is a placement, so the band does not govern it.
        if (i > 0 && ladder[i].snapToGrid)
        {
            Check(ratio >= SURVEY_CURSOR_MIN_RATIO - 0.01 &&
                  ratio <= SURVEY_CURSOR_MAX_RATIO + 0.001, "ratio in band");
        }
    }

    // 1b. Zooming inside a level can never reach the level below.
    //     This is what keeps the wheel from changing rung: the tightest
    //     view a rung can reach must still be wider than the next rung's
    //     whole window.
    for (int i = 0; i < SURVEY_LEVEL_COUNT; i++)
    {
        double zmax = SurveyZoomMax(i);
        double tightestKm = ladder[i].windowSpanKm / zmax;
        printf("   level %d %-9s zoom x1 .. x%.2f   tightest view %8.1f km\n",
               i + 1, ladder[i].name, zmax, tightestKm);
        Check(zmax >= 1.0, "zoom ceiling is at least 1x");
        if (i + 1 < SURVEY_LEVEL_COUNT)
        {
            Check(tightestKm > ladder[i + 1].windowSpanKm,
                  "zoomed in fully, still wider than the level below");
        }
        else
        {
            // The last rung does not zoom: it refines its cursor instead.
            Check(std::fabs(zmax - 1.0) < 1e-9, "site level does not zoom");
        }
    }

    // 1c. Zooming OUT cannot reach the rung above either. It costs a
    //     wider WINDOW -- the camera already frames the full width -- so
    //     the factor is a design constant, bounded from above.
    for (int i = 1; i < SURVEY_LEVEL_COUNT; i++)
    {
        double zmin = SurveyZoomMin(i);
        double widest = ladder[i].windowSpanKm / zmin;
        printf("   level %d %-9s zoom out to x%.2f  widest view %8.1f km "
               "(rung above: %.0f km)\n",
               i + 1, ladder[i].name, zmin, widest, ladder[i - 1].windowSpanKm);
        Check(zmin > 0.0 && zmin <= 1.0, "zoom floor in range");
        Check(widest < ladder[i - 1].windowSpanKm * 0.8,
              "zoomed out fully, still well inside the rung above");
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

    // 4. Snapping: ORBITAL (index 0: a 3000 km window, 200 km cursor)
    //    has an odd cell count -- 15 across -- so a cell sits on the
    //    centre. This was the SITE level's case until 2026-09-04, when
    //    that rung stopped snapping; orbital is the odd count left.
    SurveyViewport square = { 0.0f, 0.0f, 1000.0f, 1000.0f };
    SurveyCursor local = MakeSurveyCursor(0, 0.0, 0.0);
    // 1000 px over 3000 km is 1/3 px per km, so 74.7 px east is 224 km:
    // inside the cell centred on 200.
    SurveyCursorTrack(&local, square, 500.0f + 224.0f / 3.0f, 500.0f);
    Check(std::fabs(local.offsetXKm - 200.0) < 1e-9,
          "snap to the cell grid (odd count)");

    // 5. Snapping: DISTRICT (index 1: 200 km / 25 km) has an even cell count, so
    //    cells straddle the centre at +-12.5 km.
    SurveyCursor district = MakeSurveyCursor(1, 0.0, 0.0);
    SurveyCursorTrack(&district, square, 500.0f + 10.0f * 3.0f, 500.0f);
    Check(std::fabs(district.offsetXKm - 12.5) < 1e-9, "snap straddles centre (even)");

    // 5b. The outermost cell must be reachable in BOTH directions. With
    //     an even cell count the index range is asymmetric (-4..+3 for a
    //     200 km window and a 25 km cursor), and clamping symmetrically
    //     would snap the cursor a whole cell away from the mouse. The
    //     edge cell's centre is derived from the ladder so the check
    //     follows the table.
    const SurveyLevelDef* lad = GetSurveyLadder();
    double edgeKm = (lad[1].windowSpanKm - lad[1].footprintKm) * 0.5;
    SurveyCursor edge = MakeSurveyCursor(1, 0.0, 0.0);
    SurveyCursorTrack(&edge, square, 0.0f, 500.0f);            // far west
    Check(std::fabs(edge.offsetXKm + edgeKm) < 1e-9, "westmost cell reachable");
    SurveyCursorTrack(&edge, square, 1000.0f, 500.0f);         // far east
    Check(std::fabs(edge.offsetXKm - edgeKm) < 1e-9, "eastmost cell reachable");

    // 5c. Every snapped cell tiles the window: the union of the cells the
    //     mouse can reach must cover the whole span, with no gaps and no
    //     cell hanging over an edge.
    for (int lvl = 0; lvl < SURVEY_LEVEL_COUNT - 1; lvl++)
    {
        SurveyCursor c = MakeSurveyCursor(lvl, 0.0, 0.0);
        double half = c.windowSpanKm * 0.5;
        double worstGap = 0.0;
        double prev = -half;
        // Walk the mouse across the window and check the snapped cell
        // always contains the point it was aimed at.
        bool contains = true;
        for (int step = 0; step <= 400; step++)
        {
            double aim = -half + c.windowSpanKm * (step / 400.0);
            // nudge off the exact edges, which belong to no cell
            if (aim <= -half) aim = -half + 1e-6;
            if (aim >= half) aim = half - 1e-6;
            float mx = 0.0f, my = 0.0f;
            SurveyOffsetKmToScreen(square, c.windowSpanKm, aim, 0.0, &mx, &my);
            SurveyCursorTrack(&c, square, mx, my);
            if (std::fabs(aim - c.offsetXKm) > c.footprintKm * 0.5 + 1e-6)
            {
                contains = false;
                worstGap = std::fabs(aim - c.offsetXKm) - c.footprintKm * 0.5;
                printf("   level %d: aimed %.3f km, snapped %.3f km (cell %.3f)\n",
                       lvl + 1, aim, c.offsetXKm, c.footprintKm);
                break;
            }
            prev = aim;
        }
        (void)prev; (void)worstGap;
        Check(contains, "snapped cell contains the aimed point everywhere");
    }

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

    // 7. The site rung places the base. Its cursor is the build
    //    footprint from the moment it is made and it moves freely,
    //    because "which 5 km cell" is not the question there.
    //    1000 px over the 25 km window is 40 px/km, so 37 px is 0.925 km
    //    -- an offset the old snapping cursor swallowed whole.
    SurveyCursor site = MakeSurveyCursor(SURVEY_LEVEL_COUNT - 1, 0.0, 0.0);
    Check(!site.snapToGrid &&
          std::fabs(site.footprintKm - SURVEY_BUILD_FOOTPRINT_KM) < 1e-9,
          "site cursor is the free build footprint");
    SurveyCursorTrack(&site, square, 500.0f + 37.0f, 500.0f);
    Check(std::fabs(site.offsetXKm - 37.0 / 40.0) < 1e-6,
          "site cursor moves freely");

    // 7b. A wide screen shows more ground across than down, and the
    //     cursor has to reach it. 1600x900 over a 25 km rung is 36 px/km,
    //     so the view is 44.4 km wide; the far right is 22.2 km out,
    //     which the old square clamp cut back to 12.5.
    SurveyViewport wide = { 0.0f, 0.0f, 1600.0f, 900.0f };
    SurveyCursor w = MakeSurveyCursor(SURVEY_LEVEL_COUNT - 1, 0.0, 0.0);
    w.reachAcrossKm = 25.0 * (1600.0 / 900.0);
    w.reachDownKm = 25.0;
    SurveyCursorTrack(&w, wide, 1599.0f, 450.0f);
    double reachKm = (25.0 * 1600.0 / 900.0 - SURVEY_BUILD_FOOTPRINT_KM) * 0.5;
    printf("   wide screen: cursor reaches %.1f km east of centre "
           "(square clamp allowed %.1f)\n", w.offsetXKm,
           (25.0 - SURVEY_BUILD_FOOTPRINT_KM) * 0.5);
    Check(w.offsetXKm > 20.0 && w.offsetXKm <= reachKm + 1e-6,
          "cursor reaches the wide screen's right edge");
    SurveyCursorTrack(&w, wide, 800.0f, 1.0f);
    Check(std::fabs(w.offsetYKm - (25.0 - SURVEY_BUILD_FOOTPRINT_KM) * 0.5)
          < 0.5, "but is still held to the window top to bottom");

    // 8. Cursor centre -> lat/lon, and back through the child window.
    SurveyCursor c3 = MakeSurveyCursor(SURVEY_LEVEL_COUNT - 1, 32.8, -15.6);
    c3.offsetXKm = 5.0;
    c3.offsetYKm = -5.0;
    double lat = 0.0, lon = 0.0;
    SurveyCursorLatLon(c3, &lat, &lon);
    double expectLat = 32.8 - 5.0 / 30.32268;
    double expectLon = -15.6 + 5.0 / (30.32268 * std::cos(32.8 * M_PI / 180.0));
    Check(std::fabs(lat - expectLat) < 1e-9 && std::fabs(lon - expectLon) < 1e-9,
          "cursor centre -> real lat/lon");

    // 8b. ...and straight back out again.
    SurveyCursor c3b = MakeSurveyCursor(SURVEY_LEVEL_COUNT - 1, 32.8, -15.6);
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

    printf("\n%s (%d failures)\n", failures ? "FAILED" : "ALL PASS", failures);
    return failures ? 1 : 0;
}
