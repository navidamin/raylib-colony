#include <catch2/catch_test_macros.hpp>
#include "excavation_types.h"
#include "prospecting_constants.h"

// DigSite is the excavation facade's per-spot worked state. It must cover
// the WHOLE prospecting lattice: a stale literal size once left every spot
// with x or y >= 8 reading as exhausted, and the balance sim's players
// stalled on the centre spot the moment the lattice outgrew it.

TEST_CASE("the dig site covers every spot of the lattice", "[digsite]")
{
    DigSite site;
    const int N = PROSPECTING_GRID_SIZE;
    REQUIRE(DigSite::GRID == N);

    // fresh ground everywhere, including the far corner and the centre
    for (int d = 0; d < 4; d++)
    {
        DepthLayer depth = static_cast<DepthLayer>(d);
        REQUIRE(site.Remaining(0, 0, depth) == 1.0f);
        REQUIRE(site.Remaining(N / 2, N / 2, depth) == 1.0f);
        REQUIRE(site.Remaining(N - 1, N - 1, depth) == 1.0f);
        REQUIRE_FALSE(site.IsExhausted(N - 1, N - 1, depth));
    }

    // taking from one spot touches only that spot
    site.Take(N - 1, N - 1, DepthLayer::DEEP, 0.4f);
    REQUIRE(site.Remaining(N - 1, N - 1, DepthLayer::DEEP) == 0.6f);
    REQUIRE(site.Remaining(N - 1, N - 1, DepthLayer::SURFACE) == 1.0f);
    REQUIRE(site.Remaining(N - 2, N - 1, DepthLayer::DEEP) == 1.0f);
    REQUIRE(site.Remaining(0, 0, DepthLayer::DEEP) == 1.0f);

    // and only ground outside the lattice reads as nothing
    REQUIRE(site.Remaining(N, 0, DepthLayer::SURFACE) == 0.0f);
    REQUIRE(site.Remaining(-1, 0, DepthLayer::SURFACE) == 0.0f);
}
