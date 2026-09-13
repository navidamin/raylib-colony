/* drill_sim.c — see drill_sim.h. Constants and step() are transcribed from
 * docs/design/subsurface/prototypes/redline.html, not re-tuned. */
#include "drill_sim.h"

#include <math.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif
#include <stdlib.h>
#include <string.h>

/* game rules (redline.html:251-264) */
#define IDLE_RPM      0.14f
#define CLICK_KICK    0.26f
#define RPM_MAX       1.35f
#define RPM_TAU       0.65f
#define FEED          0.55f
#define CUT_RATE      5.0f
#define HEAT_GAIN     0.50f
#define HEAT_BLEED    0.20f
#define COOK_START    0.52f
#define WEAR_PER_M    0.016f
#define TRIP_BASE_S   2.0f
#define TRIP_PER_M    0.075f

static float Clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

/* STRATA (redline.html:232). band = the contact-pressure window for this rock,
 * in spindle terms. It moves with the strata, which is the point: reading the
 * model beforehand tells you where it will move to. */
static const DrillStratum STRATA[DRILL_STRATA_COUNT] = {
    {"REGOLITH",      0.0f,  12.0f,  {0x3a,0x34,0x2b}, {0x19,0x15,0x10}, {0x4c,0x44,0x37},
     0.25f, 0.26f, 0.60f, 1.0f, false},
    {"MEGAREGOLITH", 12.0f,  34.0f,  {0x45,0x3e,0x34}, {0x1c,0x17,0x12}, {0x5b,0x51,0x40},
     0.55f, 0.42f, 0.84f, 1.0f, false},
    {"FRACTURED",    34.0f,  68.0f,  {0x39,0x42,0x4d}, {0x16,0x1c,0x23}, {0x4d,0x5a,0x67},
     0.45f, 0.30f, 0.58f, 2.1f, true},
    {"INTACT BASALT",68.0f, 120.0f,  {0x27,0x2a,0x30}, {0x10,0x12,0x16}, {0x34,0x38,0x41},
     0.95f, 0.68f, 1.18f, 0.7f, false},
};

const DrillStratum *DrillSim_Strata(void) { return STRATA; }

const DrillStratum *DrillSim_At(float m)
{
    for (int i = 0; i < DRILL_STRATA_COUNT; i++)
        if (m >= STRATA[i].top && m < STRATA[i].bot) return &STRATA[i];
    return &STRATA[DRILL_STRATA_COUNT - 1];
}

void DrillSim_Reset(DrillSim *s)
{
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->rpm = IDLE_RPM;
    s->wear = 1.0f;
}

void DrillSim_Bite(DrillSim *s)
{
    if (!s || s->done || s->tripping) return;
    s->rpm = fminf(RPM_MAX, s->rpm + CLICK_KICK);
    s->shake = 1.0f;
}

void DrillSim_BeginTrip(DrillSim *s, bool broken)
{
    if (!s || s->tripping) return;
    (void)broken;
    s->tripping = true;
    s->tripT = 0.0f;
    s->tripDur = TRIP_BASE_S + TRIP_PER_M * s->depthM;
    s->tripCount++;
}

int DrillSim_BandState(const DrillSim *s)
{
    if (!s) return 0;
    const DrillStratum *g = DrillSim_At(s->depthM);
    if (s->rpm > g->bandHi) return 1;
    if (s->rpm >= g->bandLo) return 0;
    return -1;
}

void DrillSim_Step(DrillSim *s, float dt)
{
    if (!s || dt <= 0.0f) return;
    s->t += dt;
    s->shake = fmaxf(0.0f, s->shake - dt * 7.0f);

    if (s->done)
    {
        s->rpm = fmaxf(0.0f, s->rpm - dt * 0.9f);
        s->heat = Clampf(s->heat - dt * 0.22f, 0.0f, 1.0f);
        s->phase -= s->rpm * 9.0f * dt;
        return;
    }

    /* A trip is the WHOLE cost of a dull bit: the string comes out rod by rod
     * and goes back, and nothing advances while it does. Time scales with
     * depth, which is what makes pushing on a gamble rather than a freebie. */
    if (s->tripping)
    {
        s->tripT += dt;
        const float f = Clampf(s->tripT / s->tripDur, 0.0f, 1.0f);
        s->lift = s->depthM * sinf(f * PI);          /* out, then back down */
        s->rpm  = fmaxf(0.0f, s->rpm - dt * 1.6f);
        s->heat = Clampf(s->heat - dt * 0.42f, 0.0f, 1.0f);  /* cools out of the hole */
        s->rate = 0.0f;
        if (f >= 1.0f) { s->tripping = false; s->lift = 0.0f; s->wear = 1.0f; }
        s->phase -= s->rpm * 9.0f * dt;
        return;
    }

    const DrillStratum *g = DrillSim_At(s->depthM);
    s->rpm = IDLE_RPM + (s->rpm - IDLE_RPM) * expf(-dt / RPM_TAU);

    /* Below the band the bit RUBS instead of cutting and penetration
     * collapses. That is what makes lifting off the face a real dwell: you
     * stop advancing, so cooling costs time rather than costing core. */
    const float lo = g->bandLo;
    const float bite = (s->rpm >= lo) ? 1.0f : (s->rpm / lo) * (s->rpm / lo);
    s->rate = s->rpm * FEED * (1.30f - g->hard * 0.85f) * CUT_RATE * bite;
    const float adv = fminf(s->rate * dt, DRILL_TARGET_M - s->depthM);

    s->depthM = Clampf(s->depthM + adv, 0.0f, DRILL_TARGET_M);

    /* THE COUPLING: heat gains with spindle speed times rock hardness, and
     * bleeds at a constant rate whatever you do. */
    s->heat = Clampf(s->heat + (s->rpm * (0.35f + g->hard * 0.75f) * HEAT_GAIN
                                - HEAT_BLEED) * dt, 0.0f, 1.0f);

    /* wear -- the cheapest failure: it buys a trip, never an ending */
    s->wear = Clampf(s->wear - adv * g->hard * (1.0f + 2.0f * s->heat) * WEAR_PER_M,
                     0.0f, 1.0f);

    if (s->wear <= 0.0f) { DrillSim_BeginTrip(s, true); return; }
    if (s->depthM >= DRILL_TARGET_M) { s->done = true; return; }

    s->phase -= s->rpm * 9.0f * dt;
}
