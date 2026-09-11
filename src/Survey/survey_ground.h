#pragma once

#include "survey_constants.h"
#include "survey_knowledge.h"

#include <vector>

/* =====================================================================
   THE GROUND ITSELF -- generated, not measured
   ---------------------------------------------------------------------
   Four beds on a lattice. Each interface is one height field:
   undulation (shared between beds by `conform`, so beds either follow
   each other or go their own way), a regional dip that fans with depth,
   and a buried impact bowl progressively filled in above the interface
   it was cut into.

   On top of that sits the ERROR MODEL, which is the point of the whole
   thing. Each interface carries a wrongness -- a bulk shift, a tilt and
   two octaves of noise, scaled by the thinner of the two beds it
   separates so a thin bed is never given an error that would swallow
   it -- and that wrongness is multiplied by 1 - confidence at every
   node. So what the console draws is not the truth, it is the current
   ESTIMATE, and every hole visibly re-fits it: beds rise, dips shallow
   out, a level bed turns out to roll.

   Pure logic. No rendering, no input, no raylib beyond the shared
   constants header.
   ===================================================================== */

// A hole's scour, in lattice coordinates. `progress` runs 0..1 as the bowl
// is driven down, so a cut in flight deepens its own crater.
struct SurveyScour
{
    float i = 0.0f;
    float j = 0.0f;
    float progress = 0.0f;
};

class SurveyGround
{
public:
    SurveyGround();

    // Regenerates every interface. Cheap enough to call when a hole lands or
    // a scour deepens, and far too expensive to call per frame -- the caller
    // steps it on a revision, not on a clock.
    void Build(const SurveyKnowledge& knowledge, const std::vector<SurveyScour>& scours);

    int   Lattice() const { return lattice; }
    int   Stride() const { return lattice + 1; }
    float ColumnM() const { return columnM; }
    // The five interface depths in metres: 0 is the ground you stand on,
    // 4 is the base of the surveyed column.
    float EdgeM(int interfaceIndex) const { return edgeM[interfaceIndex]; }

    // Depth of an interface at a lattice node, in metres below datum.
    float DepthAt(int interfaceIndex, int i, int j) const
    {
        return surf[interfaceIndex][j * Stride() + i];
    }
    // The same field as a continuous surface, for anything that sits on the
    // ground at a fractional cell (a reticle, a rig, a pick).
    float SampleDepth(int interfaceIndex, float u, float v) const;

private:
    void BuildCraters();
    float Bowl(float u, float v) const;
    float CraterAmp(int interfaceIndex) const;

    struct Crater { float x, y, r, d; };

    int   lattice = SURVEY_LATTICE;
    float columnM = 2000.0f;
    float edgeM[SURVEY_BEDS + 1] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    std::vector<float> surf[SURVEY_BEDS + 1];
    std::vector<Crater> craters;
};
