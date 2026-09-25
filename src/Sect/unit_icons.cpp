// unit_icons.cpp — see unit_icons.h.
#include "unit_icons.h"
#include "unit_icons_svg.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvg/nanosvg.h"
#include "nanosvg/nanosvgrast.h"

std::string UnitIconKey(const std::string& unitType)
{
    std::string k = unitType;
    std::transform(k.begin(), k.end(), k.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    if (k == "manufacture") k = "manufacturing";   // the game's name for the unit
    for (const UnitIconSvg& s : UNIT_ICON_SVGS)
        if (k == s.name) return k;
    return "";
}

namespace
{
    // Ink coverage 0..1 at px x px: rasterise at SS x, black ink on white, and
    // box-filter down. nanosvg's own antialiasing is coarse at icon sizes;
    // the supersample is what keeps thin strokes (the crane's lattice) clean.
    std::vector<float> InkMask(const char* svgText, int px)
    {
        const int SS = 3, big = px * SS;
        std::vector<char> text(svgText, svgText + std::strlen(svgText) + 1);   // nsvgParse edits its input
        NSVGimage* svg = nsvgParse(text.data(), "px", 96.0f);
        std::vector<float> mask((size_t)px * px, 0.0f);
        if (!svg) return mask;
        NSVGrasterizer* rast = nsvgCreateRasterizer();
        std::vector<unsigned char> rgba((size_t)big * big * 4, 0);
        nsvgRasterize(rast, svg, 0.0f, 0.0f, big / 100.0f, rgba.data(), big, big, big * 4);
        nsvgDeleteRasterizer(rast);
        nsvgDelete(svg);
        for (int y = 0; y < px; y++)
        {
            for (int x = 0; x < px; x++)
            {
                float ink = 0.0f;
                for (int sy = 0; sy < SS; sy++)
                {
                    for (int sx = 0; sx < SS; sx++)
                    {
                        const unsigned char* p = &rgba[(((size_t)(y * SS + sy)) * big + (x * SS + sx)) * 4];
                        ink += 1.0f - (0.299f * p[0] + 0.587f * p[1] + 0.114f * p[2]) / 255.0f;
                    }
                }
                mask[(size_t)y * px + x] = std::min(1.0f, std::max(0.0f, ink / (SS * SS)));
            }
        }
        return mask;
    }
}

Image UnitIconImage(const std::string& key, int px, Color ink, Color shadow)
{
    const char* svg = nullptr;
    for (const UnitIconSvg& s : UNIT_ICON_SVGS)
        if (key == s.name) svg = s.svg;
    if (!svg || px <= 0) return Image{};

    // A margin for the shadow's blur and offset, so it is not clipped.
    const int pad = std::max(2, px / 12);
    const int W = px + 2 * pad;
    const std::vector<float> m = InkMask(svg, px);
    auto at = [&](int x, int y) -> float {
        x -= pad;
        y -= pad;
        if (x < 0 || y < 0 || x >= px || y >= px) return 0.0f;
        return m[(size_t)y * px + x];
    };

    // Shadow: the mask, offset down-right by ~1/40 of the icon and box-blurred.
    const int off = std::max(1, px / 40), rad = std::max(1, px / 40);
    Image img = GenImageColor(W, W, BLANK);
    Color* out = (Color*)img.data;
    for (int y = 0; y < W; y++)
    {
        for (int x = 0; x < W; x++)
        {
            float sh = 0.0f;
            if (shadow.a > 0)
            {
                for (int dy = -rad; dy <= rad; dy++)
                    for (int dx = -rad; dx <= rad; dx++) sh += at(x - off + dx, y - off + dy);
                sh /= (float)((2 * rad + 1) * (2 * rad + 1));
            }
            const float a = at(x, y) * ink.a / 255.0f;
            const float s = sh * shadow.a / 255.0f;
            // ink over shadow, straight alpha
            const float A = a + s * (1.0f - a);
            Color c = BLANK;
            if (A > 0.0f)
            {
                c.r = (unsigned char)std::lround((ink.r * a + shadow.r * s * (1.0f - a)) / A);
                c.g = (unsigned char)std::lround((ink.g * a + shadow.g * s * (1.0f - a)) / A);
                c.b = (unsigned char)std::lround((ink.b * a + shadow.b * s * (1.0f - a)) / A);
                c.a = (unsigned char)std::lround(A * 255.0f);
            }
            out[(size_t)y * W + x] = c;
        }
    }
    return img;
}
