#include "prospecting_grid.h"
#include "sample_tray.h"
#include <cmath>
#include <algorithm>

ProspectingGrid::ProspectingGrid(int tier, int parentGridX, int parentGridY,
                                  ResourceManager& resourceManager)
    : tier(tier)
    , gridSize(GetGridSizeForTier(tier))
    , parentGridX(parentGridX)
    , parentGridY(parentGridY)
    , resourceManager(resourceManager)
{
    AllocateGrid();
    GenerateSubCellDistribution();
}

int ProspectingGrid::GetGridSize() const { return gridSize; }
int ProspectingGrid::GetTier() const { return tier; }
int ProspectingGrid::GetParentGridX() const { return parentGridX; }
int ProspectingGrid::GetParentGridY() const { return parentGridY; }

const SubCell& ProspectingGrid::GetSubCell(int x, int y) const
{
    return cells[y][x];
}

SubCell& ProspectingGrid::GetSubCellMut(int x, int y)
{
    return cells[y][x];
}

std::map<ResourceType, float> ProspectingGrid::GetGroundTruth(int subX, int subY,
                                                               DepthLayer depth) const
{
    int d = static_cast<int>(depth);
    if (d < 0 || d > 3) return {};
    if (subY < 0 || subY >= gridSize || subX < 0 || subX >= gridSize) return {};
    return subCellResources[d][subY][subX];
}

bool ProspectingGrid::IsInReach(int subX, int subY) const
{
    // UNGATED. Prospecting's reach ring was the same species of wall the
    // depth gate was -- tier 0 had four live blocks out of 64. Deleted per
    // progression-design.md #1; the whole lattice is open and depth/distance
    // cost is what prices ambition. The free IsSubCellInReach survives for
    // EXCAVATION, which reads reach with its own tier -- hauling distance is
    // a different question from where an instrument may look.
    return subX >= 0 && subX < gridSize && subY >= 0 && subY < gridSize;
}

int ProspectingGrid::GetReach() const
{
    return gridSize;
}

float ProspectingGrid::GetQuantity(int subX, int subY, DepthLayer depth) const
{
    int d = static_cast<int>(depth);
    if (d < 0 || d > 3) return 0.0f;
    if (subY < 0 || subY >= gridSize || subX < 0 || subX >= gridSize) return 0.0f;
    return subCellQuantities[d][subY][subX];
}

float ProspectingGrid::GetTotalRichness(int subX, int subY) const
{
    float total = 0.0f;
    int maxDepth = MAX_DEPTH_PER_TIER[tier];

    for (int d = 0; d < maxDepth; d++)
    {
        total += GetQuantity(subX, subY, static_cast<DepthLayer>(d));
    }
    return total;
}

void ProspectingGrid::RecordCore(int subX, int subY, DepthLayer depth)
{
    if (subX < 0 || subX >= gridSize || subY < 0 || subY >= gridSize) return;
    int d = static_cast<int>(depth);
    if (d < 0 || d > 3) return;

    // Permanent. A core is rock you are holding; the knowledge it bought must
    // never depend on whether the specimen still sits in the tray.
    cells[subY][subX].cored[d] = true;
}

void ProspectingGrid::RecordExcavation(int subX, int subY, DepthLayer depth,
                                       float fraction)
{
    if (subX < 0 || subX >= gridSize || subY < 0 || subY >= gridSize) return;

    int d = static_cast<int>(depth);
    if (d < 0 || d > 3) return;
    if (fraction <= 0.0f) return;

    SubCell& cell = cells[subY][subX];
    float worked = cell.workedFraction[d] + fraction;
    cell.workedFraction[d] = worked > 1.0f ? 1.0f : worked;
}

float ProspectingGrid::GetExcavatedKnowledge(int subX, int subY) const
{
    if (subX < 0 || subX >= gridSize || subY < 0 || subY >= gridSize) return 0.0f;

    const SubCell& cell = cells[subY][subX];

    // Each layer dug is a quarter of the column observed directly.
    int dug = 0;
    for (int d = 0; d < 4; d++)
    {
        if (cell.HasBeenDug(d)) dug++;
    }
    return static_cast<float>(dug) / 4.0f;
}

