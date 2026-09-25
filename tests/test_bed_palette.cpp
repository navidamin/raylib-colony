#include <catch2/catch_test_macros.hpp>
#include "bed_palette.h"
#include "survey_constants.h"

// One table of bed colours for every picture of the ground. The C side is
// held to it by c2dtest section 21; this holds the C++ side, and the one
// derived shade that has a floor.

static bool Same(Color a, Color b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }

TEST_CASE("BedPalette: the C++ survey block reads the same table", "[bed_palette]")
{
    for (int k = 0; k < SURVEY_BEDS; k++)
    {
        CHECK(Same(SURVEY_BED_PALETTE[k].neon, BedPalette(k, BED_NEON)));
        CHECK(Same(SURVEY_BED_PALETTE[k].deep, BedPalette(k, BED_DEEP)));
        CHECK(Same(SURVEY_BED_PALETTE[k].mesh, BedPalette(k, BED_MESH)));
    }
}

TEST_CASE("BedPalette: a textured band is never dark enough to smudge", "[bed_palette]")
{
    // The dock tints its texture x2 against a mean of 128: a body under
    // luminance ~50 multiplies the texture into a flat smudge.
    for (int k = 0; k < BED_PALETTE_BEDS; k++)
    {
        const Color c = BedPalette(k, BED_ROCK);
        CHECK(BedPalette_Luma(c.r, c.g, c.b) >= 49.0f);
        // and it is still the bed's own hue: never brighter than the bed
        const Color n = BedPalette(k, BED_NEON);
        CHECK(c.r <= n.r);
        CHECK(c.g <= n.g);
        CHECK(c.b <= n.b);
    }
}

TEST_CASE("BedPalette: it is a compile-time table in C++", "[bed_palette]")
{
    // evaluable at compile time -- and never pinned to a colour, which is
    // the thing this table exists to let change
    constexpr Color c = BedPalette(2, BED_NEON);
    static_assert(c.a == 255, "BedPalette is constexpr in C++");
    CHECK(Same(c, BedPalette(2, BED_NEON)));
}
