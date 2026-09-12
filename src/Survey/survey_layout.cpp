#include "survey_layout.h"

#include <algorithm>

namespace
{
    constexpr float MARGIN = 10.0f;
    constexpr float GUTTER = 10.0f;
    constexpr float BAR_H = 34.0f;
    constexpr float BAR_GAP = 8.0f;
    // The design's own proportions, at 1536 x 1024:
    //   columns 338 : 740 : 328   rows 676 : 242
    constexpr float COL_L = 0.238f, COL_M = 0.526f;
    constexpr float ROW_TOP = 0.700f;
    constexpr float ROW_GAP = 10.0f;
}

SurveyLayout ComputeSurveyLayout(Rectangle region)
{
    SurveyLayout L;

    L.bar = { region.x + MARGIN, region.y + MARGIN,
              region.width - MARGIN * 2.0f, BAR_H };

    const float top = L.bar.y + L.bar.height + BAR_GAP;
    const float bottom = region.y + region.height - MARGIN;
    const float usableW = region.width - MARGIN * 2.0f - GUTTER * 2.0f;
    const float usableH = bottom - top;

    const float wL = usableW * COL_L;
    const float wM = usableW * COL_M;
    const float wR = usableW - wL - wM;

    const float hTop = (usableH - ROW_GAP) * ROW_TOP;
    const float hBot = usableH - ROW_GAP - hTop;

    const float xL = region.x + MARGIN;
    const float xM = xL + wL + GUTTER;
    const float xR = xM + wM + GUTTER;

    L.survey = { xL, top, wL, hTop };
    L.stats  = { xL, top + hTop + ROW_GAP, wL, hBot };
    L.layers = { xM, top, wM, usableH };
    L.drill  = { xR, top, wR, hTop };
    L.dstats = { xR, top + hTop + ROW_GAP, wR, hBot };

    // The log is a box INSIDE the LAYERS panel rather than a panel of its own:
    // it is what you read between holes, and it belongs to the block it is
    // reporting on.
    const float logH = std::max(70.0f, usableH * 0.26f);
    L.log = { L.layers.x + 14.0f, L.layers.y + L.layers.height - logH - 12.0f,
              L.layers.width - 28.0f, logH };

    // The block owns everything above the log, less room for the confidence
    // bar, which is the one number the whole panel is trying to grow.
    const float confH = 16.0f;
    const float blockTop = L.layers.y + 34.0f;
    const float blockBottom = L.log.y - confH - 14.0f;
    L.block = { L.layers.x + 10.0f, blockTop,
                L.layers.width - 20.0f, std::max(60.0f, blockBottom - blockTop) };
    L.confidence = { L.layers.x + 14.0f, L.block.y + L.block.height + 4.0f,
                     L.layers.width - 28.0f, confH };

    L.leftBranchX = L.survey.x + L.survey.width * 0.5f;
    L.rightBranchX = L.drill.x + L.drill.width * 0.5f;
    return L;
}
