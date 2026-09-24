/* dash_knowledge.c — see dash_knowledge.h. Transcribed from
 * src/Survey/survey_knowledge.cpp; do not re-tune one without the other. */
#include "dash_knowledge.h"

#include <math.h>
#include <string.h>

static float DkLerp(float a, float b, float t) { return a + (b - a) * t; }

static float DkSmoothStep(float e0, float e1, float x)
{
    float span = e1 - e0;
    if (fabsf(span) < 1e-6f) span = 1e-6f;
    float t = (x - e0) / span;
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return t * t * (3.0f - 2.0f * t);
}

void DashKnow_Clear(DashKnowledge *k)
{
    if (!k) return;
    memset(k, 0, sizeof(*k));
    k->cachedRevision = -1;
    k->cachedLattice = -1;
    k->cachedColumnM = -1.0f;
    k->cachedGain = -1.0f;
}

void DashKnow_Add(DashKnowledge *k, float i, float j, float depthM)
{
    if (!k || k->count >= DK_HOLES_MAX) return;
    k->holes[k->count++] = (DkHole){i, j, depthM};
    k->revision++;
}

static float g_gain = 1.0f;
void  DashKnow_SetDebugGain(float gain) { g_gain = gain < 1.0f ? 1.0f : gain; }
float DashKnow_DebugGain(void)          { return g_gain; }

float DashKnow_KnowAt(const DashKnowledge *k, float i, float j, float depthM)
{
    if (!k) return 0.0f;
    float miss = 1.0f;
    for (int n = 0; n < k->count; n++)
    {
        const DkHole *h = &k->holes[n];
        const float dx = i - h->i, dy = j - h->j;
        const float u = (dx * dx + dy * dy) / (DK_K_RADIUS * DK_K_RADIUS);
        const float nearT = DK_K_NEAR * expf(-u * 0.72f);
        /* Depth is the one hard edge: a hole that stopped at 300 m has seen
         * nothing at 1 km, so its weight decays to a fifth below its own
         * bottom -- a fifth and not zero, because beds continue. */
        const float dep = DkLerp(1.0f, DK_K_BELOW,
                                 DkSmoothStep(h->depthM, h->depthM + DK_K_SKIRT_M, depthM));
        float take = (nearT + DK_K_FAR) * dep;
        if (take > 0.985f) take = 0.985f;
        miss *= (g_gain == 1.0f) ? 1.0f - take : powf(1.0f - take, g_gain);
    }
    return 1.0f - miss;
}

float DashKnow_Confidence(float known)
{
    const float c = known / DK_K_FULL;
    return c < 0.0f ? 0.0f : (c > 1.0f ? 1.0f : c);
}

float DashKnow_Delineation(DashKnowledge *k, int lattice, float columnM)
{
    if (!k) return 0.0f;
    if (k->cachedRevision == k->revision &&
        k->cachedLattice  == lattice &&
        k->cachedColumnM  == columnM &&
        k->cachedGain     == g_gain) return k->cachedDelineation;
    k->cachedRevision = k->revision;
    k->cachedLattice  = lattice;
    k->cachedColumnM  = columnM;
    k->cachedGain     = g_gain;
    if (k->count == 0) { k->cachedDelineation = 0.0f; return 0.0f; }
    float sum = 0.0f;
    int n = 0;
    for (int a = 0; a <= 8; a++)
        for (int b = 0; b <= 8; b++)
            for (int d = 0; d < 7; d++)
            {
                sum += DashKnow_Confidence(
                    DashKnow_KnowAt(k, (float)a / 8.0f * (float)lattice,
                                       (float)b / 8.0f * (float)lattice,
                                       ((float)d + 0.5f) / 7.0f * columnM));
                n++;
            }
    k->cachedDelineation = sum / (float)n;
    return k->cachedDelineation;
}

const char *DashKnow_Tier(DashKnowledge *k, int lattice, float columnM)
{
    const float d = DashKnow_Delineation(k, lattice, columnM);
    if (d >= DK_DELIN_GATE) return "MEASURED";
    if (d >= 0.40f) return "INDICATED";
    return "INFERRED";
}

bool DashKnow_IsMeasured(DashKnowledge *k, int lattice, float columnM)
{
    return DashKnow_Delineation(k, lattice, columnM) >= DK_DELIN_GATE;
}
