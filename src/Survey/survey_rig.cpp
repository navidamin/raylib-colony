#include "survey_rig.h"
#include "survey_console.h"

#include "rlgl.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

/* Every number in this file came off the prototype, and most of them came off
   the reference artwork before that. None was re-derived here: a pixel tool
   whose proportions drift between two implementations is two tools. */
namespace
{
    constexpr float GLY_U = 4.0f;              // the pixel unit -- MUST stay whole
    constexpr float GLY_H = GLY_U * 0.5f;      // the sprite grid's half-unit cell
    constexpr int   GLY_DASHES = 2;            // the lead line's live dashes
    constexpr float BIT_PX  = 7.25f * GLY_U;   // 29 -- tip to the box's underside
    constexpr float HEAD_PX = 3.0f * GLY_U;    // 12 -- the marker box, the handle
    constexpr float TAIL_PX = 5.0f * 2.0f * GLY_U;   // 40 -- string still to feed
    constexpr float TAIL_STUB = 2.0f * GLY_U;  // one dash still standing at the end
    constexpr float SINK_MAX = TAIL_PX - TAIL_STUB;
    constexpr float DRILL_S = 0.78f;

    constexpr float SPUD_T = 0.85f, OUT_T = 0.9f, HOLD_T = 1.0f, FADE_T = 0.3f;
    constexpr float EJECTA_G_PX = 300.0f;

    constexpr Color G_LIGHT  = {250, 250, 250, 255};
    constexpr Color G_MID    = {133, 149, 172, 255};
    constexpr Color G_OUT    = { 10,  14,  20, 255};   // Dark Plating's near-black
    constexpr Color G_CY     = { 80, 225, 255, 255};
    /* The tool's standby colour: the cyan's own sibling, hue swung from 192 to
       150 with saturation and lightness held, so a rig that is standing and a
       rig that is cutting read as one machine in two states. Deliberately NOT
       amber -- amber means "this control wants you", and it is the borehole
       bar that wants you while the rig waits. */
    constexpr Color G_STANDBY = { 82, 255, 168, 255};
    constexpr Color G_BAD     = {226,  96,  78, 255};
    constexpr Color G_BADDIM  = {138,  78,  68, 255};

    /* The bit, sampled off the reference artwork at half-unit resolution:
       'L' light steel, 'm' mid steel, '.' nothing. The shape is the artwork's
       shape and not anybody's reading of it. */
    const char* BIT_ROWS[] = {
        "...mLm.....", "...mLm.....", "..mmLm.....", "..LLLmmm...",
        "..LLLLmm...", "...mm......", ".mmmLm.....", "mLLLLmmmm..",
        ".LLLLLmm...", "..mmmm.....", ".mmmmmm....", "mLLLLLmmm..",
        "mLLLLLmmm.."
    };
    constexpr int BIT_N = 13;
    constexpr int BIT_AX = 4;                  // the axis column in that grid

