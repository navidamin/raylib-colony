#include <catch2/catch_test_macros.hpp>
#include "block_pick.h"
#include <cmath>

// The block model draws its plates LIFTED by believed grade. Picking has to
// invert that same projection, or the cursor selects a different block from
// the one it is over -- invisible on a flat plate, glaring on a rich one,
// and worse since the relief was raised 50%.

namespace
{
// The shipped 16x16 geometry (BlockModelGeom at 1280x720, tier 2).
BlockPickGeom Geom()
{
    BlockPickGeom g;
    g.originX = 445.0f; g.originY = 196.0f;
    g.tileX = 13.9f;    g.tileY = 3.9f;
    g.gap = 94.0f;      g.size = 16;
    return g;
}
constexpr float RELIEF = 56.0f;   // g.relief after the x1.5 pass

float DrawnX(const BlockPickGeom& g, float i, float j)
{
    return g.originX + (i - j) * g.tileX;
}
float DrawnY(const BlockPickGeom& g, int layer, float i, float j, float lift)
{
    return g.originY + (i + j) * g.tileY + layer * g.gap - lift;
}

// Does the point actually land in this cell's drawn diamond?
bool Covers(const BlockPickGeom& g, int layer, float mx, float my,
            int i, int j, float lift)
{
    float a = (mx - g.originX) / g.tileX;
    float b = (my - g.originY - layer * g.gap + lift) / g.tileY;
    return static_cast<int>(std::floor((a + b) * 0.5f)) == i &&
           static_cast<int>(std::floor((b - a) * 0.5f)) == j;
}
}

TEST_CASE("a uniformly lifted plate round-trips every block", "[blockpick]")
{
    // No occlusion when the whole surface rises together, so the pick must
    // return exactly the block whose drawn centre was clicked. This is the
    // case the old flat inversion got wrong for every single cell.
    BlockPickGeom g = Geom();
    auto lift = [](int, int) { return RELIEF * 0.6f; };

    for (int layer = 0; layer < 4; layer++)
    {
        for (int j = 0; j < g.size; j++)
        {
            for (int i = 0; i < g.size; i++)
            {
                float mx = DrawnX(g, i + 0.5f, j + 0.5f);
                float my = DrawnY(g, layer, i + 0.5f, j + 0.5f, lift(i, j));
                int pi = -1, pj = -1;
                REQUIRE(BlockPickCell(g, layer, mx, my, lift, pi, pj));
                REQUIRE(pi == i);
                REQUIRE(pj == j);
            }
        }
    }
}

TEST_CASE("over a relief field the pick returns the front-most covering block",
          "[blockpick]")
{
    // With a shoot in the plate, a nearer high block genuinely covers the
    // pixels where a low one is drawn -- you cannot click what is hidden
    // behind it. So the invariant is not identity: it is that the answer
    // COVERS the point, and that nothing nearer also covers it.
    BlockPickGeom g = Geom();
    auto lift = [&](int i, int j) {
        float dx = (i - 5) / 4.0f, dy = (j - 6) / 4.0f;
        return RELIEF * std::exp(-(dx * dx + dy * dy));
    };

    int checked = 0;
    for (int j = 0; j < g.size; j++)
    {
        for (int i = 0; i < g.size; i++)
        {
            float mx = DrawnX(g, i + 0.5f, j + 0.5f);
            float my = DrawnY(g, 0, i + 0.5f, j + 0.5f, lift(i, j));

            int pi = -1, pj = -1;
            REQUIRE(BlockPickCell(g, 0, mx, my, lift, pi, pj));
            REQUIRE(Covers(g, 0, mx, my, pi, pj, lift(pi, pj)));

            for (int bj = 0; bj < g.size; bj++)
                for (int bi = 0; bi < g.size; bi++)
                    if (bi + bj > pi + pj)
                        REQUIRE_FALSE(Covers(g, 0, mx, my, bi, bj, lift(bi, bj)));
            checked++;
        }
    }
    REQUIRE(checked == g.size * g.size);
}

TEST_CASE("ignoring the lift picks the wrong block", "[blockpick]")
{
    // Guards the fix itself: with lift ignored the pick is off by
    // lift/tileY rows, so a lifted block resolves to some OTHER block.
    BlockPickGeom g = Geom();
    auto lift = [](int, int) { return RELIEF; };
    auto noLift = [](int, int) { return 0.0f; };

    float mx = DrawnX(g, 8.5f, 8.5f);
    float my = DrawnY(g, 1, 8.5f, 8.5f, RELIEF);

    int li = -1, lj = -1, fi = -1, fj = -1;
    REQUIRE(BlockPickCell(g, 1, mx, my, lift, li, lj));
    REQUIRE(li == 8);
    REQUIRE(lj == 8);

    bool flatHit = BlockPickCell(g, 1, mx, my, noLift, fi, fj);
    bool flatSame = (flatHit && fi == 8 && fj == 8);
    REQUIRE_FALSE(flatSame);                        // the bug this fixes
}
