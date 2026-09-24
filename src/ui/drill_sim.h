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

#include "subsurface.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DRILL_STRATA_COUNT 4
/* The bottom of the drill's reach IS the bottom of the surveyed column --
 * they were two spellings of 120 and are now one. */
#define DRILL_TARGET_M     SUB_COLUMN_M

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

    /* THE STRING TURNS ONLY WHEN IT HAS BEEN TOLD TO. Planning a hole and
     * drilling it are two acts: a depth is chosen, and then the player starts
     * the drill. Until DrillSim_Start the spindle is still and nothing
     * advances; landing the target stops it again, so going deeper is another
     * plan and another start. */
    bool  running;
} DrillSim;

/* The spindle's full scale -- what the gauges divide by. */
#define DRILL_RPM_MAX 1.35f

/* What the drill reports, each 0..1: one reading, used by DRILL STATS and by
 * the profile, so the panel and the record cannot disagree. */
typedef struct DrillReadout {
    float rpm;                  /* rotary speed, of DRILL_RPM_MAX         */
    float load;                 /* what the rock pushes back              */
    float heat;
    float wear;                 /* 0 = fresh bit, 1 = spent               */
    float vib;
} DrillReadout;

DrillReadout DrillSim_Read(const DrillSim *s);

void DrillSim_Reset(DrillSim *s);
void DrillSim_Step (DrillSim *s, float dt);

/* One click on the face. This is the whole input -- once the drill is
 * running. Before that a click is ignored: see DrillSim_Start. */
void DrillSim_Bite (DrillSim *s);

/* Start the string down toward targetM. False, and nothing happens, when
 * there is no target or the bit is already at it. */
bool DrillSim_Start(DrillSim *s);

/* Stop a running string where it is. The hole stays at the depth the bit
 * reached -- that depth becomes the target, so the hole reads as finished
 * there -- and a trip in progress ends with the string back on bottom.
 * False when nothing was running. */
bool DrillSim_Abort(DrillSim *s);

/* Pull the string and change the bit: costs time that scales with depth. */
void DrillSim_BeginTrip(DrillSim *s, bool broken);

/* C6: feed the string to exactly this depth and stop there. */
void DrillSim_SetTarget(DrillSim *s, float depthM);

/* Where the spindle sits relative to the current rock's band:
 * -1 rubbing, 0 in band, +1 over-driving. Drives the motor-pod lamp. */
int  DrillSim_BandState(const DrillSim *s);

/* ---- THE DIG PROFILE ----------------------------------------------------
 *
 * What the hole said on the way down: the drill's readout every
 * DRILL_PROFILE_STEP_M of depth, so the log of a hole is the rock as the bit
 * felt it rather than a single number at the bottom. Opened when a depth is
 * committed -- the plan exists before the first turn -- and filled while the
 * string runs. Not drawn yet; the core log will read it.
 *
 * A fixed array, not a heap one, so the console state stays one flat,
 * copyable object. */
/* Two bins a metre, so one every 0.5 m. Counted in bins rather than metres
 * because C sizes an array only from an integer constant expression, and a
 * float division is not one. */
#define DRILL_PROFILE_PER_M  2
#define DRILL_PROFILE_STEP_M (1.0f / (float)DRILL_PROFILE_PER_M)
#define DRILL_PROFILE_MAX    ((int)DRILL_TARGET_M * DRILL_PROFILE_PER_M)

typedef struct DrillSample {
    float depthM;               /* the bin's own depth: 0.5, 1.0, ...     */
    float t;                    /* sim seconds when the bit reached it    */
    DrillReadout read;
    unsigned char stratum;      /* index into DrillSim_Strata()           */
} DrillSample;

typedef struct DrillProfile {
    bool  open;                 /* a plan exists for this hole            */
    float siteI, siteJ;         /* lattice coordinates                    */
    float targetM;
    float plannedT, startedT, finishedT;   /* sim seconds; <0 = not yet   */
    bool  aborted;              /* stopped short of targetM by the player */
    /* the readings since the last bin closed: each bin keeps their MEAN, as
     * a drilling log does -- the reading at the instant of crossing would
     * alias against the player's tapping and saw-tooth bin to bin */
    DrillReadout acc;
    int   accN;
    int   count;
    DrillSample sample[DRILL_PROFILE_MAX];
} DrillProfile;

/* A new hole at a new site: forget the old one. */
void DrillProfile_Clear(DrillProfile *p);

/* Commit a depth. The first plan at a site opens the profile; a later one
 * (drilling deeper from where the last one stopped) moves the target and
 * keeps what is already recorded -- it is the same hole. */
void DrillProfile_Plan(DrillProfile *p, float siteI, float siteJ, float targetM, float t);

/* Call after every DrillSim_Step. Accumulates the readout while the bit is
 * cutting and writes one sample per bin the bit has crossed -- the mean over
 * that interval -- so a long frame cannot skip a bin. Returns how many it
 * wrote. */
int  DrillProfile_Record(DrillProfile *p, const DrillSim *s);

/* ---- THE CORE LOG: a finished hole, kept ------------------------------
 *
 * The live profile is floats and belongs to the hole being drilled. A
 * finished hole is kept as this: the same readings a bin at a time, packed to
 * a byte each (0..255 for 0..1), so a console can keep every hole it has
 * drilled -- 1.4 KB a hole -- and show any of them again. */
#define DRILL_READ_N 5              /* rpm, load, heat, wear, vib */

typedef struct DrillCoreLog {
    float u, v;                     /* cap coordinates, 0..1               */
    float siteI, siteJ;             /* lattice coordinates                 */
    float depthM;                   /* how deep it went                    */
    bool  aborted;
    int   count;                    /* bins recorded                       */
    unsigned char read[DRILL_PROFILE_MAX][DRILL_READ_N];
    unsigned char stratum[DRILL_PROFILE_MAX];
} DrillCoreLog;

void  DrillCoreLog_From(DrillCoreLog *out, const DrillProfile *p,
                        float u, float v, float depthM);
float DrillCoreLog_Read(const DrillCoreLog *l, int bin, int which);   /* 0..1 */

/* The player stopped the string: the profile ends where the bit is, short
 * of its plan, and says so. What was recorded is kept. */
void DrillProfile_Abort(DrillProfile *p, const DrillSim *s);

#ifdef __cplusplus
}
#endif

#endif /* DRILL_SIM_H */