    inline float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
    inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }
    inline Color Fade2(Color c, float a) { return Fade(c, Clamp01(a)); }

    void Rect(float x, float y, float w, float h, Color c)
    {
        DrawRectangle(static_cast<int>(std::lround(x)), static_cast<int>(std::lround(y)),
                      static_cast<int>(std::lround(w)), static_cast<int>(std::lround(h)), c);
    }

    /* A soft ring under a fill, standing in for the canvas shadowBlur the
       prototype uses. Kept to the marker box alone: bloomed over the whole rig
       it eats the sprite's cells, and a pixel tool with soft edges is not a
       pixel tool. */
    void Bloom(float x, float y, float w, float h, Color c)
    {
        for (int k = 3; k >= 1; k--)
            Rect(x - k * 1.5f, y - k * 1.5f, w + k * 3.0f, h + k * 3.0f,
                 Fade2(c, 0.06f * (4 - k)));
    }

    /* The bit itself, cell by cell, tip at (X, Y) and growing upward. Shared:
       the cursor and the planted rig draw THIS, which is the whole point --
       the machine you pick up has to be the machine that goes in.

       Its SILHOUETTE never moves; only the tones cycle through it, one cell at
       a time, leftwards. Shifting the sprite itself would slide the tool
       sideways; shifting the shading inside a fixed outline is a helix turning
       under a stationary flute, which is the thing being drawn. */
    void DrawBitSprite(float X, float Y, int shift)
    {
        for (int r = 0; r < BIT_N; r++)
        {
            const char* row = BIT_ROWS[r];
            const float y = Y - (r + 1) * GLY_H;
            int at[16], n = 0;
            for (int c = 0; row[c]; c++) if (row[c] != '.') at[n++] = c;
            if (!n) continue;
            for (int k = 0; k < n; k++)
            {
                const int src = ((k + shift) % n + n) % n;
                Rect(X + (at[k] - BIT_AX) * GLY_H, y, GLY_H, GLY_H,
                     row[at[src]] == 'L' ? G_LIGHT : G_MID);
            }
        }
    }

    /* One length of string, in the sprite's own three columns -- the bit's top
       row IS a cross-section of the string, so the shaft is not a second
       drawing of a rod, it is that row continued upward. */
    void DrawShaft(float X, float y0, float y1)
    {
        if (y1 - y0 < 1.0f) return;
        const float top = std::round(y0), ht = std::round(y1) - top;
        Rect(std::round(X - GLY_H) - 1.0f, top, GLY_H * 3.0f + 2.0f, ht, G_OUT);
        const char* row = BIT_ROWS[0];
        for (int c = 0; row[c]; c++)
        {
            if (row[c] == '.') continue;
            Rect(X + (c - BIT_AX) * GLY_H, top, GLY_H, ht,
                 row[c] == 'L' ? G_LIGHT : G_MID);
        }
    }

    /* THE GROUND RING. Open across the top, where the bit stands: that gap is
       what stops the ring reading as a plate the bit is stuck through.

       Drawn as separate AXIS-ALIGNED rectangles, not as a dashed ellipse
       stroke. A dashed ellipse gives segments rotated to the tangent, and
       rotated segments are the one thing pixel art never has. Each dash takes
       its width and height from the local tangent instead, so it comes out
       wide and flat where the curve runs horizontal and tall where it runs
       vertical, exactly as the artwork does it. */
    void DrawGlyphRing(float X, float Y, float spin, Color col, float u)
    {
        const float rx = 7.0f * u, ry = 3.2f * u;
        const float GAP = 35.0f * PI / 180.0f;
        const int NR = 16;
        for (int k = 0; k < NR; k++)
        {
            float a = (k / static_cast<float>(NR)) * 2.0f * PI + spin;
            a = std::fmod(std::fmod(a + PI, 2.0f * PI) + 2.0f * PI, 2.0f * PI) - PI;
            if (std::fabs(a + PI / 2.0f) < GAP) continue;      // behind the bit
            const float px = X + rx * std::cos(a), py = Y + ry * std::sin(a);
            const float tx = -rx * std::sin(a), ty = ry * std::cos(a);
            const float m = std::max(std::hypot(tx, ty), 1e-6f), len = u * 1.5f;
            const float dw = std::max(u, std::fabs(tx) / m * len);
            const float dh = std::max(u, std::fabs(ty) / m * len);
            Rect(px - dw * 0.5f, py - dh * 0.5f, dw, dh, col);
        }
    }

    /* THE GLYPH RUNNING. Three motions, all quantised to whole pixels or whole
       cells -- a pixel sprite that eases through fractions is a smooth sprite
       with a pixel texture, which is the failure this style cannot survive.
       The pulses step, the shading steps, the ring steps, and all three run
       anticlockwise whenever the tool is out. */
    void DrawDrillGlyph(float cx, float tipY, int mode, float t)
    {
        const float u = GLY_U, h = GLY_H;
        const bool run = t >= 0.0f;
        const float CYC = 0.85f;
        const float ph = run ? std::fmod(t, CYC) / CYC : -1.0f;
        const int beat = ph < 0.0f ? -1 : static_cast<int>(ph * (GLY_DASHES + 1));
        const float swell = ph < 0.0f ? 0.0f
            : std::sin((ph * (GLY_DASHES + 1) - beat) * PI);
        auto grow = [&](float base, float amt, bool on) {
            return base + (on ? std::round(swell * amt * base * 0.5f) * 2.0f : 0.0f);
        };
        const bool bad = mode == 2, lit = mode == 1;
        const Color col = bad ? G_BAD : G_CY;
        const float X = std::round(cx), Y = std::round(tipY);

        DrawGlyphRing(X, Y, run ? -t * 1.9f : 0.0f,
                      Fade2(bad ? G_BADDIM : G_CY, 0.95f), u);

        /* The business end. Absent when the spot is refused -- the reference
           shows no tool at all on an invalid position, which says "not here"
           far faster than a tool wearing a warning colour. */
        if (!bad) DrawBitSprite(X, Y, run ? static_cast<int>(t * 9.0f) : 0);

        // The marker: a hollow box, or an X where the ground is refused.
        const float boxS = grow(3.0f * u, 0.20f, beat == GLY_DASHES);
        const float bx = X - boxS * 0.5f;
        const float by = Y - 10.25f * u - (boxS - 3.0f * u) * 0.5f;
        if (bad)
        {
            DrawLineEx({bx + u * 0.5f, by + u * 0.5f}, {bx + u * 2.5f, by + u * 2.5f}, u, G_BAD);
            DrawLineEx({bx + u * 2.5f, by + u * 0.5f}, {bx + u * 0.5f, by + u * 2.5f}, u, G_BAD);
        }
        else
        {
            if (lit) Bloom(bx, by, boxS, boxS, G_CY);
            const float st2 = std::round(boxS / 3.0f), hole = boxS - 2.0f * st2;
            Rect(bx, by, boxS, st2, col);
            Rect(bx, by + st2 + hole, boxS, st2, col);
            Rect(bx, by + st2, st2, hole, col);
            Rect(bx + st2 + hole, by + st2, st2, hole, col);
        }

        /* The lead line. The swell runs top down and finishes on the box, so
           the line reads as feeding the tool rather than as decoration. */
        for (int k = 0; k < GLY_DASHES; k++)
        {
            const float d = grow(u, 0.40f, beat == GLY_DASHES - 1 - k);
            Rect(X - d * 0.5f, Y - (12.0f + 2.0f * k) * u - (d - u) * 0.5f, d, d, col);
        }
        if (bad)
            for (int k = 0; k < 3; k++)
                Rect(X - h, Y - (2.2f + 1.7f * k) * u, u, u, Fade2(G_BAD, 0.9f));
    }

    /* tipY is the point of the bit; everything is measured up from it, so the
       tip can be put anywhere -- on the cursor, or sinking into the ground.
       `tail` is how much string is standing above the box. */
    void DrawRigBody(float cx, float tipY, bool lit, float tail, float feed,
                     float headBotY, float clock)
    {
        const float u = GLY_U, h = GLY_H;
        const float X = std::round(cx), TIP = std::round(tipY);
        const float mid = X + h * 0.5f;              // the string's own centre line
        // The box is not carried by the bit. Once the rig is collared it stays
        // on the ground while the string runs down through it, so its position
        // is an argument, not something measured up from the tip.
        const float boxBot = std::round(headBotY);
        const float boxTop = boxBot - HEAD_PX;

        DrawBitSprite(X, TIP, lit ? static_cast<int>(clock * 9.0f) : 0);
        // The string that has gone down through the box. Underground and
        // clipped away in practice, but the rig is not drawn in pieces.
        DrawShaft(X, boxTop, TIP - BIT_N * h);

        /* THE TAIL. The string still to go in, and the one part of the tool
           that changes size. Its collars are the glyph's own lead dashes,
           marching DOWN it while cutting -- the only way a rod of constant
           width can show that it is being fed. */
        if (tail > 1.0f)
        {
            const float ty0 = std::round(boxTop - tail);
            DrawShaft(X, ty0, boxTop);
            const float step = 2.0f * u;
            const float off = std::fmod(std::fmod(feed, step) + step, step);
            for (float yy = ty0 + off; yy < boxTop - u * 0.5f; yy += step)
            {
                Rect(mid - u * 0.5f - 1.0f, yy - 1.0f, u + 2.0f, u + 2.0f, G_OUT);
                Rect(mid - u * 0.5f, yy, u, u, G_CY);
            }
        }

        /* THE HANDLE IS THE MARKER BOX. Hollow, so the string is seen running
           through it, which is the thing the rig actually does. It carries the
           one piece of state the tool has: green standing, cyan cutting -- two
           points on the same neon, so it reads as one machine changing state. */
        const Color col = lit ? G_CY : G_STANDBY;
        const float bx = mid - HEAD_PX * 0.5f, by = boxTop;
        const float th = std::round(HEAD_PX / 3.0f), hole = HEAD_PX - 2.0f * th;
        Bloom(bx, by, HEAD_PX, HEAD_PX, col);
        Rect(bx - 1.0f, by - 1.0f, HEAD_PX + 2.0f, HEAD_PX + 2.0f, G_OUT);
        Rect(bx, by, HEAD_PX, th, col);
        Rect(bx, by + th + hole, HEAD_PX, th, col);
        Rect(bx, by + th, th, hole, col);
        Rect(bx + th + hole, by + th, th, hole, col);
    }

    /* The mouth. Dark, with a lit lip on the near side and a hair of shadow on
       the far one -- the same implied upper-left key the rest of the panel
       uses, which is what makes a flat ellipse read as a hole rather than a
       sticker. */
    void DrawCollar(float cx, float cy, float k)
    {
        const float rw = 10.5f * DRILL_S * (0.55f + 0.45f * k), rh = rw * 0.38f;
        DrawEllipse(static_cast<int>(cx), static_cast<int>(cy), rw, rh, Color{10, 9, 7, 255});
        DrawEllipseLines(static_cast<int>(cx), static_cast<int>(cy), rw, rh,
                         Color{190, 205, 220, 56});
    }

    // A seeded grain, so a hole's churn is the same churn every frame instead
    // of boiling.
    uint32_t grainSeed = 1;
    float Rnd()
    {
        grainSeed = grainSeed * 1664525u + 1013904223u;
        return static_cast<float>((grainSeed >> 8) & 0xFFFFFF) / 16777216.0f;
    }

    Color Scale(Color c, float f)
    {
        auto ch = [f](unsigned char v) {
            const float r = v * f; return static_cast<unsigned char>(r > 255.0f ? 255.0f : r);
        };
        return { ch(c.r), ch(c.g), ch(c.b), c.a };
    }

    /* A FINISHED hole is not a hole. Once the string is out, what is at the
       surface is a collapsed, churned patch of the soil that came out of it.
       Left as the black ellipse the live rig stands in, a finished hole reads
       as a puncture in the picture -- nothing else in this panel is a hole
       through the ground, so the eye takes it for a missing pixel.

       Flat, deliberately: no rim, no lip, no shadow. The rim already exists a
       few cells out where the spoil is, and a second raised thing at the
       centre of it turns one landform into two competing ones. */
    void DrawCrumbs(float cx, float cy, float k, float alpha)
    {
        const float rw = 11.5f * DRILL_S * (0.62f + 0.38f * k), rh = rw * 0.42f;
        const Color base = SURVEY_BED_PALETTE[0].neon;
        DrawEllipse(static_cast<int>(cx), static_cast<int>(cy), rw, rh,
                    Fade2(Scale(base, 1.04f), 0.22f * alpha));
        const int n = 22 + static_cast<int>(std::lround(26.0f * k));
        for (int i = 0; i < n; i++)
        {
            const float r = std::pow(Rnd(), 0.62f) * 1.22f, t = Rnd() * 2.0f * PI;
            if (r > 1.0f && Rnd() > 0.45f) continue;
            const float x = cx + rw * r * std::cos(t), y = cy + rh * r * std::sin(t);
            const float tone = Rnd();
            const Color c = tone > 0.74f ? Scale(base, 1.52f)
                          : tone > 0.34f ? Scale(base, 1.12f) : Scale(base, 0.55f);
            const float w = 1.1f + Rnd() * 1.6f;
            Rect(x - w * 0.5f, y - w * 0.4f, w, w * 0.85f, Fade2(c, 0.8f * alpha));
        }
    }

    Color BedColour(const SurveyGround& g, float m)
    {
        for (int k = SURVEY_BEDS - 1; k >= 0; k--)
            if (m >= g.EdgeM(k)) return SURVEY_BED_PALETTE[k].neon;
        return SURVEY_BED_PALETTE[0].neon;
    }
}

