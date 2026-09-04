// Measures the real footprint of an ore body on the prospecting lattice.
//
// Written to answer one question from excavation-design.md Phase 9b: an access
// shaft opens a 3x3 block of sub-cells -- is that the right size? If a typical
// ore body is 2x2 the shaft trivialises siting; if it is 5x5 one shaft is never
// enough and access becomes a tax. Nobody had measured it.
//
// Uses the REAL generator (ProspectingGrid over ResourceManager), not a Python
// model of it, so the numbers are the ones the game produces.
//
// The field measured is targeted-resource yield per sub-cell:
//
//     yield(x,y,d,t) = GetQuantity(x,y,d) * GetGroundTruth(x,y,d)[t]
//
// which module-architecture.md Part II calls the product worth optimising --
// quantity alone is much flatter, fractions alone are not a tonnage.
//
// Build & run:
//   cmake --build build --target colony_measure_clusters
//   ./build/src/colony_measure_clusters            # 400 parent cells
//   ./build/src/colony_measure_clusters 900        # a wider sample

#include "resource_manager.h"
#include "prospecting_grid.h"
#include "game_constants.h"

#include <cstdio>
#include <cstdlib>
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>

// Keep in sync with PREVIEW_MAP_SEED in tools/preview/preview_main.cpp
static const unsigned int MEASURE_MAP_SEED = 20260813u;

static const int G = PROSPECTING_GRID_SIZE;
// Footprints worth asking about on this lattice, in sub-cells. At
// SUBCELL_SIZE_M each, 3 is a real shaft (9.4 m) and 12 is what the old
// 8x8 design meant by "3x3" (37.5 m) -- both are on the table so the
// choice is made against numbers rather than against a stale name.
static const int WIN[] = { 2, 3, 4, 6, 8, 12, 16 };
static const int NWIN = static_cast<int>(sizeof(WIN) / sizeof(WIN[0]));

struct Accum
{
    double sum = 0.0;
    double sumSq = 0.0;
    long   n = 0;
    double lo = 1e30;
    double hi = -1e30;

    void Add(double v)
    {
        sum += v; sumSq += v*v; n++;
        if (v < lo) lo = v;
        if (v > hi) hi = v;
    }
    double Mean() const { return n ? sum / n : 0.0; }
    double Sd() const
    {
        if (n < 2) return 0.0;
        double m = Mean();
        double var = sumSq / n - m*m;
        return var > 0.0 ? std::sqrt(var) : 0.0;
    }
};

// Best-placed NxN window: the largest share of the lattice's total yield that
// any NxN block of sub-cells can capture. This is exactly what a shaft of that
// footprint, sited perfectly, would open.
static double BestWindowShare(const float f[][G], int N, double total)
{
    if (total <= 0.0) return 0.0;
    double best = 0.0;
    for (int oy = 0; oy + N <= G; oy++)
    {
        for (int ox = 0; ox + N <= G; ox++)
        {
            double s = 0.0;
            for (int y = oy; y < oy + N; y++)
                for (int x = ox; x < ox + N; x++)
                    s += f[y][x];
            if (s > best) best = s;
        }
    }
    return best / total;
}

// Dumps one raw field so the summary above can be checked against numbers
// rather than trusted. Also tests the algebra claim in
// docs/design/excavation/implementation-plan.md Section 2:
//     GetQuantity * GetGroundTruth[t]  ==  abundance_t * w_t
// If that holds, the field's max/mean must respect SUBCELL_VARIATION_MAX.
static void DumpOneField(ResourceManager& rm, int gx, int gy)
{
    ProspectingGrid grid(3, gx, gy, rm);
    auto probe = grid.GetGroundTruth(0, 0, DepthLayer::SURFACE);
    if (probe.empty()) { printf("no resources at (%d,%d)\n", gx, gy); return; }

    ResourceType t = probe.begin()->first;
    printf("\n--- raw dump: parent cell (%d,%d), SURFACE, resource %s ---\n",
           gx, gy, ResourceTypeToString(t));

    float f[G][G];
    double total = 0.0, hi = 0.0, lo = 1e30;
    for (int y = 0; y < G; y++)
    {
        for (int x = 0; x < G; x++)
        {
            auto comp = grid.GetGroundTruth(x, y, DepthLayer::SURFACE);
            float q = grid.GetQuantity(x, y, DepthLayer::SURFACE);
            auto it = comp.find(t);
            f[y][x] = (it == comp.end()) ? 0.0f : q * it->second;
            total += f[y][x];
            if (f[y][x] > hi) hi = f[y][x];
            if (f[y][x] < lo) lo = f[y][x];
        }
    }
    // recover w by dividing out the parent cell's abundance for this resource
    float abundance = 0.0f;
    for (const auto& pr : rm.GetResourcesAtGridLayer(gx, gy, DepthLayer::SURFACE))
        if (pr.first == t) abundance = pr.second;

    double mean = total / (G*G);
    printf("   parent abundance for this resource: %.1f\n", abundance);
    printf("     yield per sub-cell (absolute)            as a multiple of the mean\n");
    for (int y = 0; y < G; y++)
    {
        printf("   ");
        for (int x = 0; x < G; x++) printf("%7.0f", f[y][x]);
        printf("     ");
        for (int x = 0; x < G; x++) printf("%6.2f", f[y][x]/mean);
        printf("\n");
    }
    printf("\n   mean %.0f   min %.0f (%.2fx)   max %.0f (%.2fx)\n",
           mean, lo, lo/mean, hi, hi/mean);
    if (abundance > 0.0f)
    {
        printf("   recovered w:  min %.3f   max %.3f   mean %.3f"
               "   (clamp %.1f-%.1f)\n",
               lo/abundance, hi/abundance, mean/abundance,
               SUBCELL_VARIATION_MIN, SUBCELL_VARIATION_MAX);
    }
    printf("\n   Normalisation sets the PRE-clamp mean of w to 1.0. Clamping the\n");
    printf("   peak DOWN to %.1f then pulls the post-clamp mean below 1.0, which is\n",
           SUBCELL_VARIATION_MAX);
    printf("   why max/mean comes out above %.1f.\n\n", SUBCELL_VARIATION_MAX);
}

