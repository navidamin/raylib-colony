// domeforge_base.cpp — roads, lunar ground and the base assembly. A 1:1 port of
// dome-forge-base.js. Road primitives are in math coordinates (y up); discs are
// dome rims: they join the road union so the kerbs flow into the rims, but
// carry no lane line.
#include "domeforge.h"
#include "domeforge_util.h"

#include <algorithm>
#include <cctype>
#include <cmath>

using namespace DomeForgeUtil;

DomeForgeRgb DomeForgeHex(const std::string& hexIn)
{
    std::string hex = hexIn;
    while (!hex.empty() && std::isspace((unsigned char)hex.back())) hex.pop_back();
    size_t i = 0;
    while (i < hex.size() && std::isspace((unsigned char)hex[i])) i++;
    hex = hex.substr(i);
    if (!hex.empty() && hex[0] == '#') hex = hex.substr(1);
    if (hex.size() != 6 || !std::all_of(hex.begin(), hex.end(), [](char c) { return std::isxdigit((unsigned char)c); }))
        return {0.5, 0.5, 0.5};
    const long n = std::strtol(hex.c_str(), nullptr, 16);
    return {((n >> 16) & 255) / 255.0, ((n >> 8) & 255) / 255.0, (n & 255) / 255.0};
}

namespace
{
    struct Prep
    {
        DomeForgePrim p;
        double dx = 0, dy = 0, len = 1;
    };

    struct Field
    {
        double d, t, perp;
        int isDisc;
        double dDisc;
    };

    // returns [dUnion, tAlong, perp, isDisc, dDisc]: tAlong/perp belong to the nearest road primitive, dDisc is the
    // distance outside the nearest dome disc (for the rim shadow on the asphalt)
    void RoadField(double x, double y, const std::vector<Prep>& prims, double F, double FD, Field& out)
    {
        double d = 1e9, best = 1e9, t = 0, perp = 0, dRoad = 1e9, dDisc = 1e9;
        int isDisc = 0;
        for (const Prep& q : prims)
        {
            const DomeForgePrim& p = q.p;
            double dp, tt = 0, pp = 0;
            if (p.t == DomeForgePrim::SEG)
            {
                const double px = x - p.ax, py = y - p.ay;
                const double h = (px * q.dx + py * q.dy) / q.len;              // along
                pp = std::fabs(px * q.dy - py * q.dx) / q.len;                 // across
                const double a = pp - p.w / 2, b = std::fabs(h - q.len / 2) - q.len / 2;   // flat-ended box
                dp = std::hypot(std::max(a, 0.0), std::max(b, 0.0)) + std::min(std::max(a, b), 0.0);
                tt = h;
            }
            else if (p.t == DomeForgePrim::RING)
            {
                const double r = std::hypot(x - p.cx, y - p.cy);
                pp = std::fabs(r - p.r);
                dp = pp - p.w / 2;
                tt = std::atan2(y - p.cy, x - p.cx) * p.r;
            }
            else
            {
                dp = std::hypot(x - p.cx, y - p.cy) - p.r;
                pp = 1e9;
                tt = 0;
                if (dp < dDisc) dDisc = dp;
            }
            if (p.t != DomeForgePrim::DISC)
            {
                d = d > 1e8 ? dp : Smin(d, dp, F);
                if (dp < dRoad)
                {
                    dRoad = dp;
                    t = tt;
                    perp = pp;
                }
            }
            else d = d > 1e8 ? dp : Smin(d, dp, FD);
            if (dp < best)
            {
                best = dp;
                isDisc = p.t == DomeForgePrim::DISC ? 1 : 0;
            }
        }
        out = {d, t, perp, isDisc, dDisc};
    }

    std::vector<Prep> PrepPrims(const std::vector<DomeForgePrim>& prims)
    {
        std::vector<Prep> out;
        out.reserve(prims.size());
        for (const DomeForgePrim& p : prims)
        {
            Prep q;
            q.p = p;
            if (p.t == DomeForgePrim::SEG)
            {
                q.dx = p.bx - p.ax;
                q.dy = p.by - p.ay;
                q.len = std::hypot(q.dx, q.dy);
                if (q.len == 0) q.len = 1;
            }
            out.push_back(q);
        }
        return out;
    }

