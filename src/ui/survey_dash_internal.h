/* survey_dash_internal.h -- what the console's own files share, and nothing
 * outside src/ui/survey_dash*.c and dash_overlay.c should include.
 *
 * survey_dash.c keeps the console's state, its phases and its input.
 * dash_overlay.c draws everything that floats over the panes -- the cursor
 * tag, the drill cursor and the grab hand, the core barrels and their card,
 * the block's height log -- and places all of it by one rule (DashPlace). */
#ifndef SURVEY_DASH_INTERNAL_H
#define SURVEY_DASH_INTERNAL_H

#include "survey_dash.h"
#include "c2d.h"

#ifndef PI
#define PI 3.14159265358979323846f
#endif
#define RGBA8(r,g,b,a) ((Color){(r),(g),(b),(unsigned char)((a)*255.0f)})

/* THREE PANES, which is a layout decision and not the reference's. The JS
 * dashboard has four blocks -- rack + tool stats down the left, block + log in
 * the middle, drill bar + drill stats down the right. This console is three:
 *
 *      left    the tool rack
 *      middle  the block, with the message log beneath it
 *      right   the drill bar
 *
 * The two stats blocks have no pane, so they are not drawn. Placement inside
 * each pane still comes from the reference (rack scale, block camera), so the
 * modules themselves are untouched -- only where the console puts them.
 *
 * 1536 = 20 + 350 + 16 + 744 + 16 + 370 + 20. */
#define PANE_M   20.0f
#define PANE_G   16.0f
#define PANE_TOP 20.0f
#define PANE_H   (SURVEY_DASH_DESIGN_H - PANE_TOP * 2.0f)

#define LEFT_X    PANE_M
#define LEFT_W    350.0f
#define MID_X     (LEFT_X + LEFT_W + PANE_G)
#define MID_W     744.0f
#define RIGHT_X   (MID_X + MID_W + PANE_G)
#define RIGHT_W   370.0f

/* THE SIDE COLUMNS ARE TWO PANELS EACH: the instrument, and its stats under
 * it. That is the reference's own arrangement (dashboard.html's layout.stats
 * and layout.dstats) and the three-pane port had dropped it for want of
 * room. The stats are what a tool or a running drill reports, so they belong
 * under the thing they describe. */
#define STATS_H   168.0f
#define STATS_GAP  12.0f
#define MAIN_H    (PANE_H - STATS_H - STATS_GAP)
#define STATS_Y   (PANE_TOP + MAIN_H + STATS_GAP)

/* THE RACK SCALES UNIFORMLY. It was 0.845 across and 0.925 down -- a squash
 * nobody chose, left over from filling a taller pane. One factor, fitted to
 * whichever of the two axes runs out first, and centred in what is left. */
#define RACK_FIT_W  (LEFT_W - 16.0f)
#define RACK_FIT_H  (MAIN_H - 30.0f)   /* the panel's bracket needs air above and below */
#define DASH_RACK_SCALE \
    ((RACK_FIT_W / TR_W) < (RACK_FIT_H / TR_H) ? (RACK_FIT_W / TR_W) : (RACK_FIT_H / TR_H))
#define DASH_RACK_SCALE_Y DASH_RACK_SCALE
#define DASH_RACK_X       (LEFT_X + (LEFT_W - TR_W * DASH_RACK_SCALE) * 0.5f)
#define DASH_RACK_Y       (PANE_TOP + (MAIN_H - TR_H * DASH_RACK_SCALE) * 0.5f)

/* the block, the confidence bar, then the log, down the middle pane */
#define CONF_H         76.0f
#define LOG_H         196.0f
#define LOG_X         (MID_X + 16.0f)
#define LOG_Y         (PANE_TOP + PANE_H - LOG_H - 16.0f)
#define CONF_Y        (LOG_Y - CONF_H - 12.0f)
#define LOG_W         (MID_W - 32.0f)
/* THE BLOCK AND ITS RULER ARE CENTRED AS ONE. The ruler stands off the
 * block's right edge, so the block sits DASH_BLOCK_DX left of the pane's
 * middle; and DASH_BLOCK_DY down, so the tallest core barrel (a hole to the
 * target, at the cap's far corner) stays inside the pane's frame. */
#define DASH_BLOCK_DX (-46.0f)
#define DASH_BLOCK_DY 18.0f
#define DASH_BLOCK_CX (MID_X + MID_W * 0.5f + DASH_BLOCK_DX)
#define DASH_BLOCK_CY (PANE_TOP + (CONF_Y - PANE_TOP) * 0.50f + DASH_BLOCK_DY)

