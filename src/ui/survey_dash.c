/* survey_dash.c — see survey_dash.h. */
#include "survey_dash_internal.h"
#include "bed_palette.h"

#include "c2d.h"

#include <stdio.h>
#include <math.h>
#include <string.h>


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
Holo3DModel *g_dashModel = NULL;

/* What ground the ONE model is currently carrying. It is the model's
 * property, not a console's: two prospecting units have different ground and
 * whichever is on screen has to re-apply its own. */
static void *g_groundCtx = NULL;
static int   g_groundRev = -1;

static float g_revealSlowmo = 1.0f;          /* harness only */
void SurveyDash_SetRevealSlowmo(float f) { g_revealSlowmo = f < 1.0f ? 1.0f : f; }


/* ---- the fog ----------------------------------------------------------
 *
 * The block's fog is the knowledge model read at a point: the same
 * DashKnow_KnowAt, through the same DashKnow_Confidence, that
 * DashKnow_Delineation averages over the volume for the DELINEATION monitor.
 * One model, two readings -- the monitor's number is the fog's mean, so the
 * block and the bar cannot disagree. */
/* While a hole is being taken in (the reveal) the fog is read between the
 * model as it stood before the hole and as it stands now, so the dimness
 * lifts over seconds rather than in a frame. `f` is 1 the rest of the time. */
typedef struct DashFogCtx { const DashKnowledge *from, *to; float f; } DashFogCtx;

static float DashFogOne(const DashKnowledge *k, float u, float v, float depth01)
{
    return DashKnow_Confidence(DashKnow_KnowAt(k, u * (float)DK_LATTICE, v * (float)DK_LATTICE,
                                               depth01 * DRILL_TARGET_M));
}

static float DashFogAt(void *ctx, float u, float v, float depth01)
{
    const DashFogCtx *c = (const DashFogCtx *)ctx;
    const float now = DashFogOne(c->to, u, v, depth01);
    if (c->f >= 1.0f || !c->from) return now;
    const float was = DashFogOne(c->from, u, v, depth01);
    return was + (now - was) * c->f;
}

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
    /* Scrolled away to read something older: the new row lands above it and
       the view stays on what was being read. At the top it just appears. */
    if (s->logScroll > 0.0f) s->logScroll += Dash_LogRowH();
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

