/* drill_sim.h — the drill's behaviour, ported from
 * docs/design/subsurface/prototypes/redline.html (its "game rules" block,
 * lines 251-264, and step(), 640-711).
 *
 * Pure logic: no drawing, no raylib beyond the types. The drill bar in
 * dash_chrome.c renders it.
 *
 * The loop in one line: CLICKING drives the spindle, and the spindle heats
 * the bit. Each click kicks the RPM up; RPM decays back toward idle on a
 * time constant, so holding a rate means keeping a rhythm. Heat gains with
 * RPM x rock hardness and bleeds constantly, so driving hard buys depth and
 * costs temperature. Each rock has a contact-pressure BAND: under it the bit
 * rubs instead of cutting and penetration collapses, over it the core is
 * ground to powder. The band moves with the strata.
 */
#ifndef DRILL_SIM_H
#define DRILL_SIM_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DRILL_STRATA_COUNT 4
#define DRILL_TARGET_M     120.0f

typedef struct DrillStratum {
    const char *name;
    float top, bot;             /* metres                                */
    unsigned char col[3];       /* body, edge, grain                     */
    unsigned char edge[3];
    unsigned char grain[3];
    float hard;                 /* 0..1, drives cut rate and heat        */
    float bandLo, bandHi;       /* the contact-pressure window           */
    float fragility;            /* how badly over-driving grinds core    */
    bool  icy;                  /* volatiles that cook off above COOK_START */
} DrillStratum;

const DrillStratum *DrillSim_Strata(void);          /* DRILL_STRATA_COUNT   */
const DrillStratum *DrillSim_At(float depthM);

typedef struct DrillSim {
    float depthM;
    float phase;                /* spindle angle, radians                */
    float heat;                 /* 0..1                                  */
    float rpm;
    float wear;                 /* 1 = fresh bit                         */
    float rate;                 /* metres per second, last step          */
    float lift;                 /* metres the bit is off bottom, tripping */
    bool  tripping;
    float tripT, tripDur;
    int   tripCount;
    float shake;                /* 0..1, decays; the click's kick        */
    bool  done;
    float t;

    /* C6: the borehole bar owns the depth. Pick a depth on the ruler and the
     * string is fed to exactly there and stops; the hole is then finished and
     * the model learns from it. targetM < 0 means "no target": free drilling,
     * which is what the prototype does. */
    float targetM;
    bool  completed;            /* set for ONE step when the target lands  */
    float completedAtM;
} DrillSim;

void DrillSim_Reset(DrillSim *s);
void DrillSim_Step (DrillSim *s, float dt);

/* One click on the face. This is the whole input. */
void DrillSim_Bite (DrillSim *s);

/* Pull the string and change the bit: costs time that scales with depth. */
void DrillSim_BeginTrip(DrillSim *s, bool broken);

/* C6: feed the string to exactly this depth and stop there. */
void DrillSim_SetTarget(DrillSim *s, float depthM);

/* Where the spindle sits relative to the current rock's band:
 * -1 rubbing, 0 in band, +1 over-driving. Drives the motor-pod lamp. */
int  DrillSim_BandState(const DrillSim *s);

#ifdef __cplusplus
}
#endif

#endif /* DRILL_SIM_H */