    // JS a % b for doubles is fmod (sign of the dividend).
    inline double Frac01(double v) { return std::fmod(std::fmod(v, 1.0) + 1, 1.0); }
}

namespace
{
    // renderRoads' constants, computed once per job.
    struct RoadCtx
    {
        std::vector<Prep> P;
        double curbW, F, FD;
        DomeForgeRgb road, curb, lane;
        V3 L;
        double L2x, L2y;
        int seed;
        double aa, e, segSp, laneW, dash, gap, bankW, shW, scale;
        const DomeForgeConfig* cfg;
    };

    // One row of renderRoads. Port: the pixel body is the JS's; what is new is
    // the span skip in front of it. The road union is 1-Lipschitz (exact SDFs
    // joined by the quadratic smin, whose gradient is a convex blend), so a
    // span whose centre is further than its half-width past the bank cannot
    // hold a pixel the JS would draw -- every one of them hits the same
    // `continue`. Most of the layer is empty ground; this skips it.
    void RenderRoadsRow(const RoadCtx& c, DomeForgeImage& img, int y)
    {
        const DomeForgeConfig& cfg = *c.cfg;
        const int W = img.width, H = img.height;
        const std::vector<Prep>& P = c.P;
        const double curbW = c.curbW, F = c.F, FD = c.FD, aa = c.aa, e = c.e, scale = c.scale;
        const DomeForgeRgb &road = c.road, &curb = c.curb, &lane = c.lane;
        const V3& L = c.L;
        const double L2x = c.L2x, L2y = c.L2y;
        const int seed = c.seed;
        const double segSp = c.segSp, laneW = c.laneW, dash = c.dash, gap = c.gap, bankW = c.bankW, shW = c.shW;
        Field f, fx, fy;
        const int SPAN = 16;
        for (int x0 = 0; x0 < W; x0 += SPAN)
        {
            const int x1 = std::min(W, x0 + SPAN);
            const double midX = (x0 + x1) * 0.5, halfW = (x1 - x0) * 0.5;
            RoadField(midX, H - (y + 0.5), P, F, FD, f);
            if (f.d - halfW > bankW + aa + 0.01) continue;
            for (int x = x0; x < x1; x++)
            {
                const double X = x + 0.5, Y = H - (y + 0.5);   // math coords: y up, so the light matches the sprites
                RoadField(X, Y, P, F, FD, f);
                const double d = f.d;
                if (d > bankW + aa) continue;
                const size_t o = ((size_t)y * W + x) * 4;
                // outward normal of the outline (needed for the ridge and the embankment)
                RoadField(X + e, Y, P, F, FD, fx);
                RoadField(X, Y + e, P, F, FD, fy);
                double gx = fx.d - d, gy = fy.d - d;
                double gl = std::hypot(gx, gy);
                if (gl == 0) gl = 1;
                gx /= gl;
                gy /= gl;
                const double facing = gx * L2x + gy * L2y;   // +1: this edge faces the light
                if (d > aa * 0.5)
                {
                    // the road is a raised slab: a lit slope on the side toward the light, a cast shadow on the far side
                    const double band = 1 - Sstep(0, bankW, d);
                    const double lit = std::max(0.0, facing) * cfg.bankLight, dark = std::max(0.0, -facing) * cfg.bankShadow;
                    const double al = band * (lit + dark);
                    const uint8_t v = lit > dark ? 255 : 0;
                    img.rgba[o] = v;
                    img.rgba[o + 1] = v;
                    img.rgba[o + 2] = v;
                    img.rgba[o + 3] = ToByte(Clamp(al, 0.0, 1.0) * 255);
                    continue;
                }
                const double cov = 1 - Sstep(-aa * 0.5, aa * 0.5, d);
                double R, G, B;
                // mottled asphalt
                const double mot = (PatchNoise(X, Y, 14 * scale, seed, 0.5) - 0.5) * cfg.roadMottle * 2 +
                                   (HashInt(x, y, seed + 2) - 0.5) * cfg.roadGrain * 2;
                const double k = 1 + mot;
                R = road.r * k;
                G = road.g * k;
                B = road.b * k;
                // lane line, dashed, along the nearest road primitive
                if (cfg.laneOn && f.isDisc == 0 && d < -curbW - 2 * scale)
                {
                    const double on = Frac01(f.t / (dash + gap)) < dash / (dash + gap) ? 1 : 0;
                    const double la = (1 - Sstep(laneW * 0.5 - 0.6, laneW * 0.5 + 0.6, f.perp)) * on * cfg.laneAlpha;
                    if (la > 0)
                    {
                        R = Lerp(R, lane.r, la);
                        G = Lerp(G, lane.g, la);
                        B = Lerp(B, lane.b, la);
                    }
                }
                // the dome rims sit above the road and throw a shadow onto it
                if (shW > 0 && f.dDisc > 0 && f.dDisc < shW)
                {
                    const double sm = 1 - cfg.domeShadow * (1 - Sstep(0, shW, f.dDisc));
                    R *= sm;
                    G *= sm;
                    B *= sm;
                }
                // kerb: a rounded ridge along the outline. Its lit face is the outer side on edges that face the
                // light and the inner side on edges that face away, so both kerbs of a street read the same.
                if (d > -curbW - 3 * scale)
                {
                    const double u = Clamp(-d / curbW, 0.0, 1.0);   // 0 at the outer edge, 1 at the asphalt
                    const double slope = cfg.curbBevel * PI * std::cos(PI * u) / curbW;
                    double nx = slope * gx, ny = slope * gy;
                    double segMul = 1;
                    if (segSp > 0 && cfg.curbSegDepth > 0)
                    {
                        const double fr = Frac01(f.t / segSp), dEdge = std::min(fr, 1 - fr) * segSp;
                        const double line = 1 - Sstep(0.3 * scale, 1.0 * scale, dEdge);
                        segMul = 1 - cfg.curbSegDepth * 0.55 * line;
                        const double tilt = std::sin(2 * PI * fr) * cfg.curbSegDepth * 0.25;
                        nx += -gy * tilt;
                        ny += gx * tilt;
                    }
                    const double nl = Hypot3(nx, ny, 1);
                    nx /= nl;
                    ny /= nl;
                    const double nz = 1 / nl;
                    const double ndl = std::max(0.0, nx * L.x + ny * L.y + nz * L.z);
                    const double hx = L.x, hy = L.y, hz = L.z + 1, hl = Hypot3(hx, hy, hz);
                    const double ndh = std::max(0.0, (nx * hx + ny * hy + nz * hz) / hl);
                    const double spec = std::pow(ndh, 14) * cfg.curbShine;
                    const double grain = (PatchNoise(X + 11, Y - 7, 6 * scale, seed + 5, 0.5) - 0.5) * 0.18 +
                                         (HashInt(x, y, seed + 6) - 0.5) * 0.06;
                    const double kc = (0.42 + 0.7 * ndl + grain) * segMul;
                    double cr = curb.r * kc + spec, cg = curb.g * kc + spec, cb = curb.b * kc + spec;
                    // inked contour along the outer edge
                    const double ol = 1 - cfg.curbOutline * (1 - Sstep(0.2 * scale, 0.9 * scale, -d));
                    cr *= ol;
                    cg *= ol;
                    cb *= ol;
                    const double ca = Sstep(-curbW - 0.5, -curbW + 0.5, d);   // kerb coverage (1 on the kerb, 0 on the asphalt)
                    // shadow the kerb throws on the asphalt, only where its inner side faces away from the light
                    const double sh = 1 - cfg.curbShadow * Sstep(-curbW - 3 * scale, -curbW, d) * (1 - ca) * Clamp(0.5 + 0.5 * facing, 0.0, 1.0);
                    R = Lerp(R * sh, cr, ca);
                    G = Lerp(G * sh, cg, ca);
                    B = Lerp(B * sh, cb, ca);
                }
                img.rgba[o] = ToByte(Clamp(R, 0.0, 1.0) * 255);
                img.rgba[o + 1] = ToByte(Clamp(G, 0.0, 1.0) * 255);
                img.rgba[o + 2] = ToByte(Clamp(B, 0.0, 1.0) * 255);
                img.rgba[o + 3] = ToByte(cov * 255);
            }
        }
    }
}

