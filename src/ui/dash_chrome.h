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

/* A graduation on the borehole ruler. `m` is what places it -- the ticks sit
 * at their true depth, because the strata bands beside them always did and an
 * evenly-spaced ruler only agreed with them while the depths happened to be an
 * arithmetic sequence. `name` marks the hole's own landmarks (the collar, the
 * target); the ROCK is named inside its band, where the rock is. */
typedef struct DashDepth { float m; const char *depth, *name; } DashDepth;

/* Live now, and driven by DrillSim -- see drill_sim.h. The strata, the string
 * position, the heat glow on the steel, the chip stream, the motor-pod band
 * lamp and the two gauges all read the simulation, so clicking the pane
 * drives the spindle and the bit temperature follows it.
 *
 * `sim` may be NULL, in which case the bar draws its static pose. */
struct DrillSim;
void Dash_DrillBar(float x, float y, float w, float h, const char *title,
                   const DashDepth *depths, int depthCount,
                   const struct DrillSim *sim, float dt);

/* The rect inside the bar that responds to a click (the hole and the
 * string). Design-space, relative to the same x/y passed to Dash_DrillBar. */
void Dash_DrillBarFace(float x, float y, float w, float h,
                       float *fx, float *fy, float *fw, float *fh);

/* Where the depth ruler runs, so a caller can point at it. Design-space,
 * same x/y/w/h as Dash_DrillBar. */
void Dash_DrillBarSpan(float x, float y, float w, float h,
                       float *rx, float *ry0, float *ry1);

/* C6: the ruler is the depth control. Returns the depth in metres a click at
 * `py` selects, or -1 if the click is not on the ruler. `armed` draws it lit,
 * which is what tells the player it has become a control. */
float Dash_DrillBarPickDepth(float x, float y, float w, float h, float px, float py);

/* ---- the two stats blocks (drawToolStats 1376, drawDrillStats 1577) ----
 *
 * Both were skipped when the console went to three panes: they had no pane.
 * They have one now, under the instrument each describes.
 *
 * TOOL STATS reports the SELECTED tool -- "NO TOOL" when nothing is picked,
 * which is also the state that refuses a drill site. `power`/`time`/`crew`
 * are 0..8 segment counts. */
void Dash_ToolStats(float x, float y, float w, float h,
                    const char *name, const char *type,
                    int power, int time, int crew);

/* DRILL STATS reads the simulation, so the bars move while the bit turns:
 * rotary speed, load, temperature, bit wear and vibration. `sim` may be NULL
 * for the idle pose. `status` is the line in the button at the foot. */
void Dash_DrillStats(float x, float y, float w, float h,
                     const struct DrillSim *sim, const char *status);

/* C5: delineation, its tier, and the gate isolate waits on. */
void Dash_Confidence(float x, float y, float w, float h,
                     float delineation, const char *tier, bool measured);

#ifdef __cplusplus
}
#endif

#endif /* DASH_CHROME_H */
