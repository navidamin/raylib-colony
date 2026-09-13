/* dash_chrome.h — the Dashboard's own drawing, ported from
 * js/dashboard.html 1290-1670 against docs/CANVAS2D_PORT_SPEC.md.
 *
 * Everything here draws through c2d.h. This is the subset the three-pane
 * console needs: the bracket panel, the message log and the drill bar, plus
 * the primitives they share. The layers blurb, the tool-stats block and the
 * drill-stats block are not ported -- the three-pane layout has no place for
 * them.
 *
 * ONE KNOWN DEVIATION: the JS sets several strings in a sans stack
 * ("Inter", system-ui, ...). Only JetBrains Mono is shipped, so those draw
 * mono. Recorded rather than silently approximated; shipping Inter would
 * close it.
 */
#ifndef DASH_CHROME_H
#define DASH_CHROME_H

#include "c2d.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Dashboard palette C (dashboard.html:1299), the entries this subset uses. */
Color DashC_Bg(void);
Color DashC_Accent(void);

/* Bracket panel: dim rounded outline, bright corner brackets, gaps after the
 * legs. `fill` may be a zero-alpha colour for no fill. */
void Dash_Panel(float x, float y, float w, float h, Color fill, float r, float leg);

/* Condensed title with the cyan underline. Returns the drawn width. */
float Dash_Title(const char *str, float x, float y, float size, float underline);

/* ---- the message log (drawLog, 1506) --------------------------------- */

typedef struct DashLogPart { const char *text; bool tag; } DashLogPart;

typedef struct DashLogEntry {
    const char        *time;
    DashLogPart        parts[6];
    int                partCount;
    const char        *chips[6];      /* CHIP keys, NULL-terminated set */
    int                chipCount;
} DashLogEntry;

void Dash_Log(float x, float y, float w, float h,
              const DashLogEntry *entries, int count);

/* ---- the drill bar (drawDrillBar, 1567) ------------------------------- */

typedef struct DashDepth { const char *depth, *name; } DashDepth;

void Dash_DrillBar(float x, float y, float w, float h, const char *title,
                   const DashDepth *depths, int depthCount);

#ifdef __cplusplus
}
#endif

#endif /* DASH_CHROME_H */