void ProspectingGrid::RecordSweep(int frequencyBand, float energyCost, float timestamp)
{
    sweepHistory.push_back({ frequencyBand, energyCost, timestamp });
}

const std::vector<SweepRecord>& ProspectingGrid::GetSweepHistory() const
{
    return sweepHistory;
}

bool ProspectingGrid::HasSweptFrequency(int frequencyBand) const
{
    for (const auto& record : sweepHistory)
    {
        if (record.frequencyBand == frequencyBand) return true;
    }
    return false;
}

void ProspectingGrid::ResizeForTier(int newTier)
{
    // The lattice is fixed, so a tier change only extends reach. Nothing is
    // reallocated -- which is what preserves sweep data, confidence, and the
    // sub-cell links held by collected samples across an upgrade. (The
    // previous size-changing grid wiped all of that on every tier-up.)
    tier = newTier;

    // No regeneration needed: depth is ungated, so every layer is generated
    // up front. (A hack briefly lived here refilling layers the old depth
    // gate had skipped; the gate is gone and so is the hack.)
}

void ProspectingGrid::AllocateGrid()
{
    cells.assign(gridSize, std::vector<SubCell>(gridSize));

    for (int d = 0; d < 4; d++)
    {
        subCellResources[d].assign(gridSize,
            std::vector<std::map<ResourceType, float>>(gridSize));
        subCellQuantities[d].assign(gridSize, std::vector<float>(gridSize, 0.0f));
    }
}

void ProspectingGrid::GenerateSubCellDistribution()
{
    int maxDepth = MAX_DEPTH_PER_TIER[tier];

    for (int d = 0; d < maxDepth; d++)
    {
        DepthLayer depth = static_cast<DepthLayer>(d);
        auto parentResources = resourceManager.GetResourcesAtGridLayer(
            parentGridX, parentGridY, depth);
        GenerateLayerDistribution(depth, parentResources);
    }

    // ResourceManager works in absolute quantities (hundreds to thousands per
    // cell); the prospecting chain works in composition fractions. Record the
    // absolute total per sub-cell, then normalize the per-element values into
    // fractions that sum to 1.
    for (int d = 0; d < maxDepth; d++)
    {
        for (int y = 0; y < gridSize; y++)
        {
            for (int x = 0; x < gridSize; x++)
            {
                auto& cellResources = subCellResources[d][y][x];

                float total = 0.0f;
                for (const auto& [type, quantity] : cellResources)
                {
                    total += quantity;
                }
                subCellQuantities[d][y][x] = total;

                if (total > 0.0f)
                {
                    for (auto& [type, quantity] : cellResources)
                    {
                        quantity /= total;
                    }
                }
            }
        }
    }
}