DomeForgeJob DomeForgeJob::Roads(const DomeForgeConfig& cfgIn, int W, int H,
                                 const std::vector<DomeForgePrim>& prims, double scale)
{
    if (scale == 0) scale = 1;
    auto cfg = std::make_shared<DomeForgeConfig>(cfgIn);
    auto c = std::make_shared<RoadCtx>();
    c->cfg = cfg.get();
    c->P = PrepPrims(prims);
    c->scale = scale;
    c->curbW = cfg->curbW * scale;
    c->F = cfg->fillet * scale;
    c->FD = cfg->filletDome * scale;
    c->road = cfg->roadColor;
    c->curb = cfg->curbColor;
    c->lane = cfg->laneColor;
    c->L = DirFromAzEl(cfg->lightAz, cfg->lightEl);
    double Lm = std::hypot(c->L.x, c->L.y);
    if (Lm == 0) Lm = 1;
    c->L2x = c->L.x / Lm;
    c->L2y = c->L.y / Lm;
    c->seed = cfg->seed + 900;
    c->aa = 1;
    c->e = 0.6;
    c->segSp = cfg->curbSeg * scale;
    c->laneW = cfg->laneW * scale;
    c->dash = cfg->laneDash * scale;
    c->gap = cfg->laneGap * scale;
    c->bankW = cfg->bankW * scale;
    c->shW = cfg->domeShadowW * scale;
    DomeForgeImage img;
    img.width = W;
    img.height = H;
    img.rgba.assign((size_t)W * H * 4, 0);
    return FromRows(std::move(img), [cfg, c](DomeForgeImage& out, int y) { RenderRoadsRow(*c, out, y); });
}