float SurveyRigDraw::PixelsPerMetre(const SurveyCamera& cam, float columnM)
{
    return cam.PixelsPerMetre(SURVEY_MODEL_D, columnM);
}

float SurveyRigDraw::SpudMetres(float pixelsPerMetre)
{
    return BIT_PX / std::max(pixelsPerMetre, 0.01f);
}

/* Ejecta leaves the collar on a ballistic arc, in lattice units so it lands on
   the ground it came out of, and it wears the colour of the bed the bit is in
   RIGHT NOW -- which makes the first thing you learn about the column a thing
   you see thrown out of it. */
static void SpawnSpatter(SurveyRig& rig, const SurveyGround& ground,
                         float pxPerM, float tx, float i, float j, float m, int n)
{
    const Color col = BedColour(ground, std::min(std::max(m, 0.0f), ground.ColumnM() - 0.01f));
    for (int k = 0; k < n; k++)
    {
        /* A cone, not a fountain. The launch speed is chosen in SCREEN pixels
           and only then converted into the two very different units the block
           moves things in -- lattice cells across, metres of depth up --
           because it is the picture that has to read as a spray. */
        const float az = static_cast<float>(GetRandomValue(0, 6283)) / 1000.0f;
        const float el = 0.72f + static_cast<float>(GetRandomValue(0, 460)) / 1000.0f;
        const float S = 78.0f + static_cast<float>(GetRandomValue(0, 740)) / 10.0f;
        const float up = S * std::sin(el), out = S * std::cos(el);
        SurveyEjecta e;
        e.i = i; e.j = j; e.z = 0.6f;
        e.vi = std::cos(az) * out / std::max(tx, 0.01f);
        e.vj = std::sin(az) * out / std::max(tx, 0.01f);
        e.vz = up / std::max(pxPerM, 0.01f);
        e.g = EJECTA_G_PX / std::max(pxPerM, 0.01f);
        e.col = col;
        e.size = 1.8f + static_cast<float>(GetRandomValue(0, 240)) / 100.0f;
        rig.spatter.push_back(e);
    }
}

