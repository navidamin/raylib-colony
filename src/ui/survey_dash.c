/* survey_dash.c — see survey_dash.h. */
#include "survey_dash.h"

#include "c2d.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

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
#define DASH_BLOCK_CX (MID_X + MID_W * 0.5f)
#define DASH_BLOCK_CY (PANE_TOP + (CONF_Y - PANE_TOP) * 0.50f)

/* The block was drawn at a fifth of its size in a pane seven hundred units
 * wide -- the reference's zoom, carried over without re-fitting it to a pane
 * that is a different shape. At 0.42 it fills the space it has. The player
 * can move it: see DASH_ZOOM_MIN/MAX. */
#define DASH_BLOCK_ZOOM 0.37f
#define DASH_ZOOM_MIN   0.15f
#define DASH_ZOOM_MAX   1.30f
#define DASH_ZOOM_STEP  1.12f

/* the rect inside which a drag rotates the block */
#define DASH_BLOCK_X0 (MID_X + 20.0f)
#define DASH_BLOCK_Y0 (PANE_TOP + 20.0f)
#define DASH_BLOCK_X1 (MID_X + MID_W - 20.0f)
#define DASH_BLOCK_Y1 (CONF_Y - 8.0f)

/* The console ships supersampled: Canvas antialiases coverage and raylib does
 * not, and 2 is where that stops paying (docs/design/prospecting/
 * holo3d-inventory.md).
 *
 * NOT ON THE WEB. Supersampling multiplies EVERY offscreen target the shim
 * owns. At 2 a design-sized target is 3072x1536 -- 18 MB apiece -- and seven
 * of them are design-sized: the surface, four clip layers and Holo3D's two
 * ghost buffers. That is 126 MB, plus a depth renderbuffer on each and the
 * downsampled blur pairs on top. A browser tab refuses somewhere in there,
 * and raylib answers a refused LoadRenderTexture with id 0. Binding id 0 is
 * binding the CANVAS -- at the viewport the texture asked for. That is
 * exactly "drawing to a destination rect smaller than the viewport rect",
 * and why the page came up broken with no error in it.
 *
 * At 1 the same seven come to 31 MB. The visual diff gate is measured at 2
 * and is a desktop and CI concern; the phone gets a console instead of a
 * blank screen, which is the better trade. */
#if defined(PLATFORM_WEB) || defined(__EMSCRIPTEN__)
  #define DASH_SS 1
#else
  #define DASH_SS 2
#endif

/* THE ONLY FILE STATICS LEFT, and they are the process's, not a console's:
 * one render surface, one set of fonts, one block geometry, shared by every
 * console that draws. Everything a console remembers is in SurveyDashState. */
static bool         g_resReady = false;
static C2DSurface   g_surf;
static Holo3DModel *g_model = NULL;

/* What ground the ONE model is currently carrying. It is the model's
 * property, not a console's: two prospecting units have different ground and
 * whichever is on screen has to re-apply its own. */
static void *g_groundCtx = NULL;
static int   g_groundRev = -1;

