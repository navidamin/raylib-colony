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

/* Finished holes kept with their readings (1.4 KB each). The knowledge
 * model saturates well before this; past it the oldest log goes. */
#define SURVEY_DASH_CORES_MAX 32

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

/* ---- what the game tells the console ---------------------------------
 *
 * Refilled every frame before Draw. A state that has never been fed keeps
 * the reference's own rack, which is what the preview and visdiff harnesses
 * run on -- they have no game behind them.
 */
#define SURVEY_DASH_TOOLS_MAX TR_SLOTS_MAX

typedef struct SurveyDashTool {
    const char *name;    /* the game's own name: "DRILL", "SURFACE SWEEP" */
    const char *kind;    /* "Point" / "Line" / "Area"                     */
    TRIcon      icon;    /* chosen at the boundary, where tool identity is */
    bool        built;   /* does the game actually RUN this one yet       */
    bool        isDrill; /* the one tool that sites a hole               */
    /* What TOOL STATS reports, 0..8 segments each. The game has no
     * per-tool cost model yet -- SurveyToolInfo carries a name, a blurb, a
     * mark and `built` -- so these arrive as placeholders from the boundary
     * and become real the day something defines them. */
    int         power, time, crew;
} SurveyDashTool;

typedef struct SurveyDashFeed {
    int            tier;            /* module tier 0..3                    */
    int            toolCount;
    SurveyDashTool tool[SURVEY_DASH_TOOLS_MAX];
    int            selectedTool;    /* index into tool[], or -1            */

    /* THE GROUND. `groundAt` samples the real block; NULL leaves the
     * reference's own five beds in place, which is what a harness with no
     * game behind it gets. Resampled only when `groundRevision` changes --
     * the ground moves when a hole lands, not when a frame does. */
    int            bedCount;
    H3DDepthFn     groundAt;
    void          *groundCtx;
    int            groundRevision;
    H3DBedText     bedText[H3D_LAYERS];

    /* The one knowledge model. The console writes finished holes into it and
     * the ground is regenerated from it, so the block and the drill cannot be
     * looking at different holes. NULL keeps the console's own. */
    DashKnowledge *knowledge;
} SurveyDashFeed;

typedef struct SurveyDashState {
    /* Zero-initialised is a valid, un-started console: Draw resets it on the
     * first frame, so a caller can just embed one and never call Reset. */
    bool          started;

    DrillSim      drill;

    /* The model this console writes holes into. It is `own` unless the game
     * has handed over its own -- see SurveyDashFeed::knowledge. Resolved on
     * every draw, so a copied state repairs itself the way the log does. */
    DashKnowledge  own;
    DashKnowledge *know;
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

    /* The bay the player last picked, or -1. The console does not change the
     * selection itself: it reports the pick, the game sets it, and the next
     * feed brings it back. One owner, and the rack cannot drift from
     * SurveyConsole::SelectedTool. */
    int           toolPick;

    /* Where the pointer is on the ground, 0..1 across the block, and whether
     * it is on the ground at all. Only meaningful while the drill is the
     * selected tool -- that is the gate. */
    bool          aimArmed;      /* the selected tool sites holes        */
    bool          aimOn;         /* the pointer is over the cap          */
    float         aimU, aimV;
    bool          sited;         /* a site has been committed            */
    float         siteU, siteV;  /* it, in the same 0..1 cap coordinates */

    /* THE STRETCH. Choosing a site does not finish anything -- the hole
     * still needs a depth -- so once a site is taken a dashed borehole runs
     * down from it to the depth the pointer is at, and the next tap commits
     * that depth. While `sited && !depthPicked` the console is stretching. */
    bool          depthPicked;

    /* The hole as the bit felt it, every 0.5 m. Opened when the depth is
     * committed, filled while the string runs. */
    DrillProfile  profile;

    /* EVERY FINISHED HOLE, KEPT: its site, its depth and its readings a bin
     * at a time. Each stands on the block as a turning core barrel, and its
     * log opens from the barrel. `coreOpen` is the log belonging to the hole
     * at the current site (deepening it updates the log rather than adding
     * one), -1 until that hole first lands. */
    DrillCoreLog  cores[SURVEY_DASH_CORES_MAX];
    int           coreCount;
    int           coreOpen;
    int           coreHover;     /* barrel under the pointer, or -1      */
    int           corePinned;    /* log held open by a click, or -1      */

    /* THE REVEAL. For DASH_REVEAL_S after a hole lands or is aborted the cut
     * stays open while the model takes the hole in: the beds morph to their
     * re-fit, the fog lifts from how it stood before the hole (`fogFrom`, a
     * copy of the knowledge taken just before it was added), and the
     * delineation climbs with it. Then the block closes. < 0 when idle. */
    float         revealT;
    DashKnowledge fogFrom;

    bool          overBody;      /* over the block's sides: a grab       */
    Vector2       pointer;       /* design space, last known            */
    bool          pointerIn;     /* inside the console at all           */

    /* mirrored from the feed, in rack slot order, for TOOL STATS */
    int           toolPower[SURVEY_DASH_TOOLS_MAX];
    int           toolTime [SURVEY_DASH_TOOLS_MAX];
    int           toolCrew [SURVEY_DASH_TOOLS_MAX];
} SurveyDashState;