DomeForgeImage DomeForgeRenderRoads(const DomeForgeConfig& cfg, int W, int H,
                                    const std::vector<DomeForgePrim>& prims, double scale)
{
    DomeForgeJob job = DomeForgeJob::Roads(cfg, W, H, prims, scale);
    job.Step(1e30);
    return job.TakeImage();
}

// ---------- lunar ground ----------
DomeForgeImage DomeForgeRenderGround(const DomeForgeConfig& cfg, int W, int H, double scale)
{
    if (scale == 0) scale = 1;
    DomeForgeImage img;
    img.width = W;
    img.height = H;
    img.rgba.assign((size_t)W * H * 4, 0);
    const DomeForgeRgb base = cfg.groundColor;
    const V3 L = DirFromAzEl(cfg.lightAz, cfg.lightEl);
    const int seed = cfg.seed + 500;
    struct Layer
    {
        double cell, prob, rMin, rMax;
        int seed;
    };
    const Layer layers[3] = {
        {130 * scale, cfg.craterBig, 16 * scale, 58 * scale, seed + 10},
        {46 * scale, cfg.craterSmall, 3.5 * scale, 11 * scale, seed + 20},
        {16 * scale, cfg.craterSmall * 0.6, 1.2 * scale, 3.5 * scale, seed + 30},
    };
    const double depth = cfg.craterDepth, rimH = cfg.craterRim;
    for (int y = 0; y < H; y++)
    {
        for (int x = 0; x < W; x++)
        {
            const double X = x + 0.5, Y = H - (y + 0.5);
            // soft large-scale mottling + fine grain
            const double mot = (PatchNoise(X, Y, 90 * scale, seed, 0.6) - 0.5) * cfg.groundMottle * 2 +
                               (PatchNoise(X + 300, Y, 22 * scale, seed + 1, 0.6) - 0.5) * cfg.groundMottle +
                               (PatchNoise(X - 150, Y + 90, 4 * scale, seed + 3, 0.4) - 0.5) * cfg.groundGrain * 1.5 +
                               (HashInt(x, y, seed + 2) - 0.5) * cfg.groundGrain * 2;
            double nx = 0, ny = 0, ao = 0;
            // craters: bowl with a raised rim; the nearest crater wins in each layer
            for (const Layer& ly : layers)
            {
                const double gx = std::floor(X / ly.cell), gy = std::floor(Y / ly.cell);
                double bu = 1e9, bdx = 0, bdy = 0;
                for (int j = -1; j <= 1; j++)
                {
                    for (int i = -1; i <= 1; i++)
                    {
                        const int cx = (int)gx + i, cy = (int)gy + j;
                        if (HashInt(cx, cy, ly.seed) > ly.prob) continue;
                        const double jx = (cx + HashInt(cx, cy, ly.seed + 1)) * ly.cell, jy = (cy + HashInt(cx, cy, ly.seed + 2)) * ly.cell;
                        const double Rr = Lerp(ly.rMin, ly.rMax, std::pow(HashInt(cx, cy, ly.seed + 3), 2.2));
                        const double dx = X - jx, dy = Y - jy, u = std::hypot(dx, dy) / Rr;
                        if (u < bu)
                        {
                            bu = u;
                            bdx = dx;
                            bdy = dy;
                        }
                    }
                }
                if (bu < 1.35)
                {
                    // height profile h(u): bowl -depth*(1-u^2) inside, raised rim around u~1
                    double rr = std::hypot(bdx, bdy);
                    if (rr == 0) rr = 1e-6;
                    const double dirx = bdx / rr, diry = bdy / rr;
                    const double rimW = 0.18, g = std::exp(-((bu - 1.02) * (bu - 1.02)) / (2 * rimW * rimW));
                    // slope dh/dr (self-similar: independent of crater size): parabolic bowl + gaussian rim
                    const double s = (bu < 1 ? 0.5 * depth * bu : 0) + 0.08 * rimH * (-(bu - 1.02) / (rimW * rimW)) * g;
                    nx += -s * dirx;
                    ny += -s * diry;
                    if (bu < 1) ao += 0.3 * (1 - bu * bu) * depth;
                }
            }
            const double nl = Hypot3(nx, ny, 1);
            const double ndl = std::max(0.0, (nx * L.x + ny * L.y + L.z) / nl);
            const double k = (0.45 + 0.75 * ndl) * (1 - ao) * (1 + mot);
            const size_t o = ((size_t)y * W + x) * 4;
            img.rgba[o] = ToByte(Clamp(base.r * k, 0.0, 1.0) * 255);
            img.rgba[o + 1] = ToByte(Clamp(base.g * k, 0.0, 1.0) * 255);
            img.rgba[o + 2] = ToByte(Clamp(base.b * k, 0.0, 1.0) * 255);
            img.rgba[o + 3] = 255;
        }
    }
    return img;
}