void SurveyRigDraw::Step(SurveyConsole& console, const SurveyCamera& cam, float dt)
{
    SurveyRig& rig = console.Rig();
    const SurveyGround& ground = console.Ground();
    const int N = ground.Lattice();
    rig.clock += dt;
    if (rig.lockFlash > 0.0f) rig.lockFlash = std::max(0.0f, rig.lockFlash - dt);

    const float pxPerM = PixelsPerMetre(cam, ground.ColumnM());
    const float tx = SURVEY_MODEL_W / N * cam.zoom;
    const float sm = SpudMetres(pxPerM);

    if (rig.mode == RigMode::AIM)
    {
        rig.phase += 2.2f * dt;
    }
    else
    {
        rig.t += dt;
        // Spinning down: cutting, then tripping, then idling in the hole.
        rig.phase += (rig.mode == RigMode::SPUD || rig.mode == RigMode::CUT ? 13.5f
                    : rig.mode == RigMode::OUT ? 5.0f
                    : rig.mode == RigMode::HOLD ? 0.7f : 0.0f) * dt;

        if (rig.mode == RigMode::SPUD)
        {
            rig.spud = Clamp01(rig.t / SPUD_T);
            rig.curM = sm * rig.spud;
            console.SetScour(rig.liveScour, static_cast<float>(rig.i), static_cast<float>(rig.j),
                             0.55f * rig.spud);
            if (GetRandomValue(0, 99) < 90)
                SpawnSpatter(rig, ground, pxPerM, tx, static_cast<float>(rig.i),
                             static_cast<float>(rig.j), rig.curM, 2 + GetRandomValue(0, 2));
            if (rig.t >= SPUD_T)
            {
                rig.spud = 1.0f; rig.curM = sm; rig.mode = RigMode::AWAIT; rig.t = 0.0f;
            }
        }
        else if (rig.mode == RigMode::CUT)
        {
            rig.sink = rig.externalCut >= 0.0f ? Clamp01(rig.externalCut)
                                               : Clamp01(rig.t / rig.cutT);
            rig.curM = Lerp(sm, rig.depthM, rig.sink);
            rig.feed += 40.0f * dt;
            // A deeper hole leaves a bigger scar: the spoil is what came out.
            console.SetScour(rig.liveScour, static_cast<float>(rig.i), static_cast<float>(rig.j),
                             0.55f + 0.45f * Clamp01(rig.depthM / std::max(ground.ColumnM(), 1.0f))
                                     * rig.sink);
            if (GetRandomValue(0, 99) < 50)
                SpawnSpatter(rig, ground, pxPerM, tx, static_cast<float>(rig.i),
                             static_cast<float>(rig.j), rig.curM, 1 + GetRandomValue(0, 1));
            // Free-running only: when the real hole is driving, the caller
            // says when the string starts coming back up.
            if (rig.externalCut < 0.0f && rig.t >= rig.cutT)
            {
                rig.sink = 1.0f; rig.sink0 = rig.sink; rig.mode = RigMode::OUT; rig.t = 0.0f;
            }
        }
        else if (rig.mode == RigMode::OUT)
        {
            /* The handle does not come up with it. It is the collar: it stays
               on the ground while the string is drawn back up THROUGH it,
               exactly the way it went down -- so what rises is the tail. */
            const float u = Clamp01(rig.t / OUT_T);
            rig.sink = rig.sink0 * (1.0f - u);
            rig.curM = Lerp(sm, rig.depthM, rig.sink);
            rig.feed -= 40.0f * dt;
            if (rig.t >= OUT_T)
            {
                rig.sink = 0.0f; rig.curM = sm;
                /* The column is real NOW, and only down to where the bit got.
                   The string clearing the collar is the moment you know
                   something: not when you aimed, and not one metre deeper
                   than you drilled. */
                console.RecordHole(static_cast<float>(rig.i), static_cast<float>(rig.j),
                                   rig.depthM);
                rig.mode = RigMode::HOLD; rig.t = 0.0f;
            }
        }
        else if (rig.mode == RigMode::HOLD)
        {
            /* A beat, with the machine standing in the hole it just finished.
               The column resolving and the rig disappearing are two separate
               events, and run on the same frame neither of them registers. */
            if (rig.t >= HOLD_T)
            {
                rig.bores.push_back({ static_cast<float>(rig.i), static_cast<float>(rig.j),
                                      rig.depthM });
                rig.mode = RigMode::FADE; rig.t = 0.0f; rig.fade = 0.0f;
            }
        }
        else if (rig.mode == RigMode::FADE)
        {
            rig.fade = Clamp01(rig.t / FADE_T);
            if (rig.t >= FADE_T)
            {
                rig.mode = RigMode::AIM; rig.t = 0.0f;
                rig.spud = rig.sink = rig.feed = rig.fade = 0.0f;
                rig.curM = 0.0f; rig.liveScour = -1;
            }
        }
    }

    // the spatter
    for (int k = static_cast<int>(rig.spatter.size()) - 1; k >= 0; k--)
    {
        SurveyEjecta& p = rig.spatter[k];
        p.i += p.vi * dt; p.j += p.vj * dt;
        p.vz -= p.g * dt; p.z += p.vz * dt;
        if (p.z <= 0.0f || p.i < -1.0f || p.j < -1.0f || p.i > N + 1 || p.j > N + 1)
            rig.spatter.erase(rig.spatter.begin() + k);
    }
    if (rig.spatter.size() > 420) rig.spatter.erase(rig.spatter.begin(),
                                                    rig.spatter.begin() + (rig.spatter.size() - 420));
}

