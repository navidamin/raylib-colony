/* survey_dash.h — the survey console, assembled from the ported modules.
 *
 * Design space is 1536x768, letterboxed onto whatever region the game gives
 * it by c2d_present_into. Placement and camera come from js/dashboard.html's
 * own DASH.layout, so the modules are laid out exactly where the reference
 * puts them.
 *
 * THE CONSOLE OWNS NO STATE. Everything that changes lives in a
 * SurveyDashState the caller holds, so one console can be drawn for each
 * prospecting module rather than there being one global drill. The only
 * things that stay process-wide are the GPU resources -- the render surface,
 * the fonts and the block's geometry -- which every console shares and which
 * SurveyDash_Init/Shutdown own.
 */
#ifndef SURVEY_DASH_H_INCLUDED
#define SURVEY_DASH_H_INCLUDED

#include "raylib.h"
#include <stdbool.h>

#include "dash_chrome.h"
#include "dash_knowledge.h"
#include "drill_sim.h"
#include "holo3d.h"
#include "toolrack.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 1536x768, not the reference's 1536x1024.
 *
 * The MODULES keep their own design units -- ToolRack and Holo3D are placed
 * and scaled exactly as dashboard.html places them, which is what the spec
 * fixes. What changed is the console's canvas, because this console is three
 * panes wide rather than the reference's four stacked blocks, and because the
 * game gives it a wide, short region. At 1536x1024 the fit-contain left it
 * 750px wide inside a 1280px region and everything was half-legible; 2:1
 * fills it. */
#define SURVEY_DASH_DESIGN_W 1536
#define SURVEY_DASH_DESIGN_H 768

#define SURVEY_DASH_LOG_MAX 6

/* The log is written by what happens, not by a table. The strings are stored
 * inline so the whole console state is one flat, copyable object; `entry` is
 * the view Dash_Log wants and is rebuilt from them on every draw rather than
 * on push, because it holds pointers INTO this struct and a copy would
 * otherwise carry pointers back into the original. */
typedef struct SurveyDashLog {
    char         time[SURVEY_DASH_LOG_MAX][8];
    char         text[SURVEY_DASH_LOG_MAX][96];
    char         tag [SURVEY_DASH_LOG_MAX][24];
    int          count;
    DashLogEntry entry[SURVEY_DASH_LOG_MAX];
} SurveyDashLog;

typedef struct SurveyDashState {
    /* Zero-initialised is a valid, un-started console: Draw resets it on the
     * first frame, so a caller can just embed one and never call Reset. */
    bool          started;

    DrillSim      drill;
    DashKnowledge know;
    H3DState      block;
    H3DView       view;
    ToolRackData  rack;

    /* The drill site, in lattice coordinates across the block. Tapping the
     * cap moves it; the hole that lands is credited there, which is what
     * makes spreading holes out worth doing. */
    float         siteI, siteJ;
    bool          saidMeasured;

    SurveyDashLog log;

    /* Drag state. `moved` distinguishes a rotate from a tap, the same way the
     * JS controller does -- without it one drag suppresses the next tap. */
    bool          down, moved, onBlock;
    Vector2       downPt;
} SurveyDashState;

/* Process-wide GPU resources. Idempotent; safe to call every frame. Returns
 * false if they could not be created, in which case Draw is a no-op. */
bool SurveyDash_Init(void);
void SurveyDash_Shutdown(void);

/* A fresh console: no holes, no knowledge, the block square on. */
void SurveyDash_Reset(SurveyDashState *s);

/* Draws into the design surface and blits it, letterboxed, into `region`. */
void SurveyDash_Draw(SurveyDashState *s, Rectangle region, float dt);

/* Screen -> design space, honouring the same letterbox Draw used. */
Vector2 SurveyDash_ToDesign(Rectangle region, Vector2 screenPt);

/* Input, in SCREEN coordinates -- the conversion happens inside, because
 * hit-testing in screen space is the mistake the spec calls out by name. */
void SurveyDash_Press  (SurveyDashState *s, Rectangle region, Vector2 screenPt);
void SurveyDash_Drag   (SurveyDashState *s, Rectangle region, Vector2 delta);
void SurveyDash_Release(SurveyDashState *s, Rectangle region, Vector2 screenPt);

#ifdef __cplusplus
}
#endif

#endif /* SURVEY_DASH_H_INCLUDED */