/* Process-wide GPU resources. Idempotent; safe to call every frame. Returns
 * false if they could not be created, in which case Draw is a no-op. */
bool SurveyDash_Init(void);
void SurveyDash_Shutdown(void);

/* A fresh console: no holes, no knowledge, the block square on. */
void SurveyDash_Reset(SurveyDashState *s);

/* Game -> console. Safe to skip entirely; the console keeps what it was last
 * told. */
void SurveyDash_Feed(SurveyDashState *s, const SurveyDashFeed *feed);

/* Console -> game: the bay the player picked since the last read, or -1.
 * Reading it clears it. */
int  SurveyDash_TakeToolPick(SurveyDashState *s);

/* Draws into the design surface and blits it, letterboxed, into `region`. */
void SurveyDash_Draw(SurveyDashState *s, Rectangle region, float dt);

/* Screen -> design space, honouring the same letterbox Draw used. */
Vector2 SurveyDash_ToDesign(Rectangle region, Vector2 screenPt);

/* Input, in SCREEN coordinates -- the conversion happens inside, because
 * hit-testing in screen space is the mistake the spec calls out by name. */
void SurveyDash_Press  (SurveyDashState *s, Rectangle region, Vector2 screenPt);

/* Where the pointer is, every frame. There is no hover on a touch screen, but
 * the web shell publishes the last touch and never clears it, so a tap
 * behaves as a hover that stays (docs/web-deploy-mobile.md). */
void SurveyDash_Hover  (SurveyDashState *s, Rectangle region, Vector2 screenPt);

/* WHERE THE HOLE IS in its life. Derived from the state every time it is
 * asked, never stored, so it cannot disagree with the drill:
 *
 *   AIM       the drill is in hand and no site is taken
 *   STRETCH   a site is taken; the pointer's height is choosing a depth
 *   PLANNED   a depth is committed; the drill bar waits to be started
 *   DRILLING  the string is running
 *   COMPLETE  the bit is at the planned depth -- site again, or deepen */
typedef enum SurveyDashPhase {
    SDP_AIM = 0,
    SDP_STRETCH,
    SDP_PLANNED,
    SDP_DRILLING,
    SDP_COMPLETE
} SurveyDashPhase;

SurveyDashPhase SurveyDash_Phase(const SurveyDashState *s);

/* The pointer the console wants, for the caller to apply: the console draws
 * the drill itself (HIDDEN), wants the ordinary arrow, or wants the hand that
 * says "this starts something". */
typedef enum SurveyDashCursor {
    SDC_ARROW = 0,
    SDC_HIDDEN,
    SDC_HAND
} SurveyDashCursor;

SurveyDashCursor SurveyDash_Cursor(const SurveyDashState *s);

/* A drag on the block turns it about its vertical axis -- yaw only. */
void SurveyDash_Drag   (SurveyDashState *s, Rectangle region, Vector2 delta);
void SurveyDash_Release(SurveyDashState *s, Rectangle region, Vector2 screenPt);

/* The secondary button (right-click): undo the last choice. A depth goes
 * back to choosing a depth, a site back to aiming. A running drill is not
 * undone by it -- that is ABORT, on the drill bar. */
void SurveyDash_Cancel (SurveyDashState *s);

/* A HARNESS HOLE: the real drill, run to `depthM` at (u, v) in one call --
 * the same DrillSim, profile, core log and knowledge update a played hole
 * goes through, with the player's rhythm stood in by a steady tap. For
 * previews and tests that need drilled ground without minutes of play.
 * `reveal` plays the model taking the hole in, as a played hole does. */
void SurveyDash_DrillNow(SurveyDashState *s, float u, float v, float depthM, bool reveal);

/* HARNESS: slow the reveal and the close down by this factor (1 = real
 * time), so an animation can be captured on a renderer that draws a frame a
 * second. Process-wide. */
void SurveyDash_SetRevealSlowmo(float factor);

#ifdef __cplusplus
}
#endif

#endif /* SURVEY_DASH_H_INCLUDED */
