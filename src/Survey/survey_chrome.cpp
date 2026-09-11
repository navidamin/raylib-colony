#include "survey_chrome.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float BRACKET_LEG = 26.0f;
    constexpr float BRACKET_W = 2.0f;
    constexpr float PANEL_ROUND = 12.0f;
}

void SurveyChrome::Panel(Rectangle r, Color fill, Color ground)
{
    const float round = std::min(0.35f, PANEL_ROUND / std::min(r.width, r.height));
    DrawRectangleRounded(r, round, 6, fill);
    DrawRectangleRoundedLinesEx(r, round, 6, 1.5f, SC_LINE);

    const float leg = std::min(BRACKET_LEG, std::min(r.width, r.height) * 0.28f);
    const float x0 = r.x, x1 = r.x + r.width, y0 = r.y, y1 = r.y + r.height;
    const struct { float x, y, dx, dy; } corners[4] =
    {
        { x0, y0,  1.0f,  1.0f }, { x1, y0, -1.0f,  1.0f },
        { x0, y1,  1.0f, -1.0f }, { x1, y1, -1.0f, -1.0f },
    };
    for (const auto& c : corners)
    {
        /* The punch. Five pixels of the dim outline are erased just past
           where each leg ends, so the bracket reads as laid OVER the frame
           rather than as the frame's own corner. Without it the two merge
           and the panel loses the one detail that makes it look machined. */
        DrawRectangle(static_cast<int>(c.x + c.dx * leg - (c.dx < 0 ? 5.0f : 0.0f)),
                      static_cast<int>(c.y - 1.0f), 5, 3, ground);
        DrawRectangle(static_cast<int>(c.x - 1.0f),
                      static_cast<int>(c.y + c.dy * leg - (c.dy < 0 ? 5.0f : 0.0f)),
                      3, 5, ground);
        DrawLineEx({c.x, c.y}, {c.x + c.dx * leg, c.y}, BRACKET_W, SC_ACCENT);
        DrawLineEx({c.x, c.y}, {c.x, c.y + c.dy * leg}, BRACKET_W, SC_ACCENT);
    }
}

void SurveyChrome::Box(Rectangle r)
{
    const float round = std::min(0.35f, 10.0f / std::min(r.width, r.height));
    DrawRectangleRounded(r, round, 5, SC_BOX_FILL);
    DrawRectangleRoundedLinesEx(r, round, 5, 1.5f, SC_BAR_EDGE);
    const float leg = 9.0f;
    const struct { float x, y, dx, dy; } c[4] =
    {
        { r.x, r.y, 1.0f, 1.0f }, { r.x + r.width, r.y, -1.0f, 1.0f },
        { r.x, r.y + r.height, 1.0f, -1.0f }, { r.x + r.width, r.y + r.height, -1.0f, -1.0f },
    };
    for (const auto& k : c)
    {
        DrawLineEx({k.x, k.y + k.dy * leg}, {k.x, k.y + k.dy * leg * 0.5f}, 2.0f, SC_ACCENT);
        DrawLineEx({k.x + k.dx * leg * 0.5f, k.y}, {k.x + k.dx * leg, k.y}, 2.0f, SC_ACCENT);
    }
}

void SurveyChrome::Connector(float x, float yFrom, float yTo)
{
    DrawLineEx({x, yFrom}, {x, yTo}, 2.0f, SC_ACCENT_DIM);
    DrawRectangle(static_cast<int>(x - 3.0f), static_cast<int>(yFrom - 1.5f), 6, 3, SC_ACCENT);
    DrawRectangle(static_cast<int>(x - 3.0f), static_cast<int>(yTo - 1.5f), 6, 3, SC_ACCENT);
}

void SurveyChrome::SegBar(Rectangle r, float value01, Color on, int segments)
{
    const float gap = 3.0f;
    const int n = segments > 0 ? segments
                               : std::max(4, static_cast<int>((r.width + gap) / (r.height + gap)));
    const float cell = (r.width - gap * (n - 1)) / n;
    const int lit = static_cast<int>(std::lround(std::min(std::max(value01, 0.0f), 1.0f) * n));
    for (int i = 0; i < n; i++)
    {
        Rectangle c = { r.x + i * (cell + gap), r.y, cell, r.height };
        const bool isOn = i < lit;
        DrawRectangleRounded(c, 0.3f, 4, isOn ? on : SC_METER_OFF);
        if (isOn)
        {
            DrawRectangleRoundedLinesEx(c, 0.3f, 4, 1.0f, Fade(BLACK, 0.45f));
            // the 2 px white top highlight that makes a lit cell read as raised
            DrawRectangle(static_cast<int>(c.x + 1.5f), static_cast<int>(c.y + 1.0f),
                          static_cast<int>(c.width - 3.0f), 2, Fade(WHITE, 0.55f));
        }
        else
        {
            DrawRectangleRoundedLinesEx(c, 0.3f, 4, 1.0f, SC_BAR_EDGE);
        }
    }
}

void SurveyChrome::HexVignette(float cx, float cy, float rx, float ry)
{
    Vector2 p[6];
    for (int k = 0; k < 6; k++)
    {
        const float a = -PI / 2.0f + k * PI / 3.0f;
        p[k] = { cx + std::cos(a) * rx, cy + std::sin(a) * ry };
    }
    for (int k = 0; k < 6; k++) DrawLineEx(p[k], p[(k + 1) % 6], 1.5f, SC_HEX);
}

void SurveyChrome::Title(const Font& font, const char* text, float x, float y, float size)
{
    DrawTextEx(font, text, {x, y}, size, 1.0f, SC_TITLE);
    const float w = std::min(40.0f, MeasureTextEx(font, text, size, 1.0f).x);
    DrawRectangleRounded({x, y + size + 5.0f, w, 3.0f}, 0.5f, 3, SC_UNDERLINE);
}

void SurveyChrome::LabelRight(const Font& font, const char* text, float rightX, float y,
                              float size, Color col)
{
    const float w = MeasureTextEx(font, text, size, 1.0f).x;
    DrawTextEx(font, text, {rightX - w, y}, size, 1.0f, col);
}

void SurveyChrome::LabelCentre(const Font& font, const char* text, float centreX, float y,
                               float size, Color col)
{
    const float w = MeasureTextEx(font, text, size, 1.0f).x;
    DrawTextEx(font, text, {centreX - w * 0.5f, y}, size, 1.0f, col);
}

Color SurveyChrome::Health(float value01)
{
    if (value01 >= 0.75f) return SC_GOOD;
    if (value01 >= 0.50f) return SC_FAIR;
    if (value01 >= 0.25f) return SC_WARN;
    return SC_BAD;
}