void ProspectingGrid::GenerateLayerDistribution(
    DepthLayer depth,
    const std::vector<std::pair<ResourceType, float>>& parentResources)
{
    // =======================================================================
    // THE GROUND IS THREE-DIMENSIONAL.
    //
    // An ore body is a SHOOT: an ellipsoid of elevated grade with a position,
    // three radii and an ORIENTATION (strike + dip), generated once per
    // (parent cell, resource) -- depth is NOT in the seed. The old generator
    // seeded per layer, so the four layers were four unrelated 2D maps and an
    // angled hole sampled uncorrelated ground: aiming bought literally
    // nothing. Now a dipping body's footprint walks sideways as you go down,
    // which is the entire reason "which way do I point the drill" is a
    // question. See docs/design/subsurface/subsurface-model.md.
    //
    // Orientation is the point, so it is drawn from a realistic band: dips of
    // 15-65 degrees, where most real ore bodies live. Steeper bodies reward a
    // vertical hole; shallow blades reward drilling along them (+90% measured
    // in the prototype). Vertical is an aim, not a default.
    //
    // The per-layer normalisation and SUBCELL_VARIATION clamps are unchanged:
    // the 3D field supplies the PATTERN, the per-layer pass keeps quantities
    // calibrated against the parent cell's depth-biased abundances exactly as
    // before, so every downstream consumer sees the same units it always did.
    // =======================================================================

    int d = static_cast<int>(depth);
    int resourceIdx = 0;
    const float zMetres = LAYER_CENTRE_M[d];

    for (const auto& [type, abundance] : parentResources)
    {
        if (abundance < 0.001f)
        {
            resourceIdx++;
            continue;
        }

        // One seed per (cell, resource): depth index fixed at 0, so every
        // layer samples the SAME 3D field.
        uint32_t seed = HashSeed(parentGridX, parentGridY, 0, resourceIdx);

        // 1-2 shoots per resource, as before -- but now with a 3D centre,
        // metre-scaled radii and an orientation.
        seed = LCG(seed);
        int numShoots = 1 + (seed % 2);

        float cx[2], cy[2], cz[2], rLong[2], rAcross[2], rThick[2];
        float strikeRad[2], dipRad[2];
        for (int c = 0; c < numShoots; c++)
        {
            seed = LCG(seed); cx[c] = (seed % 1000) / 1000.0f * 100.0f;   // m
            seed = LCG(seed); cy[c] = (seed % 1000) / 1000.0f * 100.0f;   // m
            seed = LCG(seed); cz[c] = 5.0f + (seed % 1000) / 1000.0f * 105.0f;
            seed = LCG(seed); rLong[c]   = 20.0f + (seed % 1000) / 1000.0f * 30.0f;
            seed = LCG(seed); rAcross[c] = 10.0f + (seed % 1000) / 1000.0f * 15.0f;
            seed = LCG(seed); rThick[c]  =  8.0f + (seed % 1000) / 1000.0f * 10.0f;
            seed = LCG(seed); strikeRad[c] = (seed % 1000) / 1000.0f * 6.2831853f;
            seed = LCG(seed); dipRad[c]  = (15.0f + (seed % 1000) / 1000.0f * 50.0f)
                                           * 0.0174533f;
        }

        float weights[PROSPECTING_MAX_GRID_SIZE][PROSPECTING_MAX_GRID_SIZE];
        float totalWeight = 0.0f;

        for (int y = 0; y < gridSize; y++)
        {
            for (int x = 0; x < gridSize; x++)
            {
                float px = (x + 0.5f) * SUBCELL_SIZE_M;
                float py = (y + 0.5f) * SUBCELL_SIZE_M;

                float maxInfluence = 0.1f;
                for (int c = 0; c < numShoots; c++)
                {
                    // The shoot's own axes: e1 = long axis plunging at dip
                    // along strike, e2 = horizontal across-strike, e3 = the
                    // remaining thickness direction.
                    float ca = cosf(strikeRad[c]), sa = sinf(strikeRad[c]);
                    float cd = cosf(dipRad[c]),   sd = sinf(dipRad[c]);
                    float dx = px - cx[c];
                    float dy = py - cy[c];
                    float dz = zMetres - cz[c];

                    float u = ( dx * sa * cd + dy * ca * cd + dz * sd) / rLong[c];
                    float v = (-dx * ca      + dy * sa               ) / rAcross[c];
                    float w = (-dx * sa * sd - dy * ca * sd + dz * cd) / rThick[c];

                    float influence = expf(-(u * u + v * v + w * w) / 2.0f);
                    if (influence > maxInfluence)
                        maxInfluence = influence;
                }

                weights[y][x] = maxInfluence;
                totalWeight += maxInfluence;
            }
        }

        // Normalise so mean weight = 1.0, then clamp -- unchanged from the
        // 2D generator, so quantities stay calibrated per layer.
        float avgWeight = totalWeight / (gridSize * gridSize);
        if (avgWeight < 0.001f) avgWeight = 0.001f;

        for (int y = 0; y < gridSize; y++)
        {
            for (int x = 0; x < gridSize; x++)
            {
                float w = weights[y][x] / avgWeight;
                if (w < SUBCELL_VARIATION_MIN) w = SUBCELL_VARIATION_MIN;
                if (w > SUBCELL_VARIATION_MAX) w = SUBCELL_VARIATION_MAX;
                subCellResources[d][y][x][type] = abundance * w;
            }
        }

        resourceIdx++;
    }
}

