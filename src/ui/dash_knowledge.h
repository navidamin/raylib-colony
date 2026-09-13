/* dash_knowledge.h — what the console knows.
 *
 * The same model as src/Survey/survey_knowledge.{h,cpp}, in C so the console
 * library can use it. Constants and formulas are transcribed, not re-derived;
 * the two must never disagree about the same rock. When the old console
 * retires (C7) that C++ copy goes and this is the one.
 *
 * A borehole does not illuminate. It CONSTRAINS A MODEL, and a model is one
 * object: drill anywhere and every section gets better. So the fog has no
 * shape of its own --
 *
 *     fog = 1 - known
 *
 * -- and what it hides is not the truth but the current estimate of it.
 */
#ifndef DASH_KNOWLEDGE_H
#define DASH_KNOWLEDGE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DK_LATTICE      28
#define DK_K_NEAR       0.68f    /* what one hole settles where it stands   */
#define DK_K_FAR        0.15f    /* and what it still says from anywhere    */
#define DK_K_RADIUS     9.0f     /* cells over which the near term falls    */
#define DK_K_BELOW      0.20f    /* what it says about ground it never met  */
#define DK_K_SKIRT_M  230.0f     /* over what depth it drops to that        */
#define DK_K_FULL       0.95f    /* fog at 5% is confidence 1               */
#define DK_DELIN_GATE   0.95f    /* MEASURED, and the gate isolate waits on */

#define DK_HOLES_MAX 64

typedef struct DkHole { float i, j, depthM; } DkHole;

typedef struct DashKnowledge {
    DkHole holes[DK_HOLES_MAX];
    int    count;
    int    revision;
    /* Delineation walks every hole at 567 sample points, so it is cached
     * against the revision rather than recomputed per frame. */
    int    cachedRevision;
    float  cachedDelineation;
} DashKnowledge;

void  DashKnow_Clear(DashKnowledge *k);
void  DashKnow_Add  (DashKnowledge *k, float i, float j, float depthM);

/* Holes multiply their MISSES, not their hits, which is what makes three
 * mediocre holes worth more than one good one and stops any single hole
 * settling the block on its own. */
float DashKnow_KnowAt(const DashKnowledge *k, float i, float j, float depthM);
float DashKnow_Confidence(float known);

/* Mean confidence over the block's volume: the one number for "how well is
 * this known". Tiers are the mining industry's own -- INFERRED while the
 * shape is a guess, INDICATED once drilling constrains it, MEASURED only
 * when it is dense enough to bet on. */
float       DashKnow_Delineation(DashKnowledge *k, int lattice, float columnM);
const char *DashKnow_Tier       (DashKnowledge *k, int lattice, float columnM);
bool        DashKnow_IsMeasured (DashKnowledge *k, int lattice, float columnM);

#ifdef __cplusplus
}
#endif

#endif /* DASH_KNOWLEDGE_H */