int main(int argc, char** argv)
{
    int cells = (argc > 1) ? atoi(argv[1]) : 400;
    if (cells < 1) cells = 1;

    // The constructor only allocates; Planet normally calls GenerateResourceMap.
    ResourceManager rm(PLANET_SIZE, SECT_CORE_RADIUS * 2.0f);
    rm.GenerateResourceMap(MEASURE_MAP_SEED);

    // per-window-size capture share
    Accum share[NWIN];           // index into WIN[]
    Accum richCount;             // sub-cells at >= 1.5x the lattice mean
    Accum richBoxW, richBoxH;    // bounding box of that rich ground
    Accum peakShare;             // share held by the single best sub-cell
    Accum bestOverMean;          // best sub-cell / mean sub-cell
    long  bodies = 0;
    long  richContiguousFits3 = 0;   // rich bounding box fits inside 3x3
    long  richContiguousFits4 = 0;
    Accum wLo, wHi, wMean;           // the weight field, abundance divided out
    long  minClampBound = 0;         // fields where w actually reached the MIN clamp
    long  maxClampBound = 0;

    const DepthLayer depths[4] = { DepthLayer::SURFACE, DepthLayer::SHALLOW,
                                   DepthLayer::MID, DepthLayer::DEEP };

    int side = static_cast<int>(std::sqrt(static_cast<double>(cells)));
    if (side < 1) side = 1;

    for (int py = 0; py < side; py++)
    {
        for (int px = 0; px < side; px++)
        {
            int gx = px % PLANET_SIZE;
            int gy = py % PLANET_SIZE;

            ProspectingGrid grid(3, gx, gy, rm);

            // parent abundances per depth, so w can be recovered from the yield
            std::map<ResourceType, float> abund[4];
            for (int di = 0; di < 4; di++)
                for (const auto& pr : rm.GetResourcesAtGridLayer(gx, gy, depths[di]))
                    abund[di][pr.first] = pr.second;

            // which resources are actually present in this parent cell
            std::map<ResourceType, float> probe = grid.GetGroundTruth(0, 0, DepthLayer::SURFACE);

            for (int di = 0; di < 4; di++)
            {
                for (const auto& kv : probe)
                {
                    ResourceType t = kv.first;

                    float f[G][G];
                    double total = 0.0;
                    for (int y = 0; y < G; y++)
                    {
                        for (int x = 0; x < G; x++)
                        {
                            auto comp = grid.GetGroundTruth(x, y, depths[di]);
                            float q = grid.GetQuantity(x, y, depths[di]);
                            auto it = comp.find(t);
                            float v = (it == comp.end()) ? 0.0f : q * it->second;
                            f[y][x] = v;
                            total += v;
                        }
                    }
                    if (total <= 0.0) continue;

                    bodies++;
                    double mean = total / (G*G);

                    // recover w = yield / abundance and see which clamp binds
                    auto ab = abund[di].find(t);
                    if (ab != abund[di].end() && ab->second > 0.0f)
                    {
                        double a = ab->second, wlo = 1e30, whi = 0.0, wsum = 0.0;
                        for (int y = 0; y < G; y++)
                            for (int x = 0; x < G; x++)
                            {
                                double w = f[y][x] / a;
                                wlo = std::min(wlo, w);
                                whi = std::max(whi, w);
                                wsum += w;
                            }
                        wLo.Add(wlo); wHi.Add(whi); wMean.Add(wsum/(G*G));
                        if (wlo <= SUBCELL_VARIATION_MIN + 1e-4) minClampBound++;
                        if (whi >= SUBCELL_VARIATION_MAX - 1e-4) maxClampBound++;
                    }

                    for (int wi = 0; wi < NWIN; wi++)
                        share[wi].Add(BestWindowShare(f, WIN[wi], total));

                    // rich ground: >= 1.5x mean
                    int cnt = 0, minx = G, maxx = -1, miny = G, maxy = -1;
                    double peak = 0.0;
                    for (int y = 0; y < G; y++)
                    {
                        for (int x = 0; x < G; x++)
                        {
                            if (f[y][x] > peak) peak = f[y][x];
                            if (f[y][x] >= 1.5 * mean)
                            {
                                cnt++;
                                minx = std::min(minx, x); maxx = std::max(maxx, x);
                                miny = std::min(miny, y); maxy = std::max(maxy, y);
                            }
                        }
                    }
                    richCount.Add(cnt);
                    peakShare.Add(peak / total);
                    bestOverMean.Add(peak / mean);
                    if (cnt > 0)
                    {
                        int bw = maxx - minx + 1, bh = maxy - miny + 1;
                        richBoxW.Add(bw); richBoxH.Add(bh);
                        if (bw <= 3 && bh <= 3) richContiguousFits3++;
                        if (bw <= 4 && bh <= 4) richContiguousFits4++;
                    }
                }
            }
        }
    }

    printf("\n");
    printf("=====================================================================\n");
    printf(" ORE BODY FOOTPRINT ON THE %dx%d LATTICE  (sub-cell %.3f m)\n",
           G, G, SUBCELL_SIZE_M);
    printf(" seed %u   parent cells %d   (resource x depth) fields measured %ld\n",
           MEASURE_MAP_SEED, side*side, bodies);
    printf(" field = GetQuantity * GetGroundTruth[resource]  (targeted yield)\n");
    printf("=====================================================================\n\n");

    printf(" Share of a field's total yield captured by the BEST-PLACED window\n");
    printf(" (this is what a shaft of that footprint, sited perfectly, opens)\n\n");
    printf("   window    metres   share of lattice   mean capture   sd      min      max\n");
    for (int wi = 0; wi < NWIN; wi++)
    {
        int N = WIN[wi];
        printf("    %2dx%-2d   %6.1f m       %5.1f%%          %5.1f%%      %4.1f   %5.1f%%   %5.1f%%\n",
               N, N, N * SUBCELL_SIZE_M, (100.0*N*N)/(G*G),
               100.0*share[wi].Mean(), 100.0*share[wi].Sd(),
               100.0*share[wi].lo, 100.0*share[wi].hi);
    }

    printf("\n Rich ground (sub-cells at >= 1.5x the field's mean)\n\n");
    printf("   count of rich sub-cells   mean %.2f   sd %.2f   range %.0f-%.0f\n",
           richCount.Mean(), richCount.Sd(), richCount.lo, richCount.hi);
    printf("   bounding box              mean %.2f x %.2f   widest %.0f x %.0f\n",
           richBoxW.Mean(), richBoxH.Mean(), richBoxW.hi, richBoxH.hi);
    printf("   fits inside 3x3           %.0f%% of fields\n",
           100.0 * richContiguousFits3 / (double)std::max(1L, bodies));
    printf("   fits inside 4x4           %.0f%% of fields\n",
           100.0 * richContiguousFits4 / (double)std::max(1L, bodies));

    printf("\n Single best sub-cell\n\n");
    printf("   holds %.1f%% of the field's yield   (1/%d = %.2f%% if flat)\n",
           100.0*peakShare.Mean(), G*G, 100.0/(G*G));
    printf("   best / mean = %.2fx   (SUBCELL_VARIATION_MAX clamp is %.1f)\n",
           bestOverMean.Mean(), SUBCELL_VARIATION_MAX);

    printf("\n The weight field w, with abundance divided out\n\n");
    printf("   w min    mean %.3f   lowest seen %.3f    (SUBCELL_VARIATION_MIN %.1f)\n",
           wLo.Mean(), wLo.lo, SUBCELL_VARIATION_MIN);
    printf("   w max    mean %.3f   highest seen %.3f   (SUBCELL_VARIATION_MAX %.1f)\n",
           wHi.Mean(), wHi.hi, SUBCELL_VARIATION_MAX);
    printf("   w mean   mean %.3f   -- normalisation targets 1.0 BEFORE clamping\n",
           wMean.Mean());
    printf("\n   MIN clamp binds in %5.1f%% of fields\n",
           100.0*minClampBound/(double)std::max(1L, bodies));
    printf("   MAX clamp binds in %5.1f%% of fields\n",
           100.0*maxClampBound/(double)std::max(1L, bodies));

    DumpOneField(rm, 10, 10);
    return 0;
}
