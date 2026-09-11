#include "survey_block.h"

#include "rlgl.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace
{
    constexpr float GHOST_ALPHA = 0.30f;
    constexpr float KN_MIN = 0.012f;      // below this a quad is not worth a draw call

    struct Wall
    {
        char  name;
        float nx, nz;
        int   nodeI0, nodeJ0;   // node(t) = (nodeI0 + di*t, nodeJ0 + dj*t)
        int   di, dj;
    };

    // The order and direction are the prototype's, so nothing downstream shifts.
    const Wall WALLS[4] =
    {
        { 'A',  0.0f, -1.0f, 0, 0, 1, 0 },
        { 'B',  1.0f,  0.0f, 1, 0, 0, 1 },   // nodeI0 scaled to N below
        { 'C',  0.0f,  1.0f, 0, 1, 1, 0 },
        { 'D', -1.0f,  0.0f, 0, 0, 0, 1 },
    };

    void WallNode(const Wall& w, int t, int n, int& i, int& j)
    {
        i = (w.nodeI0 != 0 ? n : 0) + w.di * t;
        j = (w.nodeJ0 != 0 ? n : 0) + w.dj * t;
    }

    float Hash3(int a, int b, int c)
    {
        uint32_t h = static_cast<uint32_t>(a * 73856093) ^ static_cast<uint32_t>(b * 19349663)
                   ^ static_cast<uint32_t>(c * 83492791);
        h = (h ^ (h >> 13)) * 1274126177u;
        return static_cast<float>((h ^ (h >> 16)) / 4294967296.0);
    }

    Vector2 Mix2(Vector2 a, Vector2 b, float t)
    {
        return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t };
    }

    Color Shade(Color c, float f, float alpha)
    {
        auto ch = [f](unsigned char v) {
            const float r = v * f;
            return static_cast<unsigned char>(r > 255.0f ? 255.0f : r);
        };
        const float a = alpha < 0.0f ? 0.0f : (alpha > 1.0f ? 1.0f : alpha);
        return { ch(c.r), ch(c.g), ch(c.b), static_cast<unsigned char>(a * 255.0f) };
    }

    Color MixColor(Color a, Color b, float t, float alpha)
    {
        auto ch = [t](unsigned char x, unsigned char y) {
            return static_cast<unsigned char>(x + (y - x) * t);
        };
        const float al = alpha < 0.0f ? 0.0f : (alpha > 1.0f ? 1.0f : alpha);
        return { ch(a.r, b.r), ch(a.g, b.g), ch(a.b, b.b),
                 static_cast<unsigned char>(al * 255.0f) };
    }

    /* The wall's vertical gradient: neon at the bedding plane above, through
       mid, into deep for the bottom quarter. Sampled per VERTEX, which is how
       the fog ramp and the shading end up being one interpolation. */
    Color WallTone(const SurveyBedPalette& bed, float t, float lit, float alpha)
    {
        if (t <= 0.20f) return MixColor(Shade(bed.neon, lit, 1.0f), Shade(bed.mid, lit, 1.0f),
                                        t / 0.20f, alpha);
        if (t <= 0.75f) return MixColor(Shade(bed.mid, lit, 1.0f), Shade(bed.deep, lit, 1.0f),
                                        (t - 0.20f) / 0.55f, alpha);
        return Shade(bed.deep, lit, alpha);
    }

    void FillQuad(Vector2 a, Vector2 b, Vector2 c, Vector2 d,
                  Color ca, Color cb, Color cc, Color cd)
    {
        rlBegin(RL_TRIANGLES);
        rlColor4ub(ca.r, ca.g, ca.b, ca.a); rlVertex2f(a.x, a.y);
        rlColor4ub(cb.r, cb.g, cb.b, cb.a); rlVertex2f(b.x, b.y);
        rlColor4ub(cc.r, cc.g, cc.b, cc.a); rlVertex2f(c.x, c.y);
        rlColor4ub(ca.r, ca.g, ca.b, ca.a); rlVertex2f(a.x, a.y);
        rlColor4ub(cc.r, cc.g, cc.b, cc.a); rlVertex2f(c.x, c.y);
        rlColor4ub(cd.r, cd.g, cd.b, cd.a); rlVertex2f(d.x, d.y);
        rlEnd();
    }

    void FillQuadFlat(Vector2 a, Vector2 b, Vector2 c, Vector2 d, Color col)
    {
        FillQuad(a, b, c, d, col, col, col, col);
    }

    /* Canvas gets its bloom from shadowBlur, which raylib has no equivalent
       for. A wider, dimmer line under the bright one reads the same at these
       widths and costs one more draw call. */
    void GlowLine(Vector2 a, Vector2 b, float width, Color col, float glow)
    {
        if (glow > 0.0f)
        {
            DrawLineEx(a, b, width + glow * 0.30f, Fade(SURVEY_BASE_RING, 0.10f * col.a / 255.0f));
            DrawLineEx(a, b, width + glow * 0.12f, Fade(SURVEY_BASE_RING, 0.16f * col.a / 255.0f));
        }
        DrawLineEx(a, b, width, col);
    }

    bool PointInPoly(const std::vector<Vector2>& poly, Vector2 p)
    {
        bool inside = false;
        for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++)
        {
            if ((poly[i].y > p.y) != (poly[j].y > p.y) &&
                p.x < (poly[j].x - poly[i].x) * (p.y - poly[i].y) / (poly[j].y - poly[i].y) + poly[i].x)
            {
                inside = !inside;
            }
        }
        return inside;
    }
}