/* Where the tip is, and where the collar it stands in is. The whole assembly
   descends by `sink`: the tail keeps its length and goes down with the handle,
   which is what buys the depth. At the bottom of its travel a stub of tail is
   still standing out of the ground, and that stub is what the bore marker
   replaces when the rig leaves. */
namespace
{
    struct RigPose
    {
        float cx, tipY, collarY, head, tail;
        bool live;
    };

    RigPose PoseOf(const SurveyRig& rig, const SurveyGround& ground,
                   const SurveyBlockState& block, const SurveyCamera& cam)
    {
        if (rig.mode == RigMode::AIM)
            return { rig.pointer.x, rig.pointer.y, rig.pointer.y + 6.0f, 0.0f, TAIL_PX, false };
        const Vector2 g = SurveyBlock::ProjectSurface(ground, cam, block,
                                                      static_cast<float>(rig.i),
                                                      static_cast<float>(rig.j));
        const float sink = rig.sink * SINK_MAX;
        return { g.x, g.y + rig.spud * BIT_PX + sink, g.y + 2.0f,
                 // Spudding in, the handle rides down with the bit until it
                 // lands. After that it stays put and the string runs through.
                 g.y + (rig.spud - 1.0f) * BIT_PX,
                 std::max(TAIL_PX - sink, TAIL_STUB), true };
    }

