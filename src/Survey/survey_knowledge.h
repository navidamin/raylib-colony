#pragma once

#include "dash_knowledge.h"

/* =====================================================================
   WHAT THE CONSOLE KNOWS
   ---------------------------------------------------------------------
   A borehole does not illuminate. It constrains a MODEL, and a model is
   one object: drill anywhere and every section you can see gets better.
   So the fog has no shape of its own at all --

       fog = 1 - known                 (see KnowAt)

   -- and what it hides is not the truth but the current ESTIMATE of it,
   which is a different picture again, and the one this console is about.

   The surface is the one thing known for free, because you can see it.

   ONE IMPLEMENTATION, IN C. The arithmetic lives in
   src/ui/dash_knowledge.{h,c} and this is a C++ face on it. It used to
   be a second transcription of the same formulas, which is a standing
   invitation for the two to drift apart about the same rock -- see the
   graveyard record. Everything below delegates; nothing below computes.
   ===================================================================== */

// A finished hole, in the units the block is generated in: lattice
// coordinates across, metres down.
struct SurveyReveal
{
    float i = 0.0f;
    float j = 0.0f;
    float depthM = 0.0f;
};

class SurveyKnowledge
{
public:
    /* Holes multiply their MISSES, not their hits:

           miss = product of (1 - k_n)        known = 1 - miss

       which is what makes three mediocre holes worth more than one good
       one, and what stops any single hole ever settling the block on its
       own.

       Depth is the one hard edge. A hole that stopped short has seen
       nothing below its own bottom, so its weight decays to a fifth
       there -- a fifth and not zero, because beds continue, and that is
       an inference the console is entitled to make. */
    float KnowAt(float i, float j, float depthM) const
    {
        return DashKnow_KnowAt(&k, i, j, depthM);
    }

    // Knowledge rescaled so the last 5% of fog is not worth arguing about:
    // once the fog is that thin the model is called settled and stops
    // moving. Without a ceiling the interfaces would creep for ever by
    // amounts too small to see and too large to ignore.
    static float Confidence(float known) { return DashKnow_Confidence(known); }

    // Sampled at the depth the thing being drawn is a picture of: an
    // interface at its own depth, a bed's wall at the middle of the bed.
    float InterfaceConfidence(float i, float j, float depthM) const
    {
        return Confidence(KnowAt(i, j, depthM));
    }

    /* DELINEATION -- the one number for "how well is this block known".
       Mean confidence over the block's VOLUME, on a coarse grid, in exactly
       the units the fog is drawn from, so the bar and the picture can never
       disagree.

       The tiers are the mining industry's own and worth borrowing whole: a
       resource is INFERRED while its shape is a guess, INDICATED once
       drilling constrains it, and MEASURED only when the drilling is dense
       enough to bet on.

       Measured on the real 120 m column: seven well-spread full-depth holes
       clear the gate, and DEPTH is what buys that -- 15 holes if each stops
       at 80% of the column, 28 at half, 35 at a tenth. Shallow drilling is
       five times the work, not a wall: it climbs slowly and does clear
       eventually, which is the intended shape and not the "plateau" the
       older notes claimed. */
    float Delineation(int lattice, float columnM) const
    {
        return DashKnow_Delineation(&k, lattice, columnM);
    }
    const char* Tier(int lattice, float columnM) const
    {
        return DashKnow_Tier(&k, lattice, columnM);
    }
    bool IsMeasured(int lattice, float columnM) const
    {
        return DashKnow_IsMeasured(&k, lattice, columnM);
    }

    void Add(float i, float j, float depthM) { DashKnow_Add(&k, i, j, depthM); }
    void Clear() { DashKnow_Clear(&k); }

    int Revision() const { return k.revision; }
    int Count() const { return k.count; }

    // The C model this is a face on, for anything that has to hand it
    // straight to the console.
    DashKnowledge&       Raw()       { return k; }
    const DashKnowledge& Raw() const { return k; }

private:
    // Mutable because Delineation caches against the revision and is called
    // from const draw code -- the same reason the old cachedDelineation was.
    mutable DashKnowledge k = {};
};