// ---------- assembly ----------
// Layout in math coordinates (origin at the centre, y up), all in px at the given scale.
DomeForgeLayout DomeForgeMakeLayout(const DomeForgeConfig& cfg, double scale)
{
    DomeForgeLayout lay;
    const double A = cfg.baseSize * scale, s = A / 1254;
    lay.A = A;
    lay.s = s;
    const double cX = A / 2, cY = A / 2 - cfg.offsetY * s;
    const double centralSize = cfg.centralSize * s, unitSize = cfg.unitSize * s;
    const DomeForgeShape &K = cfg.central, &U = cfg.unit;
    const double centralRout = (K.domeRadius + K.ringWidth) * centralSize;
    const double unitRout = (U.domeRadius + U.ringWidth) * unitSize;
    lay.domes.push_back({DomeForgeKind::CENTRAL, cX, cY, centralSize, centralRout, 0, false});
    const double orbit = cfg.orbit * s, ringR = cfg.ringRoadR * s;
    for (int i = 0; i < 8; i++)
    {
        const double ang = 90 + 45 * i, ca = std::cos(ang * DEG), sa = std::sin(ang * DEG);
        const bool cardinal = i % 2 == 0;
        const double dx = cX + orbit * ca, dy = cY + orbit * sa;
        lay.domes.push_back({DomeForgeKind::UNIT, dx, dy, unitSize, unitRout, ang, cardinal});
        if (cfg.domeRoads)
        {
            DomeForgePrim a;   // centre -> dome
            a.t = DomeForgePrim::SEG;
            a.ax = cX;
            a.ay = cY;
            a.bx = dx;
            a.by = dy;
            a.w = cfg.roadW * s;
            lay.prims.push_back(a);
            DomeForgePrim b;   // dome -> ring
            b.t = DomeForgePrim::SEG;
            b.ax = dx;
            b.ay = dy;
            b.bx = cX + ringR * ca;
            b.by = cY + ringR * sa;
            b.w = cfg.roadW * s;
            lay.prims.push_back(b);
        }
        if (cardinal && cfg.spokesBeyond)
        {
            DomeForgePrim o;
            o.t = DomeForgePrim::SEG;
            o.ax = cX + ringR * ca;
            o.ay = cY + ringR * sa;
            o.bx = cX + A * ca;
            o.by = cY + A * sa;
            o.w = cfg.roadOuterW * s;
            lay.prims.push_back(o);
        }
    }
    DomeForgePrim ring;
    ring.t = DomeForgePrim::RING;
    ring.cx = cX;
    ring.cy = cY;
    ring.r = ringR;
    ring.w = cfg.roadW * s;
    lay.prims.push_back(ring);
    for (const DomeForgeDome& d : lay.domes)
    {
        DomeForgePrim disc;
        disc.t = DomeForgePrim::DISC;
        disc.cx = d.x;
        disc.cy = d.y;
        disc.r = d.rout - 0.5 * s;
        lay.prims.push_back(disc);
    }
    return lay;
}