    /* The rig fades through ONE composite rather than fill by fill -- a rig
       faded part by part shows its own tail through its own head. Sized to the
       block's region and only ever used during the 0.3 s fade, so the render
       texture is built the first time a hole finishes and not before. */
    RenderTexture2D fadeBuf = { 0 };
    void EnsureFadeBuf(Rectangle r)
    {
        const int w = static_cast<int>(r.width), h = static_cast<int>(r.height);
        if (fadeBuf.id != 0 && fadeBuf.texture.width == w && fadeBuf.texture.height == h) return;
        if (fadeBuf.id != 0) UnloadRenderTexture(fadeBuf);
        fadeBuf = LoadRenderTexture(w, h);
    }
}

void SurveyRigDraw::Draw(const SurveyConsole& console, const SurveyCamera& cam,
                         Rectangle blockRect)
{
    const SurveyRig& rig = console.Rig();
    const SurveyGround& ground = console.Ground();
    const SurveyBlockState& block = console.Block();
    const int N = ground.Lattice();
    const float columnM = ground.ColumnM();

    /* THE HOLE MOUTHS, drawn with the ground so the block accumulates a record
       of where you have been. The live one is skipped while the rig is
       standing in it -- what you see there is the collar, not the churn. */
    for (size_t k = 0; k < console.Scours().size(); k++)
    {
        const SurveyScour& s = console.Scours()[k];
        if (s.progress < 0.05f) continue;
        float a = 1.0f;
        const bool isLive = (static_cast<int>(k) == rig.liveScour);
        if (rig.mode != RigMode::AIM && isLive)
        {
            if (rig.mode != RigMode::FADE) continue;    // the rig is standing in it
            a = rig.fade;                               // and now it is leaving
        }
        grainSeed = 1u + static_cast<uint32_t>(s.i) * 7919u + static_cast<uint32_t>(s.j) * 104729u;
        const Vector2 g = SurveyBlock::ProjectSurface(ground, cam, block, s.i, s.j);
        DrawCrumbs(g.x, g.y + 2.0f, s.progress, a);
    }

    /* THE BORE MARKERS. What the rig leaves behind: a turning wireframe core
       barrel standing in the hole, as deep as the hole went. */
    for (size_t k = 0; k < rig.bores.size(); k++)
    {
        const SurveyBore& b = rig.bores[k];
        const float alpha = (rig.mode == RigMode::FADE && k + 1 == rig.bores.size())
                          ? rig.fade : 1.0f;
        const Vector2 g = SurveyBlock::ProjectSurface(ground, cam, block, b.i, b.j);
        const float gy = g.y + 2.0f;
        const float deep = Clamp01(b.depthM / std::max(columnM, 1.0f));
        const float top = gy - (16.0f + 23.0f * deep);
        const float rw = 4.4f, rh = rw * 0.40f;
        const float ph = rig.clock * 2.4f + b.i * 0.7f + b.j * 0.31f;
        for (int s = 0; s < 3; s++)
        {
            const float a = ph + s * 2.0944f;
            const float x = g.x + rw * std::sin(a), c = std::cos(a);
            DrawLineEx({x, top + rh * c}, {x, gy + rh * c}, c > 0.0f ? 1.5f : 1.1f,
                       Fade2(G_CY, (c > 0.0f ? 0.80f : 0.24f) * alpha));
        }
        DrawEllipseLines(static_cast<int>(g.x), static_cast<int>(top), rw, rh,
                         Fade2(G_CY, 0.62f * alpha));
        DrawEllipseLines(static_cast<int>(g.x), static_cast<int>(gy), rw, rh,
                         Fade2(G_CY, 0.28f * alpha));
        DrawEllipse(static_cast<int>(g.x), static_cast<int>(top), rw, rh,
                    Fade2(G_CY, 0.16f * alpha));
    }

    if (!rig.armed) return;

    if (rig.mode == RigMode::AIM)
    {
        // The cursor IS the glyph, at the icon's own size, and a spot that
        // will be refused says so with the glyph rather than by vanishing.
        if (!rig.onCanvas) return;
        DrawDrillGlyph(rig.pointer.x, rig.pointer.y, rig.canPlace ? 1 : 2, rig.clock);
        return;
    }

    const RigPose p = PoseOf(rig, ground, block, cam);
    const bool lit = rig.mode == RigMode::SPUD || rig.mode == RigMode::CUT;
    auto paint = [&](float ox, float oy) {
        DrawCollar(p.cx - ox, p.collarY - oy, 1.0f);
        // The cursor's ring, still round the tool it was carried by, still
        // turning the same way -- the clearest thing tying the planted machine
        // to the one that was in hand a moment ago.
        DrawGlyphRing(std::round(p.cx - ox), std::round(p.collarY - oy),
                      -rig.clock * 1.9f, Fade2(G_CY, 0.7f), GLY_U);
        BeginScissorMode(static_cast<int>(p.cx - ox - 46.0f * DRILL_S),
                         static_cast<int>(blockRect.y - oy),
                         static_cast<int>(92.0f * DRILL_S),
                         static_cast<int>(p.collarY - blockRect.y));
        DrawRigBody(p.cx - ox, p.tipY - oy, lit, p.tail, rig.feed, p.head - oy, rig.clock);
        EndScissorMode();
    };

    if (rig.mode == RigMode::FADE)
    {
        EnsureFadeBuf(blockRect);
        if (fadeBuf.id != 0)
        {
            BeginTextureMode(fadeBuf);
            ClearBackground(BLANK);
            paint(blockRect.x, blockRect.y);
            EndTextureMode();
            // Render textures come back y-flipped, hence the negative height.
            DrawTextureRec(fadeBuf.texture,
                           { 0.0f, 0.0f, blockRect.width, -blockRect.height },
                           { blockRect.x, blockRect.y }, Fade2(WHITE, 1.0f - rig.fade));
        }
    }
    else
    {
        paint(0.0f, 0.0f);
    }

    /* THE SPATTER LAST, over the rig that threw it. */
    for (const SurveyEjecta& e : rig.spatter)
    {
        const float ci = std::min(std::max(e.i, 0.0f), static_cast<float>(N));
        const float cj = std::min(std::max(e.j, 0.0f), static_cast<float>(N));
        const float surf = ground.SampleDepth(0, ci / N, cj / N);
        const float u = e.i / N, v = e.j / N;
        const float x = (u - 0.5f) * SURVEY_MODEL_W, z = (v - 0.5f) * SURVEY_MODEL_W;
        const float y = -((surf - e.z) / columnM) * SURVEY_MODEL_D;
        const Vector2 q = cam.Project(x, y, z);
        Rect(q.x, q.y, e.size, e.size * 0.8f, Scale(e.col, e.z > 3.0f ? 1.9f : 1.3f));
    }
}

