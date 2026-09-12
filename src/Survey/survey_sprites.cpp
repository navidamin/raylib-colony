#include "survey_sprites.h"

#include <cmath>

namespace
{
    constexpr int ROWS = 13;
    constexpr int AXIS = 4;                    // the axis column in the grid
    constexpr Color LIGHT = {250, 250, 250, 255};
    constexpr Color MID   = {133, 149, 172, 255};

    const char* GRID[static_cast<int>(SurveyTool::COUNT)][ROWS] = {
        // DRILL
        { "...mLm.....", "...mLm.....", "..mmLm.....", "..LLLmmm...",
          "..LLLLmm...", "...mm......", ".mmmLm.....", "mLLLLmmmm..",
          ".LLLLLmm...", "..mmmm.....", ".mmmmmm....", "mLLLLLmmm..",
          "mLLLLLmmm.." },
        // SWEEP -- a body on wheels, a thing that drives across the ground
        { "mm.mmm.mm..", "LL.LLL.LL..", "mmmmmmmmm..", "mLLLLLLLm..",
          "mLLLLLLLm..", ".mmLLLmm...", "..mLLLm....", "..mLLLm....",
          "...mLm.....", "...mLm.....", "...mLm.....", "...mLm.....",
          "...mLm....." },
        // SEISMIC -- a mass over a plate
        { "mmmmmmmmm..", "mLLLLLLLm..", "...mLm.....", "...mLm.....",
          ".mmmmmmm...", "mLLLLLLLm..", "mLLLLLLLm..", "mLLLLLLLm..",
          ".mmmmmmm...", "...mLm.....", "...mLm.....", "...mLm.....",
          "...mLm....." },
        // PENETROMETER -- a needle with a load collar
        { "....L......", "...mLm.....", "...mLm.....", "..mLLm.....",
          "..mLLm.....", "..mLLm.....", "..mLLm.....", ".mLLLm.....",
          "mmLLLmm....", "mLLLLLmm...", "mmLLLmm....", "..mLLm.....",
          "..mLLm....." },
        // GPR -- a sled with a radiating face
        { "L.L.L.L.L..", "mmmmmmmmm..", "mLLLLLLLm..", "mLLLLLLLm..",
          ".mmmmmmm...", "...mLm.....", "...mLm.....", "..mLLLm....",
          "..mLLLm....", "...mLm.....", "...mLm.....", "...mLm.....",
          "...mLm....." },
    };

    const SurveyToolInfo INFO[static_cast<int>(SurveyTool::COUNT)] = {
        { "DRILL",         "auger - collar a spot, then set a depth", SurveyToolMark::POINT, true  },
        { "SURFACE SWEEP", "LIBS - surface chemistry, never classifies", SurveyToolMark::LINE,  true  },
        { "ACTIVE SEISMIC","a shot and a spread - structure, not chemistry", SurveyToolMark::LINE,  false },
        { "PENETROMETER",  "a needle pushed in - strength with depth",  SurveyToolMark::POINT, false },
        { "GPR",           "towed radar - shallow contacts, fast",      SurveyToolMark::LINE,  false },
    };
}

const SurveyToolInfo& SurveyToolOf(SurveyTool tool)
{
    return INFO[static_cast<int>(tool)];
}

int SurveyToolSpriteRows() { return ROWS; }

void DrawSurveyToolSprite(SurveyTool tool, float X, float Y, int shift, float unit)
{
    const float h = unit * 0.5f;
    const char* const* rows = GRID[static_cast<int>(tool)];
    for (int r = 0; r < ROWS; r++)
    {
        const char* row = rows[r];
        const float y = Y - (r + 1) * h;
        int at[16], n = 0;
        for (int c = 0; row[c]; c++) if (row[c] != '.') at[n++] = c;
        if (!n) continue;
        for (int k = 0; k < n; k++)
        {
            const int src = ((k + shift) % n + n) % n;
            DrawRectangle(static_cast<int>(std::lround(X + (at[k] - AXIS) * h)),
                          static_cast<int>(std::lround(y)),
                          static_cast<int>(std::lround(h)), static_cast<int>(std::lround(h)),
                          row[at[src]] == 'L' ? LIGHT : MID);
        }
    }
}