void SurveyBlockState::Step(float dt)
{
    time += dt;
    explode += (explodeTarget - explode) * std::min(1.0f, dt * 7.0f);
    if (std::fabs(explodeTarget - explode) < 0.002f) explode = explodeTarget;
}

void SurveyBlockState::Select(int bed)
{
    if (bed >= 0 && bed != selected) { selected = bed; explodeTarget = 1.0f; }
    else { selected = -1; explodeTarget = 0.0f; }
}

SurveyCamera SurveyBlock::MakeCamera(const SurveyBlockState& state,
                                     const SurveyBlockPlacement& place)
{
    return SurveyCamera::Make(state.yaw, state.pitch, place.zoom,
                              place.cx, place.cy, -SURVEY_MODEL_D * 0.5f);
}

namespace
{
    // A lattice node on an interface, projected, with the bed's explode offset.
    Vector2 ProjectNode(const SurveyGround& g, const SurveyCamera& cam,
                        int interfaceIndex, int i, int j, float offY)
    {
        const int n = g.Lattice();
        const float x = (static_cast<float>(i) / n - 0.5f) * SURVEY_MODEL_W;
        const float z = (static_cast<float>(j) / n - 0.5f) * SURVEY_MODEL_W;
        const float y = -(g.DepthAt(interfaceIndex, i, j) / g.ColumnM()) * SURVEY_MODEL_D + offY;
        return cam.Project(x, y, z);
    }

    float BedOffsetY(int bed, float explode)
    {
        return (2.0f - bed) * (SURVEY_MODEL_D * 0.16f * explode);
    }

    float LitWall(const Wall& w)
    {
        const float d = w.nx * SURVEY_LIGHT[0] + w.nz * SURVEY_LIGHT[2];
        return 0.62f + 0.38f * std::max(0.0f, d);
    }
}

/* =====================================================================
   THE WIRE CAGE -- what is drawn where nothing has been measured
   ---------------------------------------------------------------------
   Unknown volume gets no fill and no boundaries. What is left is the
   grid the instrument works ON: a column line every few lattice nodes,
   a depth ring every 250 m, and a slow crawl through the rings, so an
   undrilled block reads as LIVE BUT UNRESOLVED rather than as a panel
   that failed to load. The surface stays where it is, because you can
   see the ground -- which is why an undrilled block is the real terrain
   standing on a wire volume and not a bare box.

   Alpha is exactly 1 - confidence, so it retreats from the parts that
   have been drilled and is gone once the block is MEASURED. It fades
   out with explode: the cage is the shape of one solid volume, and a
   block taken apart is not one.
   ===================================================================== */