/* A click on the block while the rig is out. Two decisions, as they have
   always been: where it stands, then how deep it goes.

   THE DEPTH CONTROL IS PROVISIONAL, exactly as it is in the prototype: it
   belongs to the borehole bar, which is the next stage of this port. Until
   then a second click cuts the full column, so the feed, the tail, the scour
   and the trip-out can all be watched. */
bool SurveyRigDraw::Click(SurveyConsole& console, const SurveyCamera& cam, Vector2 point)
{
    SurveyRig& rig = console.Rig();
    const SurveyGround& ground = console.Ground();
    const float columnM = ground.ColumnM();
    const float sm = SpudMetres(PixelsPerMetre(cam, columnM));

    if (rig.mode == RigMode::AWAIT)
    {
        rig.depthM = columnM;
        rig.cutT = 0.7f + 2.3f * (rig.depthM - sm) / std::max(columnM - sm, 1.0f);
        rig.mode = RigMode::CUT; rig.t = 0.0f; rig.sink = 0.0f; rig.feed = 0.0f;
        return true;
    }
    if (rig.mode != RigMode::AIM) return false;      // one hole at a time
    if (!rig.canPlace) { rig.lockFlash = 1.0f; return true; }

    float pi = 0.0f, pj = 0.0f;
    if (!SurveyBlock::PickGround(ground, console.Block(), cam, point, pi, pj)) return false;
    rig.i = static_cast<int>(std::lround(pi));
    rig.j = static_cast<int>(std::lround(pj));
    rig.mode = RigMode::SPUD; rig.t = 0.0f;
    rig.spud = rig.sink = rig.feed = 0.0f;
    rig.depthM = sm;
    rig.liveScour = console.AddScour(static_cast<float>(rig.i), static_cast<float>(rig.j));
    return true;
}