const DashDepth *DashRuler(void)
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
    if (g_dashModel && feed->groundAt &&
        (g_groundCtx != feed->groundCtx || g_groundRev != feed->groundRevision))
    {
        Holo3D_SetGround(g_dashModel, feed->bedCount, feed->groundAt, feed->groundCtx,
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
    g_dashModel = Holo3D_Build(&opts);
    if (!g_dashModel) { c2d_surface_destroy(&g_surf); return false; }

    g_resReady = true;
    return true;
}

void SurveyDash_Shutdown(void)
{
    if (!g_resReady) return;
    Holo3D_Free(g_dashModel);
    g_dashModel = NULL;
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
    s->block.pitch = DASH_BLOCK_PITCH;
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
    s->coreOpen = s->coreHover = s->corePinned = -1;
    s->revealT = -1.0f;

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

/* THE SITE, ON THE GROUND. Left where the hole was collared, projected onto
 * the cap so it rides the block's rotation and zoom. */

static void DashDrawSite(const SurveyDashState *s)
{
    /* the site mark and the borehole belong to the hole being made; a
       finished hole is its barrel, and the block is whole again */
    if (!g_dashModel || !s->sited || !DashCutActive(s) || DashClosing(s)) return;
    const Vector2 p = Holo3D_CapPoint(g_dashModel, s->siteU, s->siteV);
    const float pulse = 0.85f + 0.15f * sinf(s->drill.t * 3.0f);
    DashTargetMark(p, 7.0f * pulse, s->drill.t * 0.6f, RGBA8(0xff, 0xc8, 0x4d, 0.95f), 1.6f);
}

/* ---- THE CUTAWAY -------------------------------------------------------
 *
 * Once a site is taken, the quarter of the block between it and the corner
 * nearest the viewer comes out, full depth (Holo3D_DrawCutaway). The site is
 * the inner edge of the cut, so the borehole runs down that edge with the
 * beds on both sides of it.
 *
 * Why it is needed at all: the camera tilts, so a point deep inside the block
 * is drawn higher the further back it sits, and a depth line from a site near
 * the back ended well above the same depth on the front wall -- by up to half
 * the column. Against the cut faces it sits in the rock it goes through, at
 * every yaw and tilt. (A flat slice drawn over the block did the same job
 * first and read as a panel pasted on; see the graveyard.) */

/* The bed a point on the site's borehole is in, by the block's own ground
 * at the site -- the name the tag gives the depth being chosen. */
const char *DashBedAt(const SurveyDashState *s, float screenY)
{
    if (!g_dashModel) return NULL;
    const int beds = Holo3D_LayerCount(g_dashModel);
    for (int k = 0; k < beds; k++)
    {
        Vector2 top, bot;
        Holo3D_BedSpan(g_dashModel, k, s->siteU, s->siteV, &top, &bot);
        if (screenY <= bot.y + 0.5f || k == beds - 1)
        {
            const H3DLayer *ly = Holo3D_Layer(g_dashModel, k);
            return ly ? ly->name : NULL;
        }
    }
    return NULL;
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
    if (!g_dashModel) return 0.0f;
    const Vector2 top = Holo3D_CapPoint(g_dashModel, s->siteU, s->siteV);
    const Vector2 bot = Holo3D_ColumnPoint(g_dashModel, s->siteU, s->siteV, 1.0f);
    const float span = bot.y - top.y;
    if (span <= 1.0f) return 0.0f;
    const float f = (pt.y - top.y) / span;
    return f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f);
}

/* A hole shallower than this is a scratch, and a target at the collar would
 * finish the instant it began. */
#define DASH_MIN_HOLE_M 2.0f

float DashStretchMetres(const SurveyDashState *s, Vector2 pt)
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
    if (!g_dashModel || !s->sited || !DashCutActive(s) || DashClosing(s)) return;
    const Vector2 top = Holo3D_CapPoint(g_dashModel, s->siteU, s->siteV);
    const Vector2 bot = Holo3D_ColumnPoint(g_dashModel, s->siteU, s->siteV, 1.0f);

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

/* THE CUT IS FOR THE HOLE BEING MADE. It opens when a site is taken and
 * closes when the hole is finished or aborted: the whole block is back, so
 * every spot on it can be clicked for the next site. */
bool DashCutActive(const SurveyDashState *s)
{
    const SurveyDashPhase ph = SurveyDash_Phase(s);
    if (ph == SDP_COMPLETE && s->revealT >= 0.0f) return true;   /* taking it in, closing */
    return ph == SDP_STRETCH || ph == SDP_PLANNED || ph == SDP_DRILLING;
}

/* Closing: the cavity is shrinking and the hole's line has gone. */
bool DashClosing(const SurveyDashState *s)
{
    return SurveyDash_Phase(s) == SDP_COMPLETE && s->revealT >= DASH_REVEAL_S;
}

/* WHERE THE CUT'S INNER CORNER IS. At the site while the hole is worked and
 * taken in; while closing it travels out to the block's near corner on an
 * ease-in -- slow at first, gathering speed -- so the cavity contracts and
 * shuts rather than vanishing. */
void DashCutPoint(const SurveyDashState *s, float *u, float *v)
{
    *u = s->siteU; *v = s->siteV;
    if (!DashClosing(s) || !g_dashModel) return;
    float cu, cv;
    Holo3D_NearCorner(g_dashModel, &cu, &cv);
    const float t = Clampf01v((s->revealT - DASH_REVEAL_S) / DASH_CLOSE_S);
    const float e = t * t * t;
    *u = s->siteU + (cu - s->siteU) * e;
    *v = s->siteV + (cv - s->siteV) * e;
}

/* How far the reveal has got, eased: 1 when none is running. */
static float DashRevealF(const SurveyDashState *s)
{
    if (s->revealT < 0.0f) return 1.0f;
    const float t = Clampf01v(s->revealT / DASH_REVEAL_S);
    return t * t * (3.0f - 2.0f * t);
}

/* A hole has landed (or stopped): remember how the model stood, then let it
 * learn -- the reveal reads between the two. */
static void DashBeginReveal(SurveyDashState *s)
{
    s->fogFrom = *s->know;
    s->revealT = 0.0f;
}

/* ---- THE CORE BARRELS -------------------------------------------------
 *
 * Every finished hole stands on the block as a turning core barrel: three
 * dashed staves round a vertical axis between two ellipses, taller the deeper
 * the hole went (the prototype's DrawBores, survey-dashboard.html). It says
 * "drilled here, this deep" at a glance, and it is the handle for the hole's
 * log: hover shows it, a click pins it. An aborted hole's barrel is amber. */

static void DashKeepCore(SurveyDashState *s, float depthM)
{
    int idx = s->coreOpen;
    if (idx < 0 || idx >= s->coreCount)
    {
        if (s->coreCount == SURVEY_DASH_CORES_MAX)
        {
            /* full: the oldest log goes, and every index moves down one */
            memmove(&s->cores[0], &s->cores[1], sizeof(s->cores[0]) * (SURVEY_DASH_CORES_MAX - 1));
            s->coreCount--;
            if (s->corePinned >= 0) s->corePinned--;
        }
        idx = s->coreCount++;
    }
    DrillCoreLog_From(&s->cores[idx], &s->profile, s->siteU, s->siteV, depthM);
    s->coreOpen = idx;
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
bool DashOverFace(Vector2 p)
{
    float fx, fy, fw, fh;
    Dash_DrillBarFace(RIGHT_X, PANE_TOP, RIGHT_W, MAIN_H, &fx, &fy, &fw, &fh);
    return p.x >= fx && p.x <= fx + fw && p.y >= fy && p.y <= fy + fh;
}

bool DashInMid(Vector2 p)
{
    return p.x >= MID_X && p.x <= MID_X + MID_W && p.y >= PANE_TOP && p.y <= PANE_TOP + PANE_H;
}


static bool DashInLog(Vector2 p);
static bool DashLogPress(SurveyDashState *s, Vector2 d);

/* over one of the log's scrollbar parts: the arrows or the track */
static bool DashOverLogBar(Vector2 p)
{
    Rectangle up, down, track;
    Dash_LogBar(LOG_X, LOG_Y, LOG_W, LOG_H, &up, &down, &track);
    return CheckCollisionPointRec(p, up) || CheckCollisionPointRec(p, down) ||
           CheckCollisionPointRec(p, track);
}

SurveyDashCursor SurveyDash_Cursor(const SurveyDashState *s)
{
    if (!s || !s->started || !s->pointerIn) return SDC_ARROW;
    if (s->coreHover >= 0 && SurveyDash_Phase(s) != SDP_STRETCH) return SDC_HAND;
    if (DashWantsGrab(s)) return SDC_HIDDEN;           /* the console draws the hand */
    if (s->logDrag || DashOverLogBar(s->pointer)) return SDC_HAND;
    switch (SurveyDash_Phase(s))
    {
        /* The drill is the pointer where it can site a hole -- over the
         * block's pane. Over the rack or the bar it would be pointing at
         * things it cannot collar. */
        case SDP_AIM:
        case SDP_COMPLETE:
            /* not over the log: it is read, not drilled */
            return (s->aimArmed && DashInMid(s->pointer) && !DashInLog(s->pointer)) ? SDC_HIDDEN : SDC_ARROW;
        /* the arrow is the precise thing to pick a height with */
        case SDP_STRETCH:
            return DashOverCtrl(s, s->pointer) ? SDC_HAND : SDC_ARROW;
        /* the bar is now a button: the hand says so */
        case SDP_PLANNED:
            return (DashOverFace(s->pointer) || DashOverCtrl(s, s->pointer)) ? SDC_HAND : SDC_ARROW;
        case SDP_DRILLING:
            return (DashOverFace(s->pointer) || DashOverCtrl(s, s->pointer)) ? SDC_HAND : SDC_ARROW;
    }
    return SDC_ARROW;
}

/* ---- THE TITLE-ROW CONTROL: CANCEL, then ABORT --------------------------
 *
 * One slot in the drill bar's title row, holding whatever takes the last
 * step back.
 *
 *   STRETCH, PLANNED  CANCEL -- exactly what a right-click does
 *                     (SurveyDash_Cancel). A phone has no right-click, so
 *                     without it a misplaced site could only be drilled.
 *   DRILLING          ABORT -- ends work under way. The hole is left at the
 *                     depth the bit reached.
 *
 * Drawn over the bar's veil, so it is live while the bar itself is dimmed. */
#define DASH_CTRL_W 104.0f
#define DASH_CTRL_H  30.0f
#define DASH_CTRL_X (RIGHT_X + RIGHT_W - DASH_CTRL_W - 24.0f)
#define DASH_CTRL_Y (PANE_TOP + 18.0f)

typedef enum { DASH_CTRL_NONE, DASH_CTRL_CANCEL, DASH_CTRL_ABORT } DashCtrl;

static DashCtrl DashTitleCtrl(const SurveyDashState *s)
{
    switch (SurveyDash_Phase(s))
    {
        case SDP_STRETCH:
        case SDP_PLANNED:  return DASH_CTRL_CANCEL;
        case SDP_DRILLING: return DASH_CTRL_ABORT;
        default:           return DASH_CTRL_NONE;
    }
}

bool DashOverCtrl(const SurveyDashState *s, Vector2 p)
{
    return DashTitleCtrl(s) != DASH_CTRL_NONE &&
           p.x >= DASH_CTRL_X && p.x <= DASH_CTRL_X + DASH_CTRL_W &&
           p.y >= DASH_CTRL_Y && p.y <= DASH_CTRL_Y + DASH_CTRL_H;
}

static void DashDrawCtrl(const SurveyDashState *s)
{
    const DashCtrl k = DashTitleCtrl(s);
    if (k == DASH_CTRL_NONE) return;
    const bool hot = s->pointerIn && DashOverCtrl(s, s->pointer);
    const float x = DASH_CTRL_X, y = DASH_CTRL_Y, w = DASH_CTRL_W, h = DASH_CTRL_H;
    const C2DCorner cr[4] = {{x, y, 6.0f}, {x + w, y, 6.0f}, {x + w, y + h, 6.0f}, {x, y + h, 6.0f}};
    Vector2 v[64];
    const int n = c2d_rpoly_pts(cr, 4, 0.0f, 0.0f, v, 63);
    const bool abort = k == DASH_CTRL_ABORT;
    /* ABORT is red: it stops work. CANCEL is the console's own cyan: it only
       takes back a choice. */
    const Color fill = abort ? (hot ? RGBA8(0x5a, 0x14, 0x14, 0.95f) : RGBA8(0x2a, 0x0c, 0x10, 0.92f))
                             : (hot ? RGBA8(0x0c, 0x3a, 0x4c, 0.95f) : RGBA8(0x06, 0x1e, 0x2a, 0.92f));
    const Color ink = abort ? RGBA8(0xff, 0x5a, 0x5a, hot ? 1.0f : 0.85f)
                            : RGBA8(0x35, 0xd8, 0xee, hot ? 1.0f : 0.85f);
    c2d_fill_poly(v, n, fill);
    v[n] = v[0];
    c2d_polyline(v, n + 1, ink, hot ? 1.8f : 1.4f);
    const float gx = x + 17.0f, gy = y + h * 0.5f;
    if (abort)
        c2d_rect(gx - 5.0f, gy - 5.0f, 10.0f, 10.0f, ink);            /* a stop square */
    else
    {
        const Vector2 a[2] = {{gx - 5.0f, gy - 5.0f}, {gx + 5.0f, gy + 5.0f}};
        const Vector2 b[2] = {{gx + 5.0f, gy - 5.0f}, {gx - 5.0f, gy + 5.0f}};
        c2d_polyline(a, 2, ink, 2.2f);                                  /* a cross */
        c2d_polyline(b, 2, ink, 2.2f);
    }
    c2d_text(C2D_W700, 14.0f, abort ? "ABORT" : "CANCEL", x + 32.0f, gy + 5.0f, ink,
             C2D_ALIGN_LEFT, C2D_BASELINE_ALPHABETIC);
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
    DashKeepOut_Clear();
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
    /* THE RETICLE IS THE DRILL'S TIP. The reference parks it at the centre
     * of the cap, where on real ground it read as a site already chosen and
     * never moved. It is drawn only where the drill would collar -- under the
     * pointer, laid on the ground so it follows the terrain and the view --
     * and only while the drill is in hand over the cap. */
    H3DHud hud = {0};
    if (s->aimOn && SurveyDash_Cursor(s) == SDC_HIDDEN && !DashWantsGrab(s))
    {
        hud.reticle   = true;
        hud.reticleAt = true;
        hud.reticleU  = s->aimU;
        hud.reticleV  = s->aimV;
        hud.reticleR  = DASH_TIP_RETICLE_R;
    }
    /* THE REVEAL: advance it, and draw the beds and the fog that far */
    if (s->revealT >= 0.0f)
    {
        s->revealT += dt / g_revealSlowmo;
        if (s->revealT >= DASH_REVEAL_S + DASH_CLOSE_S) s->revealT = -1.0f;
    }
    const float rf = DashRevealF(s);
    Holo3D_GroundBlend(g_dashModel, rf);

    /* the model is shared; the knowledge is this console's */
    static DashFogCtx fog;
    fog.from = (s->revealT >= 0.0f) ? &s->fogFrom : NULL;
    fog.to = s->know;
    fog.f = rf;
    Holo3D_SetFog(g_dashModel, DashFogAt, &fog);
    Holo3D_Render(g_dashModel, &s->block, &s->view);
    if (DashCutActive(s))
    {
        float cu, cv;
        DashCutPoint(s, &cu, &cv);
        Holo3D_DrawCutaway(g_dashModel, &s->block, cu, cv, DashC_Bg());
    }
    Holo3D_DrawHud(g_dashModel, &s->block, &s->view, &hud);

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
                           : (ph == SDP_COMPLETE && s->profile.aborted) ? "ABORTED"
                           : ph == SDP_COMPLETE   ? "AT TARGET"
                           : ph == SDP_PLANNED    ? "READY"
                           : "IDLE";
        Dash_DrillStats(RIGHT_X, STATS_Y, RIGHT_W, STATS_H, dr, status);
    }

    DashDrawHeightLog();
    DashDrawBorehole(s);
    DashDrawSite(s);
    DashDrawBarrels(s);

    {
        /* the monitor climbs with the fog: one model, read between the same
           two states over the same seconds */
        const float to = DashKnow_Delineation(s->know, DK_LATTICE, DRILL_TARGET_M);
        float delin = to;
        DashKnowledge *tierFrom = s->know;
        if (s->revealT >= 0.0f)
        {
            const float was = DashKnow_Delineation(&s->fogFrom, DK_LATTICE, DRILL_TARGET_M);
            delin = was + (to - was) * rf;
            if (rf < 0.5f) tierFrom = &s->fogFrom;
        }
        Dash_Confidence(LOG_X, CONF_Y, LOG_W, CONF_H, delin,
                        DashKnow_Tier(tierFrom, DK_LATTICE, DRILL_TARGET_M),
                        DashKnow_IsMeasured(tierFrom, DK_LATTICE, DRILL_TARGET_M));
    }
    DashLog_Bind(&s->log);
    s->logScroll = fminf(fmaxf(s->logScroll, 0.0f),
                         Dash_LogMaxScroll(LOG_H, s->log.entry, s->log.count));
    Dash_Log(LOG_X, LOG_Y, LOG_W, LOG_H, s->log.entry, s->log.count, s->logScroll);

    DrillSim_Step(&s->drill, dt);
    DrillProfile_Record(&s->profile, &s->drill);
    if (s->drill.completed)
    {
        /* C5+C6 meet here: a finished hole is what the model learns from. */
        DashBeginReveal(s);
        DashKnow_Add(s->know, s->siteI, s->siteJ, s->drill.completedAtM);
        DashKeepCore(s, s->drill.completedAtM);
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
                  DashRuler(), DASH_RULER_TICKS, &s->drill, dt);
    {
        /* the drill bar's ruler, labels and all, is off limits to overlays */
        float brx, bry0, bry1;
        Dash_DrillBarSpan(RIGHT_X, PANE_TOP, RIGHT_W, MAIN_H, &brx, &bry0, &bry1);
        DashKeepOut_Add((Rectangle){brx - 16.0f, bry0 - 24.0f,
                                    RIGHT_X + RIGHT_W - (brx - 16.0f), bry1 - bry0 + 40.0f});
    }
    DashDrawBarState(s);
    DashDrawCtrl(s);

    /* a pinned log stays; otherwise the one under the pointer, except while
       a depth is being stretched, when the pointer is busy */
    {
        int card = s->corePinned;
        if (card < 0 && SurveyDash_Phase(s) != SDP_STRETCH) card = s->coreHover;
        DashDrawCoreCard(s, card);
    }

    DashDrawCursor(s);

    c2d_end();

    c2d_present_into(&g_surf, region);
}

Vector2 SurveyDash_ToDesign(Rectangle region, Vector2 p)
{
    (void)region;
    return c2d_to_design(&g_surf, p);
}

/* A hole's look, from where it was sited: two holes differ, and the same
   site redraws the same hole. */
static unsigned int DashHoleSeed(float u, float v)
{
    return (unsigned int)(u * 4096.0f) * 73856093u ^ (unsigned int)(v * 4096.0f) * 19349663u;
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

    /* the log's scrollbar: a step, a page, or the start of a thumb drag */
    if (DashInLog(d) && DashLogPress(s, d))
    {
        if (!s->logDrag) s->down = false;          /* a tap: consumed */
        return;
    }

    /* CANCEL answers even while the bar is dimmed -- it is drawn over the
     * veil. The press is consumed, so the release does nothing more. */
    if (DashTitleCtrl(s) == DASH_CTRL_CANCEL && DashOverCtrl(s, d))
    {
        SurveyDash_Cancel(s);
        s->down = false;
        return;
    }

    /* Nothing else on the drill bar answers until a hole is planned -- it is
     * drawn dimmed until then, and a dimmed control that still worked would
     * be lying. */
    if (!DashBarLive(s)) return;

    if (DashTitleCtrl(s) == DASH_CTRL_ABORT && DashOverCtrl(s, d))
    {
        const float at = s->drill.depthM;
        if (DrillSim_Abort(&s->drill))
        {
            DrillProfile_Abort(&s->profile, &s->drill);
            char msg[96];
            /* the hole is real to the depth it reached, and the model learns
               from it the same as from one that landed on its plan */
            if (at >= DASH_MIN_HOLE_M)
            {
                DashBeginReveal(s);
                DashKnow_Add(s->know, s->siteI, s->siteJ, at);
                DashKeepCore(s, at);
                snprintf(msg, sizeof(msg), "Drilling aborted. Hole left at %d m and logged.",
                         (int)(at + 0.5f));
            }
            else
                snprintf(msg, sizeof(msg), "Drilling aborted at the collar. Nothing to log.");
            DashLog_Push(s, s->drill.t, msg, NULL);
        }
        return;
    }

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

    s->coreHover = (g_dashModel && s->pointerIn) ? DashBarrelAt(s, d) : -1;

    /* over the block's sides -- not its cap, which is where holes are sited
       -- the pointer is for turning the block */
    s->overBody = false;
    if (g_dashModel && s->coreHover < 0 &&
        d.x >= DASH_BLOCK_X0 && d.x <= DASH_BLOCK_X1 && d.y >= DASH_BLOCK_Y0 && d.y <= DASH_BLOCK_Y1)
    {
        float u, v;
        s->overBody = !Holo3D_HitCap(g_dashModel, d.x, d.y, &u, &v) && Holo3D_Hit(g_dashModel, d.x, d.y) >= 0;
    }

    s->aimOn = false;
    if (!s->aimArmed || !g_dashModel) return;
    /* on a barrel the pointer is picking a log, not a site */
    if (s->coreHover >= 0 && SurveyDash_Phase(s) != SDP_STRETCH) return;
    if (d.x < DASH_BLOCK_X0 || d.x > DASH_BLOCK_X1 ||
        d.y < DASH_BLOCK_Y0 || d.y > DASH_BLOCK_Y1) return;
    s->aimOn = Holo3D_HitCap(g_dashModel, d.x, d.y, &s->aimU, &s->aimV);
}

static bool DashInLog(Vector2 p)
{
    return p.x >= LOG_X && p.x <= LOG_X + LOG_W && p.y >= LOG_Y && p.y <= LOG_Y + LOG_H;
}

void SurveyDash_Wheel(SurveyDashState *s, Rectangle region, Vector2 screenPt, float notches)
{
    if (!s || !s->started || notches == 0.0f) return;
    if (!DashInLog(SurveyDash_ToDesign(region, screenPt))) return;
    /* up is newer: the newest entry is at the top */
    s->logScroll -= notches * Dash_LogRowH();
}

/* A press on the log's scrollbar: the arrows step a row, the track pages,
 * the thumb starts a drag. True when it took the press. */
static bool DashLogPress(SurveyDashState *s, Vector2 d)
{
    Rectangle up, down, track, thumb;
    Dash_LogBar(LOG_X, LOG_Y, LOG_W, LOG_H, &up, &down, &track);
    Dash_LogThumb(LOG_X, LOG_Y, LOG_W, LOG_H, s->log.entry, s->log.count, s->logScroll, &thumb);
    const float row = Dash_LogRowH();
    if (CheckCollisionPointRec(d, up))    { s->logScroll -= row; return true; }
    if (CheckCollisionPointRec(d, down))  { s->logScroll += row; return true; }
    if (CheckCollisionPointRec(d, thumb))
    {
        s->logDrag = true;
        s->logDragY0 = d.y;
        s->logScroll0 = s->logScroll;
        return true;
    }
    if (CheckCollisionPointRec(d, track))
    {
        s->logScroll += (d.y < thumb.y ? -1.0f : 1.0f) * (LOG_H - row);   /* a page */
        return true;
    }
    return false;
}

void SurveyDash_Drag(SurveyDashState *s, Rectangle region, Vector2 delta)
{
    if (!s || !s->started || !s->down) return;
    /* the letterbox scale, so a drag turns the block by the same amount
     * whatever the window size */
    const float k = (g_surf.dst.width > 0.0f)
                  ? (float)g_surf.w / g_surf.dst.width : 1.0f;
    if (s->logDrag)
    {
        /* the thumb follows the pointer: design units of travel, times the
           scroll each unit is worth */
        s->moved = true;
        s->logDragY0 += delta.y * k;
        const float per = Dash_LogThumb(LOG_X, LOG_Y, LOG_W, LOG_H, s->log.entry, s->log.count,
                                        s->logScroll0, NULL);
        s->logScroll = s->logScroll0 + (s->logDragY0 - s->downPt.y) * per;
        return;
    }
    if (!s->onBlock) return;
    (void)region;
    if (fabsf(delta.x) + fabsf(delta.y) > 0.0f) s->moved = true;
    /* yaw only: the block turns about its vertical axis, and the tilt stays */
    s->block.yaw += delta.x * k * 0.006f;
    s->block.fast = true;
}

void SurveyDash_Release(SurveyDashState *s, Rectangle region, Vector2 screenPt)
{
    if (!s || !s->started) return;
    if (s->logDrag)
    {
        s->logDrag = false;
        s->down = false;
        s->moved = false;
        return;
    }
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
        snprintf(msg, sizeof(msg), "Hole planned to %d m. Tap the drill bar to dig; CANCEL undoes.",
                 (int)m);
        DashLog_Push(s, s->drill.t, msg, NULL);
    }
    else if (s->down && !s->moved && s->onBlock && DashBarrelAt(s, d) >= 0)
    {
        /* a barrel is the hole's log: a click holds it open, or lets it go */
        const int b = DashBarrelAt(s, d);
        s->corePinned = (s->corePinned == b) ? -1 : b;
    }
    else if (s->down && !s->moved)
    {
        if (s->onBlock)
        {
            const int bed = Holo3D_Hit(g_dashModel, d.x, d.y);
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
                if (Holo3D_HitCap(g_dashModel, d.x, d.y, &u, &v))
                {
                    s->siteU = u; s->siteV = v; s->sited = true;
                    s->siteI = u * (float)DK_LATTICE;
                    s->siteJ = v * (float)DK_LATTICE;
                    s->drill.depthM = 0.0f;      /* a new site is a new hole */
                    s->drill.holeSeed = DashHoleSeed(s->siteU, s->siteV);
                    s->drill.lift = 0.0f;
                    s->drill.done = false;
                    s->drill.running = false;
                    s->depthPicked = false;       /* and now: how deep */
                    s->drill.targetM = -1.0f;
                    DrillProfile_Clear(&s->profile);
                    s->coreOpen = -1;             /* a new hole, a new log */
                    if (s->revealT >= 0.0f) { s->revealT = -1.0f; Holo3D_GroundBlend(g_dashModel, 1.0f); }
                    char msg[96];
                    snprintf(msg, sizeof(msg), "Site set at %d/%d. Pull down for a depth; CANCEL undoes.",
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

void SurveyDash_Cancel(SurveyDashState *s)
{
    if (!s || !s->started) return;
    char msg[96];
    switch (SurveyDash_Phase(s))
    {
        case SDP_STRETCH:
            /* the site goes; the drill is back in hand */
            s->sited = false;
            DrillProfile_Clear(&s->profile);
            DashLog_Push(s, s->drill.t, "Site cancelled.", NULL);
            return;
        case SDP_PLANNED:
            if (s->drill.depthM > 0.05f)
            {
                /* a deeper plan for a hole already drilled: the hole stays
                   where it stopped, and so does what it recorded */
                s->drill.targetM = s->drill.depthM;
                s->profile.targetM = s->drill.depthM;
                snprintf(msg, sizeof(msg), "Deeper plan cancelled. Hole stays at %d m.",
                         (int)(s->drill.depthM + 0.5f));
                DashLog_Push(s, s->drill.t, msg, NULL);
                return;
            }
            /* back to choosing the depth, at the same site */
            s->depthPicked = false;
            s->drill.targetM = -1.0f;
            DrillProfile_Clear(&s->profile);
            DashLog_Push(s, s->drill.t, "Depth cancelled. Pull down to choose again.", NULL);
            return;
        case SDP_DRILLING:
            DashLog_Push(s, s->drill.t, "The drill is running. ABORT on the drill bar stops it.", NULL);
            return;
        case SDP_AIM:
        case SDP_COMPLETE:
            /* nothing to take back but a log held open */
            if (s->corePinned >= 0) s->corePinned = -1;
            return;
    }
}

void SurveyDash_DrillNow(SurveyDashState *s, float u, float v, float depthM, bool reveal)
{
    if (!s) return;
    if (!s->started) SurveyDash_Reset(s);
    if (!s->know) s->know = &s->own;
    const float m = fmaxf(DASH_MIN_HOLE_M, fminf(depthM, DRILL_TARGET_M));

    s->siteU = Clampf01v(u); s->siteV = Clampf01v(v);
    s->siteI = s->siteU * (float)DK_LATTICE;
    s->siteJ = s->siteV * (float)DK_LATTICE;
    s->sited = true;
    s->depthPicked = true;
    s->coreOpen = -1;

    DrillSim_Reset(&s->drill);
    s->drill.holeSeed = DashHoleSeed(s->siteU, s->siteV);
    DrillProfile_Clear(&s->profile);
    DrillSim_SetTarget(&s->drill, m);
    DrillProfile_Plan(&s->profile, s->siteI, s->siteJ, m, s->drill.t);
    DrillSim_Start(&s->drill);
    for (int i = 0; i < 200000 && s->drill.running; i++)
    {
        if (i % 6 == 0) DrillSim_Bite(&s->drill);
        DrillSim_Step(&s->drill, 1.0f / 30.0f);
        DrillProfile_Record(&s->profile, &s->drill);
    }
    if (reveal) DashBeginReveal(s);
    else        s->revealT = -1.0f;
    DashKnow_Add(s->know, s->siteI, s->siteJ, s->drill.depthM);
    DashKeepCore(s, s->drill.depthM);
}