void SurveyBlock::DrawCage(const SurveyGround& ground, const SurveyKnowledge& knowledge,
                           const SurveyBlockState& state, const SurveyCamera& cam)
{
    const float fade = 1.0f - std::min(1.0f, state.explode * 1.6f);
    if (fade <= 0.01f) return;

    const int n = ground.Lattice();
    const float columnM = ground.ColumnM();
    // Stepped, not run: on an instrument this reads as data arriving rather
    // than as judder, and between steps the panel is free.
    const float crawl = std::floor(state.time * SURVEY_CRAWL_HZ) / SURVEY_CRAWL_HZ;

    std::vector<int> ticks;
    for (int t = 0; t <= n; t += SURVEY_CAGE_STEP) ticks.push_back(t);
    if (ticks.back() != n) ticks.push_back(n);

    for (const Wall& w : WALLS)
    {
        if (!cam.Facing(w.nx, w.nz)) continue;

        // columns: from the ground you can see, down to the base of the survey
        for (int t : ticks)
        {
            int i, j; WallNode(w, t, n, i, j);
            const float a = SURVEY_CAGE_ALPHA * fade *
                (1.0f - SurveyKnowledge::Confidence(knowledge.KnowAt(i, j, columnM * 0.5f)));
            if (a <= 0.006f) continue;
            const Vector2 top = ProjectNode(ground, cam, 0, i, j, 0.0f);
            const float x = (static_cast<float>(i) / n - 0.5f) * SURVEY_MODEL_W;
            const float z = (static_cast<float>(j) / n - 0.5f) * SURVEY_MODEL_W;
            const Vector2 bottom = cam.Project(x, -SURVEY_MODEL_D, z);
            DrawLineEx(top, bottom, 1.0f, Fade(SURVEY_CAGE, a));
        }

        // rings: one crawling wave of depth, the cheapest honest sign of life
        for (float m = SURVEY_CAGE_RING_M; m <= columnM + 1.0f; m += SURVEY_CAGE_RING_M)
        {
            const float depth = std::min(m, columnM);
            const float y = -(depth / columnM) * SURVEY_MODEL_D;
            const float wave = 0.55f + 0.45f * std::sin(m / 170.0f - crawl * 1.6f);
            for (size_t s = 0; s + 1 < ticks.size(); s++)
            {
                int i0, j0, i1, j1;
                WallNode(w, ticks[s], n, i0, j0);
                WallNode(w, ticks[s + 1], n, i1, j1);
                const float a = SURVEY_CAGE_ALPHA * fade * wave *
                    (1.0f - SurveyKnowledge::Confidence(
                        knowledge.KnowAt((i0 + i1) * 0.5f, (j0 + j1) * 0.5f, depth)));
                if (a <= 0.006f) continue;
                const float x0 = (static_cast<float>(i0) / n - 0.5f) * SURVEY_MODEL_W;
                const float z0 = (static_cast<float>(j0) / n - 0.5f) * SURVEY_MODEL_W;
                const float x1 = (static_cast<float>(i1) / n - 0.5f) * SURVEY_MODEL_W;
                const float z1 = (static_cast<float>(j1) / n - 0.5f) * SURVEY_MODEL_W;
                DrawLineEx(cam.Project(x0, y, z0), cam.Project(x1, y, z1),
                           1.0f, Fade(SURVEY_CAGE, a));
            }
        }
    }
}

