#pragma once

#include <vector>

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
   Pure logic: no rendering, no input, no raylib.
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

       Depth is the one hard edge. A hole that stopped at 300 m has seen
       nothing at 1 km, so its weight decays to a fifth below its own
       bottom -- a fifth and not zero, because beds continue, and that is
       an inference the console is entitled to make. */
    float KnowAt(float i, float j, float depthM) const;

    // Knowledge rescaled so the last 5% of fog is not worth arguing about:
    // once the fog is that thin the model is called settled and stops
    // moving. Without a ceiling the interfaces would creep for ever by
    // amounts too small to see and too large to ignore.
    static float Confidence(float known);

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
       enough to bet on. About seven well-spread full-depth holes clear 95%;
       a 3x3 grid reaches 99.8%; shallow holes alone plateau, which is
       correct, since nothing has been established about the bottom half of
       the column. */
    float Delineation(int lattice, float columnM) const;
    const char* Tier(int lattice, float columnM) const;
    bool IsMeasured(int lattice, float columnM) const;

    void Add(float i, float j, float depthM);
    void Clear();

    int  Revision() const { return revision; }
    int  Count() const { return static_cast<int>(reveals.size()); }
    const std::vector<SurveyReveal>& Reveals() const { return reveals; }

private:
    std::vector<SurveyReveal> reveals;
    int revision = 0;
    // Delineation walks every hole at 567 sample points, so it is cached
    // against the revision rather than recomputed per frame.
    mutable int   cachedRevision = -1;
    mutable float cachedDelineation = 0.0f;
};