static float Clampf01v(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

/* ---- the log ---------------------------------------------------------- */

static void DashLog_Push(SurveyDashState *s, float t, const char *text, const char *tag)
{
    SurveyDashLog *L = &s->log;
    for (int i = (L->count < SURVEY_DASH_LOG_MAX ? L->count : SURVEY_DASH_LOG_MAX - 1); i > 0; i--)
    {
        memcpy(L->time[i], L->time[i - 1], sizeof(L->time[0]));
        memcpy(L->text[i], L->text[i - 1], sizeof(L->text[0]));
        memcpy(L->tag [i], L->tag [i - 1], sizeof(L->tag [0]));
    }
    snprintf(L->time[0], sizeof(L->time[0]), "%02d:%02d",
             (int)(t / 60.0f) % 100, (int)t % 60);
    snprintf(L->text[0], sizeof(L->text[0]), "%s", text);
    snprintf(L->tag [0], sizeof(L->tag [0]), "%s", tag ? tag : "");
    if (L->count < SURVEY_DASH_LOG_MAX) L->count++;
}

/* Point the entries at this struct's own strings. Done on the way to the
 * draw, never on push, so a copied state is repaired rather than left with
 * pointers into the state it was copied from. */
static void DashLog_Bind(SurveyDashLog *L)
{
    for (int i = 0; i < L->count; i++)
    {
        L->entry[i].time = L->time[i];
        L->entry[i].parts[0] = (DashLogPart){L->text[i], false};
        L->entry[i].partCount = 1;
        if (L->tag[i][0])
        {
            L->entry[i].parts[1] = (DashLogPart){L->tag[i], true};
            L->entry[i].partCount = 2;
        }
        L->entry[i].chipCount = 0;
    }
}

/* The ruler's graduations, from the ground itself rather than from a table:
 * the collar, then the base of each stratum, then the bottom of the column.
 * Those bases ARE the game's four depth layers (LAYER_THICKNESS_M), so the
 * ruler, the bands beside it, the energy a hole costs and the layer it cores
 * are finally all reading the same numbers.
 *
 * The strata are named inside their own bands; what the ruler marks is the
 * HOLE's landmarks. */
#define DASH_RULER_TICKS (DRILL_STRATA_COUNT + 1)

static const DashDepth *SurveyDash_Ruler(void)
{
    static DashDepth d[DASH_RULER_TICKS];
    static char      label[DASH_RULER_TICKS][12];
    static bool      built = false;
    if (!built)
    {
        const DrillStratum *S = DrillSim_Strata();
        snprintf(label[0], sizeof(label[0]), "%d m", (int)(S[0].top + 0.5f));
        d[0] = (DashDepth){S[0].top, label[0], "SURFACE"};
        for (int i = 0; i < DRILL_STRATA_COUNT; i++)
        {
            snprintf(label[i + 1], sizeof(label[i + 1]), "%d m", (int)(S[i].bot + 0.5f));
            d[i + 1] = (DashDepth){S[i].bot, label[i + 1],
                                   (i == DRILL_STRATA_COUNT - 1) ? "TARGET" : NULL};
        }
        built = true;
    }
    return d;
}

/* ---- the feed ---------------------------------------------------------
 *
 * An UNBUILT tool is an EMPTY BAY, not a dim one. The rack already draws a
 * null entry as an empty machined socket -- the JS data has one -- and "a bay
 * you have no tool for yet" is exactly what that reads as. The alternative,
 * drawing it present but inactive, collides with the state a BUILT tool sits
 * in when it is simply not running, and the player would have no way to tell
 * a tool they can use from one they cannot.
 *
 * What it costs: the rack no longer names what is coming. Nothing else in the
 * game advertises unbuilt tools either, so inventing that here would be
 * designing rather than integrating. Recorded in
 * docs/design/prospecting/console-real-data.md. */
void SurveyDash_Feed(SurveyDashState *s, const SurveyDashFeed *feed)
{
    if (!s || !feed) return;
    if (!s->started) SurveyDash_Reset(s);

    ToolRackData *r = &s->rack;
    r->header = "SURVEY TOOLS";
    r->slots = feed->toolCount > TR_SLOTS_MAX ? TR_SLOTS_MAX : feed->toolCount;

    for (int i = 0; i < TR_SLOTS_MAX; i++)
    {
        ToolRackTool *t = &r->tools[i];
        memset(t, 0, sizeof(*t));
        t->fade = -1.0f;                 /* no transition in flight */
        if (i >= r->slots) continue;

        const SurveyDashTool *g = &feed->tool[i];
        if (!g->built) continue;         /* the empty socket */

        t->present  = true;
        t->name     = g->name;
        t->type     = g->kind;
        t->icon     = g->icon;
        t->active   = (i == feed->selectedTool);
        t->selected = t->active;
        if (t->active) s->aimArmed = g->isDrill;
        s->toolPower[i] = g->power;
        s->toolTime[i]  = g->time;
        s->toolCrew[i]  = g->crew;
    }

    /* nothing selected is not armed */
    {
        bool any = false;
        for (int i = 0; i < r->slots; i++) if (r->tools[i].present && r->tools[i].active) any = true;
        if (!any) s->aimArmed = false;
    }

    /* One model for the block and the drill. Repointed every frame, so a
     * copied state stops pointing into the state it was copied from. */
    s->know = feed->knowledge ? feed->knowledge : &s->own;

    /* The ground, only when it has actually moved. Holo3D holds one point
     * grid for the process, so the check is against what the MODEL carries,
     * not against what this console last asked for. */
    if (g_model && feed->groundAt &&
        (g_groundCtx != feed->groundCtx || g_groundRev != feed->groundRevision))
    {
        Holo3D_SetGround(g_model, feed->bedCount, feed->groundAt, feed->groundCtx,
                         feed->bedText);
        g_groundCtx = feed->groundCtx;
        g_groundRev = feed->groundRevision;
    }
}

int SurveyDash_TakeToolPick(SurveyDashState *s)
{
    if (!s) return -1;
    const int p = s->toolPick;
    s->toolPick = -1;
    return p;
}

bool SurveyDash_Init(void)
{
    if (g_resReady) return true;

    c2d_set_supersample(DASH_SS);
    c2d_fonts_load("src/assets/fonts/JetBrainsMono-Medium.ttf",
                   "src/assets/fonts/JetBrainsMono-SemiBold.ttf",
                   "src/assets/fonts/JetBrainsMono-Bold.ttf");
    g_surf = c2d_surface_create(SURVEY_DASH_DESIGN_W, SURVEY_DASH_DESIGN_H);
    if (g_surf.tex.id == 0)
    {
        TraceLog(LOG_WARNING,
                 "SURVEY CONSOLE: no %dx%d render target (supersample %d) -- "
                 "the console cannot draw",
                 SURVEY_DASH_DESIGN_W * DASH_SS, SURVEY_DASH_DESIGN_H * DASH_SS, DASH_SS);
        return false;
    }

    H3DBuildOpts opts = {0};
    opts.surfaceW = SURVEY_DASH_DESIGN_W;
    opts.surfaceH = SURVEY_DASH_DESIGN_H;
    opts.plain = true;          /* real ground: no decorative motes */
    g_model = Holo3D_Build(&opts);
    if (!g_model) { c2d_surface_destroy(&g_surf); return false; }

    g_resReady = true;
    return true;
}

void SurveyDash_Shutdown(void)
{
    if (!g_resReady) return;
    Holo3D_Free(g_model);
    g_model = NULL;
    g_groundCtx = NULL;
    g_groundRev = -1;
    c2d_surface_destroy(&g_surf);
    c2d_fonts_unload();
    g_resReady = false;
}

void SurveyDash_Reset(SurveyDashState *s)
{
    if (!s) return;
    memset(s, 0, sizeof(*s));

    s->block.yaw = -0.1f;
    s->block.pitch = 0.42f;
    s->block.selected = -1;

    s->view.cx = DASH_BLOCK_CX;
    s->view.cy = DASH_BLOCK_CY;
    s->view.zoom = DASH_BLOCK_ZOOM;

    s->rack = ToolRack_Demo();
    DrillSim_Reset(&s->drill);
    DrillProfile_Clear(&s->profile);
    DashKnow_Clear(&s->own);

    /* NOTHING IS SITED until the player sites it. The console used to open
     * with a collar in the middle of the block, which read as a decision
     * already taken and left a large mark sitting idle on the ground. */
    s->sited = false;
    s->siteI = DK_LATTICE * 0.5f;
    s->siteJ = DK_LATTICE * 0.5f;
    s->toolPick = -1;
    s->know = &s->own;

    DashLog_Push(s, 0.0f, "Console online. Select the DRILL, then tap the block to site a hole.", NULL);
    s->started = true;
}

/* ---- the target mark ---------------------------------------------------
 *
 * Four ticks around a ring, turning slowly, with a dot at the centre. It is
 * the same mark wherever it appears -- riding the bit's tip while you aim,
 * left on the ground once the site is taken, and carried by the cursor on
 * its way to the ruler -- because they are all the same statement: THIS
 * SPOT. Only the size changes. */
static void DashTargetMarkShapes(Vector2 p, float r, float spin, Color c, float lw)
{
    Vector2 ring[25];
    for (int i = 0; i <= 24; i++)
    {
        const float a = (float)(i % 24) / 24.0f * 2.0f * PI;
        ring[i] = (Vector2){p.x + cosf(a) * r, p.y + sinf(a) * r};
    }
    c2d_polyline(ring, 25, c, lw);

    for (int k = 0; k < 4; k++)
    {
        const float a = spin + (float)k * PI * 0.5f;
        const Vector2 t[2] = {
            {p.x + cosf(a) * r * 1.15f, p.y + sinf(a) * r * 1.15f},
            {p.x + cosf(a) * r * 1.75f, p.y + sinf(a) * r * 1.75f}};
        c2d_polyline(t, 2, c, lw);
    }
    c2d_disc(p, r * 0.22f, c);
}

/* The cap is busy -- imagery, grid, grain -- so the mark carries its own
 * glow. One shadow layer for the whole mark, not a halo per shape: the ring,
 * the four ticks and the dot overlap, and stacking their halos source-over
 * fills the gaps between them (CLAUDE.md, the two ways to draw a shadowBlur). */
static void DashTargetMark(Vector2 p, float r, float spin, Color c, float lw)
{
    c2d_shadow_begin();
    DashTargetMarkShapes(p, r, spin, c, lw);
    c2d_shadow_end(c, 7.0f);
    DashTargetMarkShapes(p, r, spin, c, lw);
}

/* ---- THE HEIGHT LOG --------------------------------------------------
 *
 * The same ruler the drill bar carries, stood beside the block, so the two
 * read as one scale: 12 m on the block IS 12 m in the borehole.
 *
 * It rides the block's RIGHTMOST vertical edge. The camera has no world-y
 * term in screen x, so every edge of the block is a vertical line on screen
 * at every yaw and pitch, and depth along it is linear -- a flat ruler laid
 * against it is exact, not an approximation, and it follows rotation and
 * zoom for free. Rightmost, because the ruler labels to its right and the
 * pane has room there; the leftmost edge would put the labels on the block. */
static void DashDrawHeightLog(void)
{
    if (!g_model) return;
    static const float cu[4] = {0.0f, 1.0f, 1.0f, 0.0f};
    static const float cv[4] = {0.0f, 0.0f, 1.0f, 1.0f};
    int best = 0;
    float bestX = -1e9f;
    for (int c = 0; c < 4; c++)
    {
        const Vector2 p = Holo3D_ColumnPoint(g_model, cu[c], cv[c], 0.0f);
        if (p.x > bestX) { bestX = p.x; best = c; }
    }
    const Vector2 top = Holo3D_ColumnPoint(g_model, cu[best], cv[best], 0.0f);
    const Vector2 bot = Holo3D_ColumnPoint(g_model, cu[best], cv[best], 1.0f);

    /* clear of the edge, and never past the pane's own margin */
    float x = top.x + 22.0f;
    if (x > MID_X + MID_W - 96.0f) x = MID_X + MID_W - 96.0f;
    /* Depths only. SURFACE and TARGET are the HOLE's landmarks and belong on
     * the drill bar; on the block the cap is plainly the surface, and at this
     * scale SURFACE lands on the 12 m label. */
    DashDepth plain[DASH_RULER_TICKS];
    const DashDepth *src = SurveyDash_Ruler();
    for (int i = 0; i < DASH_RULER_TICKS; i++)
        plain[i] = (DashDepth){src[i].m, src[i].depth, NULL};
    Dash_DepthRuler(x, top.y, bot.y, plain, DASH_RULER_TICKS);
}

/* THE SITE, ON THE GROUND. Left where the hole was collared, projected onto
 * the cap so it rides the block's rotation and zoom. */
static void DashDrawSite(const SurveyDashState *s)
{
    if (!g_model || !s->sited) return;
    const Vector2 p = Holo3D_CapPoint(g_model, s->siteU, s->siteV);
    const float pulse = 0.85f + 0.15f * sinf(s->drill.t * 3.0f);
    DashTargetMark(p, 7.0f * pulse, s->drill.t * 0.6f, RGBA8(0xff, 0xc8, 0x4d, 0.95f), 1.6f);
}

/* ---- THE STRETCH -------------------------------------------------------
 *
 * How deep a hole goes is chosen ON THE BLOCK, where the hole is. The ruler
 * in the drill bar still sets a depth too, but it is a remote control for a
 * spatial choice; the block is the thing itself.
 *
 * Depth runs down the vertical under the site -- the one line the camera
 * keeps vertical on screen at every yaw and pitch -- from the collar (0) to
 * the base of the column (1), and the pointer's HEIGHT picks along it. The
 * borehole stays vertical when the pointer drifts sideways, because the
 * drill goes straight down; the label follows the pointer. */
static bool DashStretching(const SurveyDashState *s)
{
    return s->sited && !s->depthPicked;
}

static float DashStretch01(const SurveyDashState *s, Vector2 pt)
{
    if (!g_model) return 0.0f;
    const Vector2 top = Holo3D_CapPoint(g_model, s->siteU, s->siteV);
    const Vector2 bot = Holo3D_ColumnPoint(g_model, s->siteU, s->siteV, 1.0f);
    const float span = bot.y - top.y;
    if (span <= 1.0f) return 0.0f;
    const float f = (pt.y - top.y) / span;
    return f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f);
}