void SurveyBlock::DrawBeds(const SurveyGround& ground, const SurveyKnowledge& knowledge,
                           SurveyBlockState& state, const SurveyCamera& cam)
{
    const int n = ground.Lattice();
    const float columnM = ground.ColumnM();
    const bool exploded = state.explode > 0.02f;
    state.hits.clear();

    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    auto note = [&](Vector2 p) {
        minX = std::min(minX, p.x); minY = std::min(minY, p.y);
        maxX = std::max(maxX, p.x); maxY = std::max(maxY, p.y);
    };

    // Knowledge, at the depth the thing being drawn is a picture of. The
    // SURFACE is known for free, because you can see it.
    auto ifaceKnow = [&](int L, int i, int j) {
        return L == 0 ? 1.0f
                      : SurveyKnowledge::Confidence(knowledge.KnowAt(i, j, ground.EdgeM(L)));
    };
    auto bedKnow = [&](int k, int i, int j) {
        return SurveyKnowledge::Confidence(
            knowledge.KnowAt(i, j, (ground.EdgeM(k) + ground.EdgeM(k + 1)) * 0.5f));
    };

    // Isolate ghosts every bed but the focus. The prototype composites the
    // ghosts through offscreen buffers so stacked ghosts cannot accumulate;
    // here they are flat per-bed alpha, which is the documented fallback and
    // what the isolate stage of this port will revisit.
    auto bedAlpha = [&](int k) {
        return (state.selected < 0 || k == state.selected) ? 1.0f : GHOST_ALPHA;
    };

    /* rlgl QUEUES vertices and draws them at the next flush, but the cull
       state is set on the GL context the moment it is called. Disabling
       culling, queueing the block and re-enabling it therefore draws the
       whole block with culling ON -- which silently ate every wall quad
       whose winding came out the wrong way round while the cap, wound the
       other way, rendered perfectly. Found by rendering, not by reading:
       the geometry, the mesh and the boundary lines were all in the right
       place and only the fills were missing.

       So the batch is flushed on both sides of the state change. */
    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();

    // Painted back to front: with no depth buffer, the order IS the depth test.
    for (int k = SURVEY_BEDS - 1; k >= 0; k--)
    {
        const SurveyBedPalette& bed = SURVEY_BED_PALETTE[k];
        const float offY = BedOffsetY(k, state.explode);
        const float ghost = bedAlpha(k);
        const bool showTop = (k == 0) || exploded;

        // ---- walls ----
        for (const Wall& w : WALLS)
        {
            const int count = n;
            std::vector<Vector2> top(count + 1), bottom(count + 1);
            std::vector<float> known(count + 1);
            for (int t = 0; t <= count; t++)
            {
                int i, j; WallNode(w, t, n, i, j);
                top[t] = ProjectNode(ground, cam, k, i, j, offY);
                bottom[t] = ProjectNode(ground, cam, k + 1, i, j, offY);
                known[t] = bedKnow(k, i, j);
                note(top[t]);
                if (k == SURVEY_BEDS - 1) note(bottom[t]);
            }
            if (!cam.Facing(w.nx, w.nz)) continue;

            const float lit = LitWall(w);
            float y0 = 1e9f, y1 = -1e9f;
            for (int t = 0; t <= count; t++)
            {
                y0 = std::min({y0, top[t].y, bottom[t].y});
                y1 = std::max({y1, top[t].y, bottom[t].y});
            }
            const float span = std::max(1.0f, y1 - y0);
            const int rows = std::max(2, static_cast<int>(std::lround(span / 26.0f)));

            float best = 0.0f;
            for (float v : known) best = std::max(best, v);
            /* A bed you cannot see is a bed you cannot lift out, so the hit
               polygon goes in only where there is something on the wall. */
            if (best > 0.25f)
            {
                SurveyBlockState::HitPoly hp;
                hp.bed = k;
                hp.poly.reserve((count + 1) * 2);
                for (int t = 0; t <= count; t++) hp.poly.push_back(top[t]);
                for (int t = count; t >= 0; t--) hp.poly.push_back(bottom[t]);
                state.hits.push_back(std::move(hp));
            }
            if (best <= KN_MIN) continue;

            /* One quad per lattice column per mesh row, coloured at the
               corners. The vertical gradient and the horizontal knowledge
               ramp are the same interpolation, which is the whole reason
               this reads better here than it does on a canvas. */
            /* The gradient is sampled at the quad CORNERS, so the fill needs
               finer rows than the mesh does: at two rows the vertices land on
               0, 0.5 and 1 and the neon stop at 0.2 is stepped straight over,
               which turns a lit bedding plane into a flat band. */
            const int fillRows = std::max(8, rows * 2);
            for (int t = 0; t < count; t++)
            {
                const float aL = known[t], aR = known[t + 1];
                if (aL <= KN_MIN && aR <= KN_MIN) continue;
                for (int m = 0; m < fillRows; m++)
                {
                    const float q0 = static_cast<float>(m) / fillRows;
                    const float q1 = static_cast<float>(m + 1) / fillRows;
                    const Vector2 p0 = Mix2(top[t], bottom[t], q0);
                    const Vector2 p1 = Mix2(top[t + 1], bottom[t + 1], q0);
                    const Vector2 p2 = Mix2(top[t + 1], bottom[t + 1], q1);
                    const Vector2 p3 = Mix2(top[t], bottom[t], q1);
                    FillQuad(p0, p1, p2, p3,
                             WallTone(bed, (p0.y - y0) / span, lit, aL * ghost),
                             WallTone(bed, (p1.y - y0) / span, lit, aR * ghost),
                             WallTone(bed, (p2.y - y0) / span, lit, aR * ghost),
                             WallTone(bed, (p3.y - y0) / span, lit, aL * ghost));
                }
            }

            // mesh: the column lines and the depth rows inside the wall
            for (int t = 1; t < count; t++)
            {
                const float a = known[t] * ghost;
                if (a <= KN_MIN) continue;
                DrawLineEx(top[t], bottom[t], 1.0f, Fade(bed.mesh, 0.22f * lit * a));
            }
            for (int m = 1; m < rows; m++)
            {
                const float q = static_cast<float>(m) / rows;
                for (int t = 0; t < count; t++)
                {
                    const float a = (known[t] + known[t + 1]) * 0.5f * ghost;
                    if (a <= KN_MIN) continue;
                    DrawLineEx(Mix2(top[t], bottom[t], q), Mix2(top[t + 1], bottom[t + 1], q),
                               1.0f, Fade(bed.mesh, 0.14f * lit * a));
                }
            }

            /* The scatter is the wall's MATERIAL -- without it the beds are
               painted card. Dropped while the camera is being dragged. */
            if (!state.fast)
            {
                for (int t = 0; t < count; t++)
                {
                    const float a = (known[t] + known[t + 1]) * 0.5f * ghost;
                    if (a <= KN_MIN) continue;
                    for (int m = 0; m < rows; m++)
                    {
                        const float h = Hash3(t + 1, m + 7, k * 4 + w.name);
                        if (h > 0.18f) continue;
                        const float a0 = (m + 0.35f) / rows, a1 = (m + 0.65f) / rows;
                        const Vector2 l0 = Mix2(top[t], bottom[t], a0);
                        const Vector2 l1 = Mix2(top[t + 1], bottom[t + 1], a0);
                        const Vector2 l2 = Mix2(top[t + 1], bottom[t + 1], a1);
                        const Vector2 l3 = Mix2(top[t], bottom[t], a1);
                        const Color col = h < 0.14f ? Color{0, 8, 20, static_cast<unsigned char>(0.32f * 255 * a)}
                                                    : Color{200, 235, 255, static_cast<unsigned char>(0.14f * 255 * a)};
                        FillQuadFlat(Mix2(l0, l1, 0.3f), Mix2(l0, l1, 0.7f),
                                     Mix2(l3, l2, 0.7f), Mix2(l3, l2, 0.3f), col);
                    }
                }
            }
        }

        // ---- top surface (slope-shaded cells) ----
        if (showTop)
        {
            for (int i = 0; i < n; i++)
            {
                for (int j = 0; j < n; j++)
                {
                    const float a = (k == 0) ? 1.0f
                        : (ifaceKnow(k, i, j) + ifaceKnow(k, i + 1, j)
                         + ifaceKnow(k, i + 1, j + 1) + ifaceKnow(k, i, j + 1)) * 0.25f;
                    if (a <= KN_MIN) continue;

                    const float cell = SURVEY_MODEL_W / n;
                    const float yA = -(ground.DepthAt(k, i, j) / columnM) * SURVEY_MODEL_D;
                    const float yB = -(ground.DepthAt(k, i + 1, j) / columnM) * SURVEY_MODEL_D;
                    const float yD = -(ground.DepthAt(k, i, j + 1) / columnM) * SURVEY_MODEL_D;
                    // up-facing normal of the cell, from its two edges
                    const float ex[3] = { cell, yB - yA, 0.0f };
                    const float ez[3] = { 0.0f, yD - yA, cell };
                    const float nx = ez[1] * ex[2] - ez[2] * ex[1];
                    const float ny = ez[2] * ex[0] - ez[0] * ex[2];
                    const float nz = ez[0] * ex[1] - ez[1] * ex[0];
                    const float len = std::max(1e-6f, std::sqrt(nx * nx + ny * ny + nz * nz));
                    const float f = 0.45f + 0.55f * std::max(0.0f,
                        (nx * SURVEY_LIGHT[0] + ny * SURVEY_LIGHT[1] + nz * SURVEY_LIGHT[2]) / len);
                    const Color tone = (k == 0)
                        ? MixColor({18, 63, 110, 255}, {63, 146, 208, 255}, f, a * ghost)
                        : MixColor(bed.deep, bed.neon, f * 0.85f, a * ghost);

                    FillQuadFlat(ProjectNode(ground, cam, k, i, j, offY),
                                 ProjectNode(ground, cam, k, i + 1, j, offY),
                                 ProjectNode(ground, cam, k, i + 1, j + 1, offY),
                                 ProjectNode(ground, cam, k, i, j + 1, offY), tone);

                    if (!state.fast && Hash3(i + 3, j + 5, 99 + k) < 0.12f)
                    {
                        const Vector2 c0 = ProjectNode(ground, cam, k, i, j, offY);
                        const Vector2 c1 = ProjectNode(ground, cam, k, i + 1, j, offY);
                        const Vector2 c2 = ProjectNode(ground, cam, k, i + 1, j + 1, offY);
                        const Vector2 c3 = ProjectNode(ground, cam, k, i, j + 1, offY);
                        const Vector2 q0 = Mix2(c0, c1, 0.3f), q1 = Mix2(c0, c1, 0.7f);
                        const Vector2 q2 = Mix2(c3, c2, 0.7f), q3 = Mix2(c3, c2, 0.3f);
                        FillQuadFlat(Mix2(q0, q3, 0.3f), Mix2(q1, q2, 0.3f),
                                     Mix2(q1, q2, 0.7f), Mix2(q0, q3, 0.7f),
                                     Color{0, 10, 30, static_cast<unsigned char>(0.28f * 255 * a * ghost)});
                    }
                }
            }
            // the lattice over the surface, and the outline that is its hit target
            if (k == 0 || ifaceKnow(k, n / 2, n / 2) > KN_MIN)
            {
                const Color grid = Fade(bed.mesh, 0.30f * ghost);
                for (int i = 1; i < n; i++)
                    for (int j = 0; j < n; j++)
                        DrawLineEx(ProjectNode(ground, cam, k, i, j, offY),
                                   ProjectNode(ground, cam, k, i, j + 1, offY), 1.0f, grid);
                for (int j = 1; j < n; j++)
                    for (int i = 0; i < n; i++)
                        DrawLineEx(ProjectNode(ground, cam, k, i, j, offY),
                                   ProjectNode(ground, cam, k, i + 1, j, offY), 1.0f, grid);
            }
            if (k == 0 || ifaceKnow(k, n / 2, n / 2) > 0.25f)
            {
                SurveyBlockState::HitPoly hp;
                hp.bed = k;
                for (int j = 0; j <= n; j++) hp.poly.push_back(ProjectNode(ground, cam, k, 0, j, offY));
                for (int i = 1; i <= n; i++) hp.poly.push_back(ProjectNode(ground, cam, k, i, n, offY));
                for (int j = n - 1; j >= 0; j--) hp.poly.push_back(ProjectNode(ground, cam, k, n, j, offY));
                for (int i = n - 1; i >= 0; i--) hp.poly.push_back(ProjectNode(ground, cam, k, i, 0, offY));
                state.hits.push_back(std::move(hp));
            }
        }

        /* ---- edges ----
           A BOUNDARY IS A CLAIM, and an unmeasured one is not drawn. Each
           segment carries the lesser of its two ends' confidence and fades in
           across it rather than popping. */
        for (const Wall& w : WALLS)
        {
            const bool visible = cam.Facing(w.nx, w.nz);
            if (!visible && !showTop) continue;
            const Color col = visible ? (k == 0 ? SURVEY_RIM : bed.line)
                                      : Fade(bed.mesh, 0.45f);
            const float width = visible ? (k == 0 ? 2.2f : 1.5f) : 1.0f;
            const float glow = visible ? (k == 0 ? 16.0f : 10.0f) : 0.0f;
            for (int t = 0; t < n; t++)
            {
                int i0, j0, i1, j1;
                WallNode(w, t, n, i0, j0);
                WallNode(w, t + 1, n, i1, j1);
                const float a = std::min(ifaceKnow(k, i0, j0), ifaceKnow(k, i1, j1)) * ghost;
                if (a <= 0.02f) continue;
                GlowLine(ProjectNode(ground, cam, k, i0, j0, offY),
                         ProjectNode(ground, cam, k, i1, j1, offY),
                         width, Fade(col, a), state.fast ? 0.0f : glow * a);
            }
            // the base of the surveyed column: a depth somebody chose, and
            // still a claim, so it is gated like any other boundary
            if (k == SURVEY_BEDS - 1 && visible)
            {
                for (int t = 0; t < n; t++)
                {
                    int i0, j0, i1, j1;
                    WallNode(w, t, n, i0, j0);
                    WallNode(w, t + 1, n, i1, j1);
                    const float a = std::min(ifaceKnow(SURVEY_BEDS, i0, j0),
                                             ifaceKnow(SURVEY_BEDS, i1, j1)) * ghost;
                    if (a <= 0.02f) continue;
                    DrawLineEx(ProjectNode(ground, cam, SURVEY_BEDS, i0, j0, offY),
                               ProjectNode(ground, cam, SURVEY_BEDS, i1, j1, offY),
                               1.1f, Fade(Color{160, 190, 215, 255}, 0.6f * a));
                }
            }
        }

        // vertical corner edges: bright where two visible walls meet, dim on
        // the silhouette
        const int corners[4][4] = { {0, 0, 0, 3}, {1, 0, 0, 1}, {1, 1, 1, 2}, {0, 1, 2, 3} };
        for (const auto& c : corners)
        {
            const bool v0 = cam.Facing(WALLS[c[2]].nx, WALLS[c[2]].nz);
            const bool v1 = cam.Facing(WALLS[c[3]].nx, WALLS[c[3]].nz);
            if (!v0 && !v1) continue;
            const int i = c[0] * n, j = c[1] * n;
            const float a = bedKnow(k, i, j) * ghost;
            if (a <= 0.02f) continue;
            const bool bright = v0 && v1;
            GlowLine(ProjectNode(ground, cam, k, i, j, offY),
                     ProjectNode(ground, cam, k + 1, i, j, offY),
                     bright ? 2.2f : 1.1f,
                     Fade(bright ? SURVEY_RIM : Color{180, 215, 240, 255}, (bright ? 1.0f : 0.55f) * a),
                     state.fast ? 0.0f : (bright ? 14.0f : 2.0f) * a);
        }
    }

    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    state.drawnBounds = { minX, minY, maxX - minX, maxY - minY };
}