uint32_t ProspectingGrid::HashSeed(int px, int py, int depth, int resourceIdx)
{
    uint32_t h = 2166136261u;
    h ^= static_cast<uint32_t>(px);
    h *= 16777619u;
    h ^= static_cast<uint32_t>(py);
    h *= 16777619u;
    h ^= static_cast<uint32_t>(depth);
    h *= 16777619u;
    h ^= static_cast<uint32_t>(resourceIdx);
    h *= 16777619u;
    return h;
}

uint32_t ProspectingGrid::LCG(uint32_t seed)
{
    return seed * 1664525u + 1013904223u;
}

// ---------------------------------------------------------------------------
// Per-depth confidence, classification and roll-up
//
// Moved here from Excavation/site_view.cpp unchanged: every input is
// prospecting state, and two copies of this arithmetic is exactly the drift
// the single-threshold rule exists to prevent.
// ---------------------------------------------------------------------------

// 3D distance between two (sub-cell, depth-layer) points, in metres.
static float PointSeparationM(int x1, int y1, int d1, int x2, int y2, int d2)
{
    float dx = (x1 - x2) * SUBCELL_SIZE_M;
    float dy = (y1 - y2) * SUBCELL_SIZE_M;
    float dz = LAYER_CENTRE_M[d1] - LAYER_CENTRE_M[d2];
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

float GetDepthConfidence(const ProspectingGrid& grid, const SampleTray& tray,
                         int x, int y, DepthLayer depth)
{
    (void)tray;   // knowledge lives on the grid now; signature kept for the
                  // many call sites (excavation's SiteView included)

    int size = grid.GetGridSize();
    if (x < 0 || x >= size || y < 0 || y >= size) return 0.0f;

    int d = static_cast<int>(depth);
    if (d < 0 || d > 3) return 0.0f;

    const SubCell& cell = grid.GetSubCell(x, y);

    // --- Direct observation: dug or cored, this exact spot and depth ---
    // Per DEPTH: a core through the surface says nothing about what lies
    // under it beyond the falloff below.
    if (cell.HasBeenDug(d)) return 1.0f;
    if (cell.HasCore(d))    return 1.0f;

    // --- Support from the NEAREST core, never a weight sum ---
    // support = exp(-(d_nearest / RANGE)^2). Measured going the wrong way in
    // the prototype when a weight sum was used: distant rich holes out-voted
    // the prior and error ROSE on adding a hole. Nearest-only cannot do that.
    // At RANGE = 20 m the lattice falls out: neighbour 12.5 m -> 0.68
    // (INDICATED), two cells 25 m -> 0.21 (INFERRED), three cells -> 0.03.
    // Adjacent depth layers step 0.49 / 0.14 / 0.01 -- deep stays a bet.
    // Cores and dug faces both teach, at different reaches: a core speaks for
    // ESTIMATE_RANGE_M of ground, a working face for half that -- calibrated
    // by colony_sim, see EXCAVATION_SUPPORT_RANGE_M.
    float nearestCore = 1.0e9f, nearestDug = 1.0e9f;
    for (int cy = 0; cy < size; cy++)
    {
        for (int cx = 0; cx < size; cx++)
        {
            const SubCell& c = grid.GetSubCell(cx, cy);
            for (int cd = 0; cd < 4; cd++)
            {
                bool isCore = c.cored[cd];
                bool isDug  = c.workedFraction[cd] > 0.0f;
                if (!isCore && !isDug) continue;
                float sep = PointSeparationM(x, y, d, cx, cy, cd);
                if (isCore && sep < nearestCore) nearestCore = sep;
                if (isDug  && sep < nearestDug)  nearestDug  = sep;
            }
        }
    }

    float coreSupport = 0.0f;
    if (nearestCore < 1.0e8f)
    {
        float r = nearestCore / ESTIMATE_RANGE_M;
        coreSupport = expf(-r * r);
    }
    if (nearestDug < 1.0e8f)
    {
        float r = nearestDug / EXCAVATION_SUPPORT_RANGE_M;
        coreSupport = std::max(coreSupport, expf(-r * r));
    }

    // --- Sweep evidence: LIBS, surface chemistry only ---
    // Capped below the Inferred threshold: a wide surface reading may make
    // ground look interesting, never make it count.
    float sweepConfidence = 0.0f;
    if (cell.hasBeenSwept && cell.sweepFrequencyBand >= 0)
    {
        int band = cell.sweepFrequencyBand;
        if (band < 0) band = 0;
        if (band > SWEEP_FREQUENCY_BANDS - 1) band = SWEEP_FREQUENCY_BANDS - 1;

        if (d < SWEEP_DEPTH_PENETRATION[band])
        {
            float attenuation = 1.0f / (1.0f + d * SWEEP_DEPTH_ATTENUATION);
            sweepConfidence = cell.aggregateConfidence * attenuation *
                              PROSPECT_SWEEP_CONFIDENCE_WEIGHT;
            sweepConfidence = std::min(sweepConfidence,
                                       PROSPECT_SWEEP_CONFIDENCE_CAP);
        }
    }

    // Independent evidence combines rather than replaces.
    float combined = 1.0f - (1.0f - sweepConfidence) * (1.0f - coreSupport);
    if (combined < 0.0f) combined = 0.0f;
    if (combined > 1.0f) combined = 1.0f;
    return combined;
}

float GetSubCellYield(const ProspectingGrid& grid, int x, int y,
                      DepthLayer depth, ResourceType type)
{
    // quantity (absolute) x composition (fraction). Neither alone is a yield.
    float quantity = grid.GetQuantity(x, y, depth);
    if (quantity <= 0.0f) return 0.0f;

    std::map<ResourceType, float> composition = grid.GetGroundTruth(x, y, depth);
    auto it = composition.find(type);
    if (it == composition.end()) return 0.0f;

    return quantity * it->second;
}

// =======================================================================
// THE ESTIMATE FIELD (block-model-design.md #3)
//
// The player never sees ground truth. They see an ESTIMATE, and how much to
// trust it. Grade is inverse-distance-weighted from every core, pulled
// toward the prior where there is no evidence; trust is the nearest-core
// support computed in GetDepthConfidence.
//
//   estimate(p) = lerp( prior(p),  sum(w_i * g_i) / sum(w_i),  support(p) )
//                 with w_i = 1 / d_i^POWER
//
// The prior is honest about the instruments: an unswept layer's prior is the
// LAYER MEAN (you know only the cell average -- the literal wording of
// UNCLASSIFIED); a swept SURFACE layer gets the lateral pattern LIBS really
// reads. LIBS is blind below the regolith, so deeper layers keep the flat
// mean no matter how much you sweep. Cores are the only way down.
// =======================================================================

float GetEstimatedYield(const ProspectingGrid& grid, int x, int y,
                        DepthLayer depth, ResourceType type)
{
    int size = grid.GetGridSize();
    if (x < 0 || x >= size || y < 0 || y >= size) return 0.0f;
    int d = static_cast<int>(depth);
    if (d < 0 || d > 3) return 0.0f;

    // Direct observation is not an estimate.
    const SubCell& self = grid.GetSubCell(x, y);
    if (self.cored[d] || self.workedFraction[d] > 0.0f)
    {
        return GetSubCellYield(grid, x, y, depth, type);
    }

    // --- The prior ---
    float layerMean = 0.0f;
    for (int py = 0; py < size; py++)
        for (int px = 0; px < size; px++)
            layerMean += GetSubCellYield(grid, px, py, depth, type);
    layerMean /= static_cast<float>(size * size);

    float prior = layerMean;
    if (d == 0 && self.hasBeenSwept)
    {
        // LIBS saw the surface here: the prior carries the real lateral
        // pattern, blurred by trusting it only partway.
        float here = GetSubCellYield(grid, x, y, depth, type);
        prior = layerMean + (here - layerMean) * 0.6f;
    }

    // --- IDW over every cored / dug point ---
    float wSum = 0.0f, gSum = 0.0f;
    for (int cy = 0; cy < size; cy++)
    {
        for (int cx = 0; cx < size; cx++)
        {
            const SubCell& c = grid.GetSubCell(cx, cy);
            for (int cd = 0; cd < 4; cd++)
            {
                if (!c.cored[cd] && !(c.workedFraction[cd] > 0.0f)) continue;
                float sep = PointSeparationM(x, y, d, cx, cy, cd);
                if (sep < 0.5f) sep = 0.5f;
                float w = 1.0f / powf(sep, ESTIMATE_IDW_POWER);
                wSum += w;
                gSum += w * GetSubCellYield(grid, cx, cy,
                                            static_cast<DepthLayer>(cd), type);
            }
        }
    }

    if (wSum <= 0.0f) return prior;

    // Support from the nearest core decides how far the data may out-vote
    // the prior -- same kernel as GetDepthConfidence, minus the sweep term.
    float nearestCore = 1.0e9f, nearestDug = 1.0e9f;
    for (int cy = 0; cy < size; cy++)
        for (int cx = 0; cx < size; cx++)
        {
            const SubCell& c = grid.GetSubCell(cx, cy);
            for (int cd = 0; cd < 4; cd++)
            {
                bool isCore = c.cored[cd];
                bool isDug  = c.workedFraction[cd] > 0.0f;
                if (!isCore && !isDug) continue;
                float sep = PointSeparationM(x, y, d, cx, cy, cd);
                if (isCore && sep < nearestCore) nearestCore = sep;
                if (isDug  && sep < nearestDug)  nearestDug  = sep;
            }
        }
    float rc = nearestCore / ESTIMATE_RANGE_M;
    float rd = nearestDug / EXCAVATION_SUPPORT_RANGE_M;
    float support = std::max(expf(-rc * rc), expf(-rd * rd));

    return prior + (gSum / wSum - prior) * support;
}

float ClassSplit::Total() const
{
    return measured + indicated + inferred + unclassified;
}

float ClassSplit::Committable() const
{
    return measured + indicated;
}

float ClassSplit::Get(ResourceClass cls) const
{
    switch (cls)
    {
        case ResourceClass::MEASURED:  return measured;
        case ResourceClass::INDICATED: return indicated;
        case ResourceClass::INFERRED:  return inferred;
        default:                       return unclassified;
    }
}

ClassSplit GetClassSplit(const ProspectingGrid& grid, const SampleTray& tray,
                         ResourceType type, int tier)
{
    // KNOWLEDGE-SCOPED, not window-scoped. Reach and depth are ungated, so
    // the statement covers the whole lattice at every depth; what varies is
    // how well each block is known, which the class already expresses. The
    // tier parameter is kept for the call sites and ignored.
    (void)tier;

    ClassSplit split;
    int size = grid.GetGridSize();

    for (int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++)
        {
            for (int d = 0; d < 4; d++)
            {
                DepthLayer depth = static_cast<DepthLayer>(d);

                // A statement is built from the ESTIMATE, not from ground
                // truth -- tonnage you have not measured is a belief.
                float yield = GetEstimatedYield(grid, x, y, depth, type);
                if (yield <= 0.0f) continue;

                float confidence = GetDepthConfidence(grid, tray, x, y, depth);

                switch (GetResourceClass(confidence))
                {
                    case ResourceClass::MEASURED:  split.measured     += yield; break;
                    case ResourceClass::INDICATED: split.indicated    += yield; break;
                    case ResourceClass::INFERRED:  split.inferred     += yield; break;
                    default:                       split.unclassified += yield; break;
                }
            }
        }
    }

    return split;
}