/* A hole shallower than this is a scratch, and a target at the collar would
 * finish the instant it began. */
#define DASH_MIN_HOLE_M 2.0f

static float DashStretchMetres(const SurveyDashState *s, Vector2 pt)
{
    float m = roundf(DashStretch01(s, pt) * DRILL_TARGET_M);
    return m < DASH_MIN_HOLE_M ? DASH_MIN_HOLE_M : m;
}

static Vector2 DashLerp(Vector2 a, Vector2 b, float t)
{
    return (Vector2){a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

/* The borehole, on the block: stretching while a depth is being chosen,
 * then planned (dashed, quieter) with what has actually been drilled drawn
 * solid over it. */
static void DashDrawBorehole(const SurveyDashState *s)
{
    if (!g_model || !s->sited) return;
    const Vector2 top = Holo3D_CapPoint(g_model, s->siteU, s->siteV);
    const Vector2 bot = Holo3D_ColumnPoint(g_model, s->siteU, s->siteV, 1.0f);

    if (DashStretching(s))
    {
        if (!s->pointerIn) return;
        const float f = DashStretchMetres(s, s->pointer) / DRILL_TARGET_M;
        const Vector2 end = DashLerp(top, bot, f);
        const Vector2 line[2] = {top, end};
        const Color c = RGBA8(0x35, 0xd8, 0xee, 0.95f);
        c2d_dashed_polyline(line, 2, 6.0f, 4.0f, c, 1.8f);
        DashTargetMark(end, 5.0f, s->drill.t * 0.9f, c, 1.4f);
        return;
    }

    /* Planned. It was drawn at 0.55 alpha and 1.4 wide, which the letterbox
     * shrinks to two-thirds of a pixel: it existed and could not be seen. The
     * plan is what the player is about to spend a drill run on, so it gets
     * the stretch's own weight, and its bottom keeps the mark the stretch
     * ended on. */
    const float plan = (s->drill.targetM >= 0.0f ? s->drill.targetM : DRILL_TARGET_M) / DRILL_TARGET_M;
    const Vector2 end = DashLerp(top, bot, plan);
    const Vector2 line[2] = {top, end};
    const Color planCol = RGBA8(0x35, 0xd8, 0xee, 0.85f);
    c2d_dashed_polyline(line, 2, 5.0f, 4.0f, planCol, 1.8f);
    DashTargetMark(end, 4.5f, 0.0f, planCol, 1.3f);

    /* drilled -- the string's real depth, out of the same simulation that
       turns the auger in the drill bar */
    if (s->drill.depthM > 0.05f)
    {
        const Vector2 bit = DashLerp(top, bot, s->drill.depthM / DRILL_TARGET_M);
        const Vector2 hole[2] = {top, bit};
        c2d_polyline(hole, 2, RGBA8(0xff, 0xc8, 0x4d, 0.95f), 2.2f);
    }
}

/* ---- THE PHASE ------------------------------------------------------- */

SurveyDashPhase SurveyDash_Phase(const SurveyDashState *s)
{
    if (!s || !s->sited)             return SDP_AIM;
    if (!s->depthPicked)             return SDP_STRETCH;
    if (s->drill.running)            return SDP_DRILLING;
    if (s->drill.depthM >= s->drill.targetM) return SDP_COMPLETE;
    return SDP_PLANNED;
}

/* The drill bar is dead until there is a hole to drill: a site AND a depth.
 * Before that it would only be a second way to choose a depth for a hole
 * that has no site. */
static bool DashBarLive(const SurveyDashState *s)
{
    const SurveyDashPhase ph = SurveyDash_Phase(s);
    return ph != SDP_AIM && ph != SDP_STRETCH;
}

/* The drill bar's face -- the hole and the string, the part a click starts
 * and drives. The same rect the press tests. */
static bool DashOverFace(Vector2 p)
{
    float fx, fy, fw, fh;
    Dash_DrillBarFace(RIGHT_X, PANE_TOP, RIGHT_W, MAIN_H, &fx, &fy, &fw, &fh);
    return p.x >= fx && p.x <= fx + fw && p.y >= fy && p.y <= fy + fh;
}

static bool DashInMid(Vector2 p)
{
    return p.x >= MID_X && p.x <= MID_X + MID_W && p.y >= PANE_TOP && p.y <= PANE_TOP + PANE_H;
}

SurveyDashCursor SurveyDash_Cursor(const SurveyDashState *s)
{
    if (!s || !s->started || !s->pointerIn) return SDC_ARROW;
    switch (SurveyDash_Phase(s))
    {
        /* The drill is the pointer where it can site a hole -- over the
         * block's pane. Over the rack or the bar it would be pointing at
         * things it cannot collar. */
        case SDP_AIM:
        case SDP_COMPLETE:
            return (s->aimArmed && DashInMid(s->pointer)) ? SDC_HIDDEN : SDC_ARROW;
        /* the arrow is the precise thing to pick a height with */
        case SDP_STRETCH:
            return SDC_ARROW;
        /* the bar is now a button: the hand says so */
        case SDP_PLANNED:
        case SDP_DRILLING:
            return DashOverFace(s->pointer) ? SDC_HAND : SDC_ARROW;
    }
    return SDC_ARROW;
}

/* ---- THE CURSOR TAG ----------------------------------------------------
 *
 * What the next tap will do, hung off the pointer, because the pointer is
 * where the eye already is. Two lines: a small caption that names the act,
 * and the value or the verb large under it. It was one 12-unit line and at
 * the console's letterbox scale that is 8 pixels -- present, and missed. */
#define DASH_TAG_CAP_FS  12.0f
#define DASH_TAG_MAIN_FS 21.0f

static void DashDrawTag(Vector2 at, const char *caption, const char *main, Color mainCol)
{
    const float cw = c2d_measure(C2D_W500, DASH_TAG_CAP_FS, caption);
    const float mw = c2d_measure(C2D_W700, DASH_TAG_MAIN_FS, main);
    const float pw = fmaxf(cw, mw) + 24.0f, ph = 54.0f;

    /* up and to the right of the tip; flipped to the left near the right
       edge, and kept inside the surface at the top */
    float px = at.x + 20.0f, py = at.y - ph - 10.0f;
    if (px + pw > (float)SURVEY_DASH_DESIGN_W - 8.0f) px = at.x - 20.0f - pw;
    if (py < 8.0f) py = at.y + 24.0f;

    const C2DCorner cr[4] = {{px, py, 6.0f}, {px + pw, py, 6.0f},
                             {px + pw, py + ph, 6.0f}, {px, py + ph, 6.0f}};
    Vector2 plate[64];
    const int pn = c2d_rpoly_pts(cr, 4, 0.0f, 0.0f, plate, 63);
    c2d_fill_poly(plate, pn, RGBA8(0x03, 0x14, 0x20, 0.94f));
    plate[pn] = plate[0];
    c2d_polyline(plate, pn + 1, RGBA8(mainCol.r, mainCol.g, mainCol.b, 0.75f), 1.4f);
    c2d_text(C2D_W500, DASH_TAG_CAP_FS, caption, px + 12.0f, py + 19.0f,
             RGBA8(0x8f, 0xbf, 0xe6, 0.95f), C2D_ALIGN_LEFT, C2D_BASELINE_ALPHABETIC);
    c2d_text(C2D_W700, DASH_TAG_MAIN_FS, main, px + 12.0f, py + 44.0f,
             mainCol, C2D_ALIGN_LEFT, C2D_BASELINE_ALPHABETIC);
}

/* ---- THE CURSOR --------------------------------------------------------
 *
 * With the drill in hand the pointer IS the drill, tip down, and the spot it
 * would collar is marked at the tip. That is the whole reason the rack has a
 * tool in it.
 *
 * Once a site is taken the console gives the pointer back and hangs the next
 * act off it instead: the depth while one is being chosen, then START
 * DIGGING until the drill bar is started. */
static void DashDrawCursor(const SurveyDashState *s)
{
    if (!s->pointerIn) return;
    const Color cyan  = RGBA8(0x35, 0xd8, 0xee, 1.0f);
    const Color amber = RGBA8(0xff, 0xc8, 0x4d, 1.0f);

    switch (SurveyDash_Phase(s))
    {
        case SDP_STRETCH:
        {
            char m[24];
            snprintf(m, sizeof(m), "%d m", (int)DashStretchMetres(s, s->pointer));
            DashDrawTag(s->pointer, "SELECT DEPTH", m, cyan);
            return;
        }
        case SDP_PLANNED:
        {
            char cap[32];
            if (DashOverFace(s->pointer))
                snprintf(cap, sizeof(cap), "%d m HOLE", (int)(s->drill.targetM + 0.5f));
            else
                snprintf(cap, sizeof(cap), "TAP THE DRILL BAR");
            DashDrawTag(s->pointer, cap, "START DIGGING", amber);
            return;
        }
        case SDP_DRILLING:
            return;
        case SDP_AIM:
        case SDP_COMPLETE:
            break;
    }

    if (SurveyDash_Cursor(s) == SDC_HIDDEN)
    {
        Vector2 tip = s->pointer;
        if (s->aimOn && g_model) tip = Holo3D_CapPoint(g_model, s->aimU, s->aimV);
        ToolRack_DrawDrillCursor(tip.x, tip.y, 0.85f, s->aimOn);
        if (s->aimOn)
            DashTargetMark(tip, 9.0f, s->drill.t * 0.9f, cyan, 1.7f);
    }
}

/* ---- THE DRILL BAR'S STATE ---------------------------------------------
 *
 * Dimmed while there is nothing to drill, so the player's eye goes to the
 * block, where the next act is. Once a hole is planned the face breathes
 * amber until it is started: the bar has become the button. */
static void DashDrawBarState(const SurveyDashState *s)
{
    if (!DashBarLive(s))
    {
        const C2DCorner cr[4] = {{RIGHT_X, PANE_TOP, 12.0f}, {RIGHT_X + RIGHT_W, PANE_TOP, 12.0f},
                                 {RIGHT_X + RIGHT_W, PANE_TOP + MAIN_H, 12.0f},
                                 {RIGHT_X, PANE_TOP + MAIN_H, 12.0f}};
        Vector2 v[64];
        const Color bg = DashC_Bg();
        c2d_fill_poly(v, c2d_rpoly_pts(cr, 4, 0.0f, 0.0f, v, 64),
                      RGBA8(bg.r, bg.g, bg.b, 0.66f));
        return;
    }
    if (SurveyDash_Phase(s) != SDP_PLANNED) return;

    /* the face below the gauges -- Dash_DrillBar's own inset */
    float fx, fy, fw, fh;
    Dash_DrillBarFace(RIGHT_X, PANE_TOP, RIGHT_W, MAIN_H, &fx, &fy, &fw, &fh);
    fy += 44.0f; fh -= 44.0f;
    const float a = 0.55f + 0.40f * (0.5f + 0.5f * sinf(s->drill.t * 4.0f));
    const C2DCorner cr[4] = {{fx, fy, 8.0f}, {fx + fw, fy, 8.0f},
                             {fx + fw, fy + fh, 8.0f}, {fx, fy + fh, 8.0f}};
    Vector2 v[64];
    const int n = c2d_rpoly_pts(cr, 4, 0.0f, 0.0f, v, 63);
    v[n] = v[0];
    const Color c = RGBA8(0xff, 0xc8, 0x4d, a);
    c2d_shadow_begin();
    c2d_polyline(v, n + 1, c, 2.0f);
    c2d_shadow_end(c, 8.0f);
    c2d_polyline(v, n + 1, c, 2.0f);
}

void SurveyDash_Draw(SurveyDashState *s, Rectangle region, float dt)
{
    if (!s) return;
    if (!SurveyDash_Init())
    {
        /* A blank rectangle is indistinguishable from a hung game. Say what
         * happened, on the screen, where the console would have been. */
        DrawRectangleRec(region, DashC_Bg());
        const char *msg = "SURVEY CONSOLE UNAVAILABLE - no render target";
        const int tw = MeasureText(msg, 20);
        DrawText(msg, (int)(region.x + (region.width - tw) * 0.5f),
                 (int)(region.y + region.height * 0.5f - 10.0f), 20,
                 (Color){0xff, 0x8a, 0x5a, 255});
        return;
    }
    if (!s->started) SurveyDash_Reset(s);
    if (!s->know) s->know = &s->own;

    Holo3D_Tick(&s->block, dt, (float)GetTime());
    s->block.time = (float)GetTime();

    c2d_begin(&g_surf, DashC_Bg());

    Dash_Panel(LEFT_X, PANE_TOP, LEFT_W, MAIN_H,  (Color){0, 0, 0, 0}, 12.0f, 26.0f);
    Dash_Panel(LEFT_X, STATS_Y,  LEFT_W, STATS_H, (Color){0, 0, 0, 0}, 12.0f, 22.0f);
    Dash_Panel(MID_X,  PANE_TOP, MID_W,  PANE_H,  (Color){0, 0, 0, 0}, 12.0f, 26.0f);
    Dash_Panel(RIGHT_X, STATS_Y, RIGHT_W, STATS_H, (Color){0, 0, 0, 0}, 12.0f, 22.0f);

    ToolRackOpts ro = {0};
    ro.level = 6;
    ro.grain = false;      /* the grain tile is not wired to a seed yet */
    c2d_save();
    c2d_translate(DASH_RACK_X, DASH_RACK_Y);
    c2d_scale(DASH_RACK_SCALE, DASH_RACK_SCALE_Y);
    ToolRack_DrawB(&s->rack, 0.0f, 0.0f, &ro);
    c2d_restore();

    /* drawBlock3D (1611): callouts, brackets and the base ring are OFF in the
     * dashboard -- they belong to the block's own full-screen view. */
    H3DHud hud = {0};
    hud.reticle = true;
    Holo3D_Render(g_model, &s->block, &s->view);
    Holo3D_DrawHud(g_model, &s->block, &s->view, &hud);

    /* ---- the two stats blocks, under the instruments they describe ---- */
    {
        int sel = -1;
        for (int i = 0; i < s->rack.slots; i++)
            if (s->rack.tools[i].present && s->rack.tools[i].active) { sel = i; break; }
        Dash_ToolStats(LEFT_X, STATS_Y, LEFT_W, STATS_H,
                       sel >= 0 ? s->rack.tools[sel].name : NULL,
                       sel >= 0 ? s->rack.tools[sel].type : NULL,
                       sel >= 0 ? s->toolPower[sel] : 0,
                       sel >= 0 ? s->toolTime[sel]  : 0,
                       sel >= 0 ? s->toolCrew[sel]  : 0);
    }
    {
        const DrillSim *dr = &s->drill;
        const SurveyDashPhase ph = SurveyDash_Phase(s);
        const char *status = dr->tripping         ? "TRIPPING"
                           : dr->done             ? "HOLE COMPLETE"
                           : ph == SDP_DRILLING   ? "DRILLING"
                           : ph == SDP_COMPLETE   ? "AT TARGET"
                           : ph == SDP_PLANNED    ? "READY"
                           : "IDLE";
        Dash_DrillStats(RIGHT_X, STATS_Y, RIGHT_W, STATS_H, dr, status);
    }

    DashDrawHeightLog();
    DashDrawBorehole(s);
    DashDrawSite(s);

    Dash_Confidence(LOG_X, CONF_Y, LOG_W, CONF_H,
                    DashKnow_Delineation(s->know, DK_LATTICE, DRILL_TARGET_M),
                    DashKnow_Tier(s->know, DK_LATTICE, DRILL_TARGET_M),
                    DashKnow_IsMeasured(s->know, DK_LATTICE, DRILL_TARGET_M));
    DashLog_Bind(&s->log);
    Dash_Log(LOG_X, LOG_Y, LOG_W, LOG_H, s->log.entry, s->log.count);

    DrillSim_Step(&s->drill, dt);
    DrillProfile_Record(&s->profile, &s->drill);
    if (s->drill.completed)
    {
        /* C5+C6 meet here: a finished hole is what the model learns from. */
        DashKnow_Add(s->know, s->siteI, s->siteJ, s->drill.completedAtM);
        char msg[96];
        snprintf(msg, sizeof(msg), "Hole to %d m logged at site %d/%d in ",
                 (int)(s->drill.completedAtM + 0.5f), (int)s->siteI, (int)s->siteJ);
        DashLog_Push(s, s->drill.t, msg, DrillSim_At(s->drill.completedAtM)->name);
        snprintf(msg, sizeof(msg), "Profile: %d readings, one every %.1f m.",
                 s->profile.count, DRILL_PROFILE_STEP_M);
        DashLog_Push(s, s->drill.t, msg, NULL);
        /* announced ONCE -- it fires on a completion, and every later hole
         * is also a completion with the model still measured */
        if (!s->saidMeasured && DashKnow_IsMeasured(s->know, DK_LATTICE, DRILL_TARGET_M))
        {
            DashLog_Push(s, s->drill.t, "Model MEASURED. Isolate unlocked.", NULL);
            s->saidMeasured = true;
        }
    }
    Dash_DrillBar(RIGHT_X, PANE_TOP, RIGHT_W, MAIN_H, "DRILL BAR",
                  SurveyDash_Ruler(), DASH_RULER_TICKS, &s->drill, dt);
    DashDrawBarState(s);

    DashDrawCursor(s);

    c2d_end();

    c2d_present_into(&g_surf, region);
}

Vector2 SurveyDash_ToDesign(Rectangle region, Vector2 p)
{
    (void)region;
    return c2d_to_design(&g_surf, p);
}

void SurveyDash_Press(SurveyDashState *s, Rectangle region, Vector2 screenPt)
{
    if (!s || !s->started) return;
    const Vector2 d = SurveyDash_ToDesign(region, screenPt);
    s->down = true;
    s->moved = false;
    s->downPt = d;
    s->onBlock = (d.x >= DASH_BLOCK_X0 && d.x <= DASH_BLOCK_X1 &&
                  d.y >= DASH_BLOCK_Y0 && d.y <= DASH_BLOCK_Y1);

    /* Nothing on the drill bar answers until a hole is planned -- it is
     * drawn dimmed until then, and a dimmed control that still worked would
     * be lying. */
    if (!DashBarLive(s)) return;

    /* C6: the ruler re-plans the depth -- deeper from where the bit stopped,
     * or shorter before it gets there. Checked before the face, because it
     * sits inside the bar and a click on it must re-plan rather than drill. */
    const float pick = Dash_DrillBarPickDepth(RIGHT_X, PANE_TOP, RIGHT_W, MAIN_H, d.x, d.y);
    if (pick >= 0.0f)
    {
        const float m = fmaxf(roundf(pick), DASH_MIN_HOLE_M);
        DrillSim_SetTarget(&s->drill, m);
        DrillProfile_Plan(&s->profile, s->siteI, s->siteJ, s->drill.targetM, s->drill.t);
        char msg[96];
        snprintf(msg, sizeof(msg), "Hole re-planned to %d m.", (int)m);
        DashLog_Push(s, s->drill.t, msg, NULL);
        return;
    }

    /* The first tap on the face STARTS the drill; every later one drives
     * it. Input on PRESS, not release: the loop is a rhythm the player
     * keeps, and waiting for the button to come up puts a lag between the
     * tap and the kick. */
    if (!DashOverFace(d)) return;
    if (SurveyDash_Phase(s) == SDP_PLANNED)
    {
        if (DrillSim_Start(&s->drill))
        {
            char msg[96];
            snprintf(msg, sizeof(msg), "Drilling to %d m. Tap the bar to drive the bit.",
                     (int)(s->drill.targetM + 0.5f));
            DashLog_Push(s, s->drill.t, msg, NULL);
        }
        return;
    }
    DrillSim_Bite(&s->drill);
}

void SurveyDash_Hover(SurveyDashState *s, Rectangle region, Vector2 screenPt)
{
    if (!s || !s->started) return;
    const Vector2 d = SurveyDash_ToDesign(region, screenPt);
    s->pointer = d;
    s->pointerIn = (d.x >= 0.0f && d.x <= (float)SURVEY_DASH_DESIGN_W &&
                    d.y >= 0.0f && d.y <= (float)SURVEY_DASH_DESIGN_H);

    s->aimOn = false;
    if (!s->aimArmed || !g_model) return;
    if (d.x < DASH_BLOCK_X0 || d.x > DASH_BLOCK_X1 ||
        d.y < DASH_BLOCK_Y0 || d.y > DASH_BLOCK_Y1) return;
    s->aimOn = Holo3D_HitCap(g_model, d.x, d.y, &s->aimU, &s->aimV);
}

void SurveyDash_Zoom(SurveyDashState *s, Rectangle region, Vector2 screenPt, float steps)
{
    if (!s || !s->started || steps == 0.0f) return;
    const Vector2 d = SurveyDash_ToDesign(region, screenPt);
    /* Only over the block. The rack and the drill bar are lists, and a wheel
     * over a list should never move something else. */
    if (d.x < DASH_BLOCK_X0 || d.x > DASH_BLOCK_X1 ||
        d.y < DASH_BLOCK_Y0 || d.y > DASH_BLOCK_Y1) return;

    float z = s->view.zoom * powf(DASH_ZOOM_STEP, steps);
    if (z < DASH_ZOOM_MIN) z = DASH_ZOOM_MIN;
    if (z > DASH_ZOOM_MAX) z = DASH_ZOOM_MAX;
    s->view.zoom = z;
}

void SurveyDash_Drag(SurveyDashState *s, Rectangle region, Vector2 delta)
{
    if (!s || !s->started || !s->down || !s->onBlock) return;
    /* the letterbox scale, so a drag turns the block by the same amount
     * whatever the window size */
    const float k = (g_surf.dst.width > 0.0f)
                  ? (float)g_surf.w / g_surf.dst.width : 1.0f;
    (void)region;
    if (fabsf(delta.x) + fabsf(delta.y) > 0.0f) s->moved = true;
    s->block.yaw   += delta.x * k * 0.006f;
    s->block.pitch += delta.y * k * 0.004f;
    if (s->block.pitch < 0.05f) s->block.pitch = 0.05f;
    if (s->block.pitch > 1.30f) s->block.pitch = 1.30f;
    s->block.fast = true;
}

void SurveyDash_Release(SurveyDashState *s, Rectangle region, Vector2 screenPt)
{
    if (!s || !s->started) return;
    const Vector2 d = SurveyDash_ToDesign(region, screenPt);
    s->block.fast = false;
    const bool inMid = DashInMid(d);
    if (s->down && !s->moved && DashStretching(s) && inMid)
    {
        /* The tap that ends the stretch. Anywhere in the middle pane, not
         * only on the block -- the base of the column can sit below the
         * block's own rectangle, and reaching for 120 m should not miss. */
        const float m = DashStretchMetres(s, d);
        DrillSim_SetTarget(&s->drill, m);
        s->depthPicked = true;
        /* THE PLAN EXISTS BEFORE THE FIRST TURN: the profile is opened here,
           with its site and target, and fills once the drill is started. */
        DrillProfile_Plan(&s->profile, s->siteI, s->siteJ, m, s->drill.t);
        char msg[96];
        snprintf(msg, sizeof(msg), "Hole planned to %d m. Tap the drill bar to start digging.",
                 (int)m);
        DashLog_Push(s, s->drill.t, msg, NULL);
    }
    else if (s->down && !s->moved)
    {
        if (s->onBlock)
        {
            const int bed = Holo3D_Hit(g_model, d.x, d.y);
            /* C5: isolate is GATED. Until the model is MEASURED a tap on the
             * block moves the drill site instead of peeling a bed -- the
             * control the player has before they have earned the other one. */
            if (DashKnow_IsMeasured(s->know, DK_LATTICE, DRILL_TARGET_M))
            {
                if (bed >= 0) Holo3D_Select(&s->block, bed);
            }
            else if (!s->aimArmed)
            {
                /* THE GATE. Siting a hole is the drill's act, so the drill has
                 * to be the tool in hand. Without this the block answered taps
                 * whatever was selected, which made the rack decorative. */
                DashLog_Push(s, s->drill.t,
                             "Select the DRILL in the rack before siting a hole.", NULL);
            }
            else if (s->drill.running)
            {
                /* A running string is not abandoned by a stray tap. */
                char msg[96];
                snprintf(msg, sizeof(msg), "Drill running to %d m. Site the next hole when it lands.",
                         (int)(s->drill.targetM + 0.5f));
                DashLog_Push(s, s->drill.t, msg, NULL);
            }
            else
            {
                /* ON THE GROUND, not across the panel. The old mapping took
                 * the tap's position in the pane and called it a position on
                 * the block -- which is only the same thing when the block is
                 * square on and fills the pane. It is neither. */
                float u, v;
                if (Holo3D_HitCap(g_model, d.x, d.y, &u, &v))
                {
                    s->siteU = u; s->siteV = v; s->sited = true;
                    s->siteI = u * (float)DK_LATTICE;
                    s->siteJ = v * (float)DK_LATTICE;
                    s->drill.depthM = 0.0f;      /* a new site is a new hole */
                    s->drill.lift = 0.0f;
                    s->drill.done = false;
                    s->drill.running = false;
                    s->depthPicked = false;       /* and now: how deep */
                    s->drill.targetM = -1.0f;
                    DrillProfile_Clear(&s->profile);
                    char msg[96];
                    snprintf(msg, sizeof(msg), "Site set at %d/%d. Pull down to choose a depth.",
                             (int)s->siteI, (int)s->siteJ);
                    DashLog_Push(s, s->drill.t, msg, NULL);
                }
            }
        }
        else
        {
            /* rack-local units: undo the translate and the scale, in that
               order, exactly as the draw applied them */
            const float rx = (d.x - DASH_RACK_X) / DASH_RACK_SCALE;
            const float ry = (d.y - DASH_RACK_Y) / DASH_RACK_SCALE_Y;
            const int slot = ToolRack_HitTestB(rx, ry, s->rack.slots);
            if (slot >= 0 && s->rack.tools[slot].present)
            {
                /* Report it, do not act on it. The game owns the selection
                 * and hands it back on the next feed, so the rack can never
                 * disagree with SurveyConsole::SelectedTool. A harness with no
                 * game behind it takes the pick and feeds it straight back --
                 * the same handshake, not a second code path. */
                s->toolPick = slot;
            }
        }
    }
    s->down = false;
    s->moved = false;
}
