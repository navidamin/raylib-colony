#include <catch2/catch_test_macros.hpp>
#include "rock_texture.h"

#include <cmath>
#include <cstdlib>

// The generated strata (src/Prospecting/rock_texture.h). Three properties the
// rest of the renderer depends on, none of which is visible in a screenshot.

static float MeanLum(const std::vector<unsigned char>& px)
{
    double sum = 0.0;
    int n = static_cast<int>(px.size() / 4);
    for (int i = 0; i < n; i++)
    {
        sum += 0.299 * px[i * 4] + 0.587 * px[i * 4 + 1] + 0.114 * px[i * 4 + 2];
    }
    return static_cast<float>(sum / n);
}

TEST_CASE("every stratum texture is centred on 128", "[rocktex]")
{
    // The contract that let texture be added without re-tuning one palette
    // entry: the caller draws `rockColour * 2 * tex/255`, so a mean of 128 is
    // a mean of x1.0 and a textured band holds the tone its flat fill had.
    for (int L = 0; L < 4; L++)
    {
        std::vector<unsigned char> px = RockTexture::Generate(L, RockTexture::SIZE);
        REQUIRE(px.size() == static_cast<size_t>(RockTexture::SIZE) * RockTexture::SIZE * 4);
        float mean = MeanLum(px);
        REQUIRE(std::fabs(mean - 128.0f) < 2.0f);
    }
}

TEST_CASE("strata textures tile without a seam", "[rocktex]")
{
    // The strip tiles a band down a column of any height. A texture whose
    // edges do not meet shows a hard line at every repeat, so the wrap has to
    // be built in, not filtered away: the step across the seam must be no
    // worse than a typical step inside the tile.
    const int N = RockTexture::SIZE;
    for (int L = 0; L < 4; L++)
    {
        std::vector<unsigned char> px = RockTexture::Generate(L, N);
        auto lum = [&](int x, int y) {
            int i = (y * N + x) * 4;
            return 0.299f * px[i] + 0.587f * px[i + 1] + 0.114f * px[i + 2];
        };

        double interior = 0.0, seamX = 0.0, seamY = 0.0;
        for (int y = 0; y < N; y++)
            for (int x = 0; x + 1 < N; x++) interior += std::fabs(lum(x + 1, y) - lum(x, y));
        interior /= static_cast<double>(N) * (N - 1);

        for (int y = 0; y < N; y++) seamX += std::fabs(lum(0, y) - lum(N - 1, y));
        for (int x = 0; x < N; x++) seamY += std::fabs(lum(x, 0) - lum(x, N - 1));
        seamX /= N; seamY /= N;

        REQUIRE(seamX < interior * 1.6f);
        REQUIRE(seamY < interior * 1.6f);
    }
}

TEST_CASE("the four strata are different rocks, and each is deterministic", "[rocktex]")
{
    // Ground must not shimmer between frames, and four layers that came out
    // as the same noise re-tinted would defeat the whole exercise.
    std::vector<std::vector<unsigned char>> tex;
    for (int L = 0; L < 4; L++) tex.push_back(RockTexture::Generate(L, RockTexture::SIZE));

    for (int L = 0; L < 4; L++)
    {
        REQUIRE(RockTexture::Generate(L, RockTexture::SIZE) == tex[L]);
    }
    for (int a = 0; a < 4; a++)
        for (int b = a + 1; b < 4; b++)
        {
            long diff = 0;
            for (size_t i = 0; i < tex[a].size(); i += 4)
                diff += std::abs(static_cast<int>(tex[a][i]) - static_cast<int>(tex[b][i]));
            REQUIRE(diff / static_cast<long>(tex[a].size() / 4) > 4);
        }
}