DomeForgeConfig DomeForgeSpriteConfig(const DomeForgeConfig& cfg, const DomeForgeLayout& lay,
                                      const DomeForgeDome& d, const DomeForgeRgb& color)
{
    double unitSize = cfg.unitSize * lay.s;
    for (const DomeForgeDome& u : lay.domes)
    {
        if (u.kind == DomeForgeKind::UNIT)
        {
            unitSize = u.size;
            break;
        }
    }
    const int size = std::max(32, (int)JsRound(d.size));
    DomeForgeConfig sc = cfg;
    sc.bgOn = false;
    sc.color = color;
    if (d.kind == DomeForgeKind::UNIT)
    {
        sc.socketOn = cfg.socketOn && cfg.unitSockets;
        // two loops: one where the connector road from the centre plugs in, one toward the ring road
        sc.unit.size = size;
        sc.unit.socketCount = 2;
        sc.unit.socketStart = cfg.socketsToCentre ? std::fmod(d.angle + 180, 360.0) : cfg.unit.socketStart;
    }
    else
    {
        sc.socketOn = cfg.socketOn && cfg.centralSockets;
        // one loop per connector road, aimed at the unit domes. Socket size is derived from the unit
        // sprite size, so pass the unit size used in this base (scaled) or the loops come out too big.
        sc.unit.size = std::max(32, (int)JsRound(unitSize));
        sc.central.size = size;
        sc.central.socketCount = 8;
        sc.central.socketStart = 90;
    }
    return sc;
}

DomeForgeImage DomeForgeRenderBase(const DomeForgeConfig& cfg, double scale)
{
    if (scale == 0) scale = 1;
    const DomeForgeLayout lay = DomeForgeMakeLayout(cfg, scale);
    const int W = (int)JsRound(lay.A), H = W;
    DomeForgeImage ground = DomeForgeRenderGround(cfg, W, H, scale * cfg.baseSize / 1254);
    const DomeForgeImage roads = DomeForgeRenderRoads(cfg, W, H, lay.prims, scale * cfg.baseSize / 1254);
    DomeForgeBlit(ground, roads, 0, 0);
    for (const DomeForgeDome& d : lay.domes)
    {
        const DomeForgeRgb color = d.kind == DomeForgeKind::CENTRAL ? cfg.centralColor
                                   : d.cardinal                    ? cfg.cardinalColor
                                                                   : cfg.diagonalColor;
        const DomeForgeImage img = DomeForgeRender(DomeForgeSpriteConfig(cfg, lay, d, color), d.kind);
        DomeForgeBlit(ground, img, (int)JsRound(d.x - img.cx), (int)JsRound(H - d.y - img.cy));
    }
    return ground;
}
