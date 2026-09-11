#include "survey_knowledge.h"
#include "survey_constants.h"

#include <algorithm>
#include <cmath>

namespace
{
    float Lerp01(float a, float b, float t)
    {
        return a + (b - a) * t;
    }

    float SmoothStep(float edge0, float edge1, float x)
    {
        float span = edge1 - edge0;
        if (std::fabs(span) < 1e-6f) span = 1e-6f;
        float t = (x - edge0) / span;
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        return t * t * (3.0f - 2.0f * t);
    }
}

float SurveyKnowledge::KnowAt(float i, float j, float depthM) const
{
    float miss = 1.0f;
    for (const SurveyReveal& h : reveals)
    {
        const float dx = i - h.i;
        const float dy = j - h.j;
        const float u = (dx * dx + dy * dy) / (SURVEY_K_RADIUS * SURVEY_K_RADIUS);
        const float near = SURVEY_K_NEAR * std::exp(-u * 0.72f);
        const float dep = Lerp01(1.0f, SURVEY_K_BELOW,
                                 SmoothStep(h.depthM, h.depthM + SURVEY_K_SKIRT_M, depthM));
        miss *= 1.0f - std::min((near + SURVEY_K_FAR) * dep, 0.985f);
    }
    return 1.0f - miss;
}

float SurveyKnowledge::Confidence(float known)
{
    const float c = known / SURVEY_K_FULL;
    return c < 0.0f ? 0.0f : (c > 1.0f ? 1.0f : c);
}

float SurveyKnowledge::Delineation(int lattice, float columnM) const
{
    if (cachedRevision == revision) return cachedDelineation;
    cachedRevision = revision;
    if (reveals.empty())
    {
        cachedDelineation = 0.0f;
        return 0.0f;
    }
    float sum = 0.0f;
    int n = 0;
    for (int a = 0; a <= 8; a++)
    {
        for (int b = 0; b <= 8; b++)
        {
            for (int d = 0; d < 7; d++)
            {
                sum += Confidence(KnowAt(a / 8.0f * lattice, b / 8.0f * lattice,
                                         (d + 0.5f) / 7.0f * columnM));
                n++;
            }
        }
    }
    cachedDelineation = sum / static_cast<float>(n);
    return cachedDelineation;
}

const char* SurveyKnowledge::Tier(int lattice, float columnM) const
{
    const float d = Delineation(lattice, columnM);
    if (d >= SURVEY_DELIN_GATE) return "MEASURED";
    if (d >= 0.40f) return "INDICATED";
    return "INFERRED";
}

bool SurveyKnowledge::IsMeasured(int lattice, float columnM) const
{
    return Delineation(lattice, columnM) >= SURVEY_DELIN_GATE;
}

void SurveyKnowledge::Add(float i, float j, float depthM)
{
    reveals.push_back({i, j, depthM});
    revision++;
}

void SurveyKnowledge::Clear()
{
    reveals.clear();
    revision++;
}