void SurveyBlock::DrawBaseRing(const SurveyBlockState& state, const SurveyCamera& cam,
                               const SurveyGround& ground)
{
    (void)ground;
    const float s = std::min(1.4f, cam.zoom / 0.94f);
    const float yb = -SURVEY_MODEL_D + BedOffsetY(SURVEY_BEDS - 1, state.explode) - 40.0f;
    const float r = SURVEY_MODEL_W * 0.78f;
    auto ring = [&](float a) { return cam.Project(std::cos(a) * r, yb, std::sin(a) * r); };
    // dashes, drawn as segments -- the block's own silhouette hides the part
    // that would cross it, because the ring is drawn before nothing and after
    // the beds, at the bottom of the stack where the block is not.
    for (int k = 0; k < 72; k++)
    {
        if (k % 2) continue;
        DrawLineEx(ring(k / 72.0f * 2.0f * PI), ring((k + 1) / 72.0f * 2.0f * PI),
                   1.2f, Fade(SURVEY_BASE_RING, 0.35f));
    }
    for (int k = 0; k < 36; k++)
    {
        const float a = k * 2.0f * PI / 36.0f;
        const float r1 = (k % 9 == 0) ? 1.1f : 1.04f;
        DrawLineEx(ring(a), cam.Project(std::cos(a) * r * r1, yb, std::sin(a) * r * r1),
                   1.0f, Fade(SURVEY_BASE_RING, 0.5f));
    }
}