/* Backing out of a planted rig. It does NOT throw the hole away: the ground is
   already broken and the spoil is already on it, so the shallow column it did
   establish is what you keep. A control that can leave the panel in a state
   with no way forward is worse than one that costs you a bad hole. */
void SurveyRigDraw::BeginCut(SurveyConsole& console, const SurveyCamera& cam, float depthM)
{
    SurveyRig& rig = console.Rig();
    if (rig.mode != RigMode::AWAIT) return;
    const float sm = SpudMetres(PixelsPerMetre(cam, console.Ground().ColumnM()));
    rig.depthM = std::max(depthM, sm);
    rig.cutT = 0.7f + 2.3f * (rig.depthM - sm)
             / std::max(console.Ground().ColumnM() - sm, 1.0f);
    rig.mode = RigMode::CUT; rig.t = 0.0f; rig.sink = 0.0f; rig.feed = 0.0f;
}

void SurveyRigDraw::BeginOut(SurveyConsole& console)
{
    SurveyRig& rig = console.Rig();
    if (rig.mode != RigMode::CUT) return;
    rig.sink0 = rig.sink;
    rig.mode = RigMode::OUT; rig.t = 0.0f;
}

void SurveyRigDraw::Abort(SurveyConsole& console, const SurveyCamera& cam)
{
    SurveyRig& rig = console.Rig();
    if (rig.mode != RigMode::AWAIT && rig.mode != RigMode::SPUD) return;
    const float sm = SpudMetres(PixelsPerMetre(cam, console.Ground().ColumnM()));
    rig.depthM = sm * std::max(rig.spud, 0.35f);
    rig.sink0 = rig.sink;
    rig.mode = RigMode::OUT; rig.t = 0.0f;
}