/* The block was drawn at a fifth of its size in a pane seven hundred units
 * wide -- the reference's zoom, carried over without re-fitting it to a pane
 * that is a different shape. At 0.34 it fills the space it has, with room
 * above the cap for the core barrels.
 *
 * THE VIEW IS FIXED BUT FOR YAW. The block turns about its vertical axis
 * and nothing else: no zoom, no tilt. Every depth reading on it -- the
 * height log, the stretch, the cutaway -- is read at this one tilt, and a
 * view that moved in three ways gave three ways to lose the thread (see the
 * graveyard, console-block-zoom-and-tilt.md). */
#define DASH_BLOCK_ZOOM  0.34f
#define DASH_BLOCK_PITCH 0.42f

/* the rect inside which a drag rotates the block */
#define DASH_BLOCK_X0 (MID_X + 20.0f)
#define DASH_BLOCK_Y0 (PANE_TOP + 20.0f)
#define DASH_BLOCK_X1 (MID_X + MID_W - 20.0f)
#define DASH_BLOCK_Y1 (CONF_Y - 8.0f)

/* The tip reticle's radius, as a fraction of the block's width: about the
 * size the target mark it replaced read at, with its ticks clear of the
 * drill's point. */
#define DASH_TIP_RETICLE_R 0.10f

/* How long the model takes a hole in, with the cut still open: the beds
 * morph, the fog lifts, the delineation climbs. Then the cavity closes over
 * DASH_CLOSE_S -- slowly at first, gathering speed, shut at the end -- and
 * the whole block is back. */
#define DASH_REVEAL_S 3.0f
#define DASH_CLOSE_S  2.0f

/* Core barrels: sized to be seen. At 5.5 wide and 18-46 tall the first cut
 * was a faint dashed sliver on the bright cap, and the playtest could not
 * find it. */
#define DASH_BARREL_RW 9.0f
#define DASH_BARREL_RISE_S 0.4f        /* a new barrel rising out of its collar */

#define DASH_RULER_TICKS (DRILL_STRATA_COUNT + 1)

/* the one block geometry, shared by every console (survey_dash.c owns it) */
extern Holo3DModel *g_dashModel;

static inline float Clampf01v(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

typedef struct DashBarrel { float cx, gy, top, rw, rh; } DashBarrel;

/* ---- survey_dash.c: phases and geometry the overlays read ------------- */
bool         DashCutActive(const SurveyDashState *s);
bool         DashClosing(const SurveyDashState *s);
bool         DashInMid(Vector2 p);
bool         DashOverFace(Vector2 p);
bool         DashOverCtrl(const SurveyDashState *s, Vector2 p);   /* CANCEL / ABORT */
const char  *DashBedAt(const SurveyDashState *s, float screenY);
float        DashStretchMetres(const SurveyDashState *s, Vector2 pt);
const DashDepth *DashRuler(void);
void         DashCutPoint(const SurveyDashState *s, float *u, float *v);

/* ---- dash_overlay.c ---------------------------------------------------- */

/* Where a floating thing may not go this frame: the rulers. Cleared at the
 * top of SurveyDash_Draw, added to as each ruler is drawn. */
void      DashKeepOut_Clear(void);
void      DashKeepOut_Add(Rectangle r);

/* THE PLACEMENT RULE, for everything that floats. A w x h box beside an
 * anchor: to its right (gap from it), else to its left, whichever overlaps
 * no keep-out and stays in the region; then clamped into the region. y is
 * the caller's. CENTRED instead centres it on the anchor and clamps. */
typedef enum { DASH_PLACE_SIDE, DASH_PLACE_CENTRED } DashPlaceMode;
Vector2   DashPlace(Vector2 anchor, float y, float w, float h, float gap,
                    Rectangle region, DashPlaceMode mode);

void      DashDrawHeightLog(void);
DashBarrel DashBarrelOf(const DrillCoreLog *l);
bool      DashBarrelShown(const SurveyDashState *s, int i);
int       DashBarrelAt(const SurveyDashState *s, Vector2 p);
void      DashDrawBarrels(const SurveyDashState *s);
void      DashDrawCoreCard(const SurveyDashState *s, int i);
bool      DashDragging(const SurveyDashState *s);
bool      DashWantsGrab(const SurveyDashState *s);
void      DashDrawCursor(const SurveyDashState *s);

#endif