int SurveyBlock::HitBed(const SurveyBlockState& state, Vector2 point)
{
    int any = -1;
    for (int i = static_cast<int>(state.hits.size()) - 1; i >= 0; i--)
    {
        const SurveyBlockState::HitPoly& h = state.hits[i];
        if (!PointInPoly(h.poly, point)) continue;
        // a solid bed under the pointer wins over a ghosted one
        if (state.selected < 0 || h.bed == state.selected) return h.bed;
        if (any < 0) any = h.bed;
    }
    return any;
}

bool SurveyBlock::PickGround(const SurveyGround& ground, const SurveyBlockState& state,
                             const SurveyCamera& cam, Vector2 point,
                             float& outI, float& outJ)
{
    const float offY = BedOffsetY(0, state.explode);
    const float a = (point.x - cam.cx) / cam.zoom;
    const float b = -(point.y - cam.cy) / cam.zoom;

    // Every point projecting to `point` lies on one line, parameterised by h
    // (distance along the view axis). Walk it and take the FIRST crossing.
    auto SignedGap = [&](float h, float& u, float& v, bool& inside) {
        const float x =  a * cam.f1 + h * cam.f0;
        const float z = -a * cam.f0 + h * cam.f1;
        u = x / SURVEY_MODEL_W + 0.5f;
        v = z / SURVEY_MODEL_W + 0.5f;
        inside = (u >= -1e-6f && u <= 1.0f + 1e-6f && v >= -1e-6f && v <= 1.0f + 1e-6f);
        if (!inside) return 0.0f;
        const float yRay = (b - h * cam.sp) / (std::fabs(cam.cp) < 1e-4f ? 1e-4f : cam.cp);
        const float ySurf = -(ground.SampleDepth(0, u, v) / ground.ColumnM()) * SURVEY_MODEL_D
                          + offY - cam.centreY;
        return yRay - ySurf;
    };

    const float reach = SURVEY_MODEL_W * 0.78f;
    const int steps = 80;
    float prevH = -reach, prevGap = 0.0f;
    bool prevInside = false;
    float u = 0.0f, v = 0.0f;
    bool inside = false;
    bool have = false;
    float h0 = 0.0f, h1 = 0.0f;

    for (int s = 0; s <= steps; s++)
    {
        const float h = -reach + (2.0f * reach) * s / steps;
        const float gap = SignedGap(h, u, v, inside);
        if (inside && prevInside && ((gap > 0.0f) != (prevGap > 0.0f)))
        {
            h0 = prevH; h1 = h; have = true; break;
        }
        prevH = h; prevGap = gap; prevInside = inside;
    }
    if (!have) return false;

    for (int it = 0; it < 22; it++)
    {
        const float hm = (h0 + h1) * 0.5f;
        const float g0 = SignedGap(h0, u, v, inside);
        const float gm = SignedGap(hm, u, v, inside);
        if ((gm > 0.0f) == (g0 > 0.0f)) h0 = hm; else h1 = hm;
    }
    SignedGap((h0 + h1) * 0.5f, u, v, inside);
    if (!inside) return false;

    const int n = ground.Lattice();
    outI = std::min(std::max(u, 0.0f), 1.0f) * n;
    outJ = std::min(std::max(v, 0.0f), 1.0f) * n;
    return true;
}

Vector2 SurveyBlock::ProjectSurface(const SurveyGround& ground, const SurveyCamera& cam,
                                    const SurveyBlockState& state, float i, float j)
{
    const int n = ground.Lattice();
    const float u = i / n, v = j / n;
    const float x = (u - 0.5f) * SURVEY_MODEL_W;
    const float z = (v - 0.5f) * SURVEY_MODEL_W;
    const float y = -(ground.SampleDepth(0, u, v) / ground.ColumnM()) * SURVEY_MODEL_D
                  + BedOffsetY(0, state.explode);
    return cam.Project(x, y, z);
}
