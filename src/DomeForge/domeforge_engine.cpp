// domeforge_engine.cpp — dome sprites. A 1:1 port of dome-forge-engine.js:
// same functions, same order, same arithmetic (in double). Comments that
// describe the look are the JS's own; comments about the port say "port:".
#include "domeforge.h"
#include "domeforge_util.h"

#include <algorithm>
#include <chrono>
#include <cmath>

using namespace DomeForgeUtil;

DomeForgeConfig DomeForgeDefaults()
{
    DomeForgeConfig c;
    c.unit = DomeForgeShape{256, 0.33, 0.07, 0, 0.085, false, 1, 270.0};
    c.central = DomeForgeShape{560, 0.295, 0.068, 0, 0.085, false, 8, 90.0};
    c.color = DomeForgeHex("#1fb75b");
    c.frameColor = DomeForgeHex("#a4a4a4");
    c.lightColor = DomeForgeHex("#ffad55");
    c.bg = DomeForgeHex("#2b2b2b");
    c.roadColor = DomeForgeHex("#645e57");
    c.curbColor = DomeForgeHex("#a89d90");
    c.laneColor = DomeForgeHex("#8f877a");
    c.groundColor = DomeForgeHex("#2c2a26");
    c.centralColor = DomeForgeHex("#8d8d8d");
    c.cardinalColor = DomeForgeHex("#1fb75b");
    c.diagonalColor = DomeForgeHex("#8a8a8a");
    return c;
}

namespace
{
    // ---------- frame profiles ----------
    enum SegKind { UP, DOWN, RIDGE, GROOVE, GAP, SEAM, FLAT };
    struct Seg
    {
        double w;
        SegKind k;
        double a;
        double o;
        bool seg;
    };
    struct Profile
    {
        double innerFrac;
        std::vector<Seg> outer, inner;
    };

    const Profile& ProfileFor(const std::string& name)
    {
        static const Profile plate = {0.34,
            {{0.18, UP, 1, 0, false}, {0.50, FLAT, 0, 0, false}, {0.18, DOWN, 1, 0, false}, {0.14, GAP, 0, 0, false}},
            {{0.22, GAP, 0, 0, false}, {0.34, UP, 1, 0, false}, {0.32, FLAT, 0, 0, false}, {0.12, DOWN, 0.6, 0, false}}};
        static const Profile classic = {0.30,
            {{0.58, RIDGE, 1.0, 0, true}, {0.10, GROOVE, 0.5, 0, false}, {0.32, FLAT, 0, 0.2, false}},
            {{0.24, GAP, 0, 0, false}, {0.64, RIDGE, 0.9, 0, false}, {0.12, FLAT, 0, 0.2, false}}};
        static const Profile simple = {0.22,
            {{0.16, UP, 1, 0, false}, {0.84, FLAT, 0, 0, false}},
            {{0.25, GAP, 0, 0, false}, {0.75, FLAT, 0, 0, false}}};
        static const Profile heavy = {0.34,
            {{0.30, RIDGE, 1.1, 0, true}, {0.10, GROOVE, 0.6, 0, false}, {0.24, RIDGE, 0.8, 0, false}, {0.36, FLAT, 0, 0.16, false}},
            {{0.2, GAP, 0, 0, false}, {0.6, RIDGE, 1.0, 0, false}, {0.2, FLAT, 0, 0.16, false}}};
        static const Profile chamfer = {0.40,
            {{0.10, UP, 1, 0, false}, {0.14, FLAT, 0, 0, false}, {0.15, GROOVE, 0.9, 0, false}, {0.61, FLAT, 0, 0, false}},
            {{0.14, GAP, 0, 0, false}, {0.46, RIDGE, 1, 0, false}, {0.40, FLAT, 0, 0, false}}};
        if (name == "plate") return plate;
        if (name == "simple") return simple;
        if (name == "heavy") return heavy;
        if (name == "chamfer") return chamfer;
        return classic;   // PROFILES[x] || PROFILES.classic
    }

    const std::vector<Seg> LIP = {{0.30, UP, 0.8, 0, false}, {0.42, FLAT, 0, 0.05, false}, {0.28, SEAM, 0, 0, false}};
    const std::vector<Seg> HOLLOW = {{0.34, GAP, 0, 0, false}, {0.66, UP, 0.8, 0, false}};

    struct Prof4
    {
        double slope, occ, gap, seg;
    };

    Prof4 EvalProfile(const std::vector<Seg>& segs, double t, double wpxAll)
    {
        double acc = 0.0;
        for (size_t i = 0; i < segs.size(); i++)
        {
            const Seg& s = segs[i];
            if (t < acc + s.w || i == segs.size() - 1)
            {
                const double u = Clamp((t - acc) / s.w, 0.0, 1.0), wpx = std::max(0.5, s.w * wpxAll);
                const double o = s.o, sg = s.seg ? 1.0 : 0.0;
                switch (s.k)
                {
                    case UP: return {s.a / wpx, o, 0, sg};
                    case DOWN: return {-s.a / wpx, o, 0, sg};
                    case RIDGE:
                    {
                        const double sn = std::sin(PI * u);
                        return {s.a * PI * std::cos(PI * u) / wpx, o + 0.28 * (1 - sn) * (1 - sn), 0, sg};
                    }
                    case GROOVE: return {-s.a * PI * std::cos(PI * u) / wpx, o + 0.55 * std::sin(PI * u), 0, sg};
                    case GAP: return {0, 0.75, 1, 0};
                    case SEAM: return {0, 0.6, 0, 0};
                    default: return {0, o, 0, sg};
                }
            }
            acc += s.w;
        }
        return {0, 0, 0, 0};
    }

    // ---------- geometry ----------
    double ChamferRectSDF(double x, double y, double hw, double hh, double c)   // rectangle with cut corners (octagon)
    {
        const double ax = std::fabs(x), ay = std::fabs(y);
        return std::max({ax - hw, ay - hh, (ax + ay - (hw + hh - c)) * 0.7071068});
    }

    struct Socket
    {
        double ang, rc, c, s;
    };
    struct RimLight
    {
        double c, s, r0;
    };
    struct Geo
    {
        double S, sc, us, Rd, ringW, Rout;
        // socket loop
        double skW, skH, skT, skRc, skHw, skHh, skHrc;
        bool skChamfer;
        std::vector<Socket> sockets;
        std::vector<RimLight> lights;
        double fillet;
        int sides;
        double corner, rot;
        // light sizes
        double ltLen, ltW, ltSig, ltReach, ltPt, ltPsig;
        double cx, cy;
    };

    double SocketShape(const Geo& g, double x, double y, double hw, double hh, double rc)
    {
        return g.skChamfer ? ChamferRectSDF(x, y, hw, hh, rc) : RoundedRectSDF(x, y, hw, hh, rc);
    }

    // the rim outline on its own: a circle, or a regular polygon with heavily rounded corners
    double RingSDF(double X, double Y, const Geo& g)
    {
        const double r = std::hypot(X, Y);
        if (g.sides < 3) return r - g.Rout;
        const double sector = 2 * PI / g.sides, ap = g.Rout - g.corner;
        const double ang = std::atan2(Y, X);
        const double base = g.rot + JsRound((ang - g.rot) / sector) * sector;   // nearest edge normal
        const double a = ang - base, px = r * std::cos(a), py = r * std::sin(a);
        double d;
        if (px <= ap) d = px - ap;
        else
        {
            const double t = ap * std::tan(sector / 2), ey = Clamp(py, -t, t);
            d = std::hypot(px - ap, py - ey);
        }
        return d - g.corner;
    }

    double BoundaryDistance(const Geo& g, double angDeg)
    {
        const double c = std::cos(angDeg * DEG), s = std::sin(angDeg * DEG);
        double lo = g.Rd, hi = g.Rout * 2;
        for (int i = 0; i < 40; i++)
        {
            const double mid = (lo + hi) / 2;
            if (RingSDF(mid * c, mid * s, g) < 0) lo = mid;
            else hi = mid;
        }
        return (lo + hi) / 2;
    }

    // The metal is one piece: the rim merged with every socket loop. This is the signed
    // distance to that merged outline (negative inside). o receives [union, ring, loops].
    double MetalSDF(double X, double Y, const Geo& g, double* o)
    {
        const double dr = RingSDF(X, Y, g);
        double dl = 1e9;
        for (const Socket& so : g.sockets)
        {
            const double ly = X * so.c + Y * so.s - so.rc, lx = -X * so.s + Y * so.c;
            const double d = SocketShape(g, lx, ly, g.skW / 2, g.skH / 2, g.skRc);
            if (d < dl) dl = d;
        }
        const double du = dl < 1e8 ? Smin(dr, dl, g.fillet) : dr;
        if (o)
        {
            o[0] = du;
            o[1] = dr;
            o[2] = dl;
        }
        return du;
    }

    // signed distance to the nearest socket hollow (negative inside the hole)
    double HollowSDF(double X, double Y, const Geo& g)
    {
        double dh = 1e9;
        for (const Socket& so : g.sockets)
        {
            const double ly = X * so.c + Y * so.s - so.rc, lx = -X * so.s + Y * so.c;
            const double d = SocketShape(g, lx, ly, g.skHw, g.skHh, g.skHrc);
            if (d < dh) dh = d;
        }
        return dh;
    }

    double MetalOnly(double X, double Y, const Geo& g) { return MetalSDF(X, Y, g, nullptr); }

    template <typename Fn>
    void Gradient(Fn fn, double X, double Y, const Geo& g, double* out)
    {
        const double e = 0.35;
        const double gx = fn(X + e, Y, g) - fn(X - e, Y, g), gy = fn(X, Y + e, g) - fn(X, Y - e, g);
        double l = std::hypot(gx, gy);
        if (l == 0) l = 1;
        out[0] = gx / l;
        out[1] = gy / l;
    }

    // pointy-top hex grid, circumradius 1. Returns cell id, centre, local offset, and distance to the nearest edge.
    struct Cell
    {
        double q, r, cu, cv, lu, lv, edge;
    };

    void HexCell(double u, double v, Cell& o)
    {
        const double q = (SQ3 / 3) * u - v / 3, r = (2.0 / 3.0) * v;
        double rx = JsRound(q), rz = JsRound(r), ry = JsRound(-q - r);
        const double dx = std::fabs(rx - q), dy = std::fabs(ry + q + r), dz = std::fabs(rz - r);
        if (dx > dy && dx > dz) rx = -ry - rz;
        else if (dy <= dz) rz = -rx - ry;
        const double cu = SQ3 * (rx + rz / 2), cv = 1.5 * rz;
        const double lu = u - cu, lv = v - cv;
        const double h = std::max({std::fabs(lu), std::fabs(lu * 0.5 + lv * 0.8660254), std::fabs(lu * 0.5 - lv * 0.8660254)});
        o.q = rx;
        o.r = rz;
        o.cu = cu;
        o.cv = cv;
        o.lu = lu;
        o.lv = lv;
        o.edge = 0.8660254 - h;
    }

    // ---------- scene setup ----------
    struct Scene
    {
        Geo g;
        const DomeForgeConfig* cfg;
        V3 L, H, Hm, Rdir;
        double L2x, L2y;
        DomeForgeRgb base, frameBase, lightCol, lightCore, specCol, rimCol;
        const Profile* prof;
        double Wi, Wo, Wh, bevel, hexLinePx, s, glintW, glintH, lensD, thMax, curve;
        int seed;
        double hexCos, hexSin;
    };

    DomeForgeRgb LerpRgb(const DomeForgeRgb& c, double to, double t)
    {
        return {Lerp(c.r, to, t), Lerp(c.g, to, t), Lerp(c.b, to, t)};
    }

    void BuildScene(const DomeForgeConfig& cfg, DomeForgeKind kind, Scene& sc)
    {
        const DomeForgeShape& K = (kind == DomeForgeKind::CENTRAL) ? cfg.central : cfg.unit;
        Geo& g = sc.g;
        g.S = K.size;
        g.sc = g.S / 256.0;
        g.us = cfg.unit.size / 256.0;   // sc: this sprite's scale, us: socket scale
        const double S = g.S, us = g.us;
        g.Rd = K.domeRadius * S;
        g.ringW = K.ringWidth * S;
        g.Rout = g.Rd + g.ringW;
        g.skW = cfg.socketW * us;
        g.skH = cfg.socketH * us;
        g.skT = cfg.socketT * us;
        g.skRc = cfg.socketCorner * us;
        g.skChamfer = cfg.socketChamfer;
        g.skHw = g.skW / 2 - g.skT;
        g.skHh = g.skH / 2 - g.skT;
        g.skHrc = g.skChamfer ? std::max(0.0, g.skRc - g.skT * 0.6) : std::max(0.5, g.skRc - g.skT);
        g.sides = K.sides >= 3 ? K.sides : 0;
        g.fillet = cfg.socketFillet * us;
        g.corner = 0;
        g.rot = 0;
        if (g.sides)
        {
            // Rout is the apothem (distance to the flat edges); corners are rounded with radius `corner`
            g.corner = std::min(K.corner * S, g.Rout - g.Rd - 1);
            g.rot = (K.socketStart + (K.socketsAtCorners ? 180.0 / g.sides : 0.0)) * DEG;
        }
        const int n = cfg.socketOn ? K.socketCount : 0;
        g.sockets.clear();
        for (int i = 0; i < n; i++)
        {
            const double ang = K.socketStart + (360.0 / n) * i;
            // loop centre: hangs off the rim, pulled inward by socketInset so the two merge into one piece
            const double rc = BoundaryDistance(g, ang) + g.skH / 2 - cfg.socketInset * us;
            g.sockets.push_back({ang, rc, std::cos(ang * DEG), std::sin(ang * DEG)});
        }
        // socket lights: two bars on the rim, lightAngle degrees either side of each socket
        g.lights.clear();
        if (n && cfg.rimLights && cfg.lightGlow > 0)
        {
            // when sockets are packed so tightly that neighbouring pairs would collide, put one bar in each gap instead
            const double spacing = 360.0 / n;
            const bool single = spacing / 2 <= cfg.lightAngle + 4;
            std::vector<double> angles;
            for (const Socket& so : g.sockets)
            {
                if (single) angles.push_back(so.ang + spacing / 2);
                else
                {
                    angles.push_back(so.ang - cfg.lightAngle);
                    angles.push_back(so.ang + cfg.lightAngle);
                }
            }
            for (double a : angles)
                g.lights.push_back({std::cos(a * DEG), std::sin(a * DEG), BoundaryDistance(g, a) - g.ringW * cfg.lightRadial});
        }
        g.ltLen = cfg.lightLen * us;
        g.ltW = cfg.lightW * us;
        g.ltSig = cfg.lightGlowR * us * 0.45;
        g.ltReach = cfg.lightGlowR * us * 2.2;
        g.ltPt = cfg.lightSize * us;
        g.ltPsig = cfg.lightGlowR * us * 0.5;
        // bounds -> centre so everything fits
        const double ext = g.sides ? g.Rout / std::cos(PI / g.sides) : g.Rout;
        double minX = -ext, maxX = ext, minY = -ext, maxY = ext;
        for (const Socket& so : g.sockets)
        {
            const double reach = so.rc + g.skH / 2, half = g.skW / 2;
            const double px = so.c * reach, py = so.s * reach;
            minX = std::min(minX, px - half);
            maxX = std::max(maxX, px + half);
            minY = std::min(minY, py - half);
            maxY = std::max(maxY, py + half);
        }
        g.cx = S / 2 - (minX + maxX) / 2;
        g.cy = S / 2 + (minY + maxY) / 2;   // screen y is down; math y is up

        // lighting
        sc.cfg = &cfg;
        sc.L = DirFromAzEl(cfg.lightAz, cfg.lightEl);
        const double hz = std::sqrt(std::max(0.02, 1 - cfg.hlX * cfg.hlX - cfg.hlY * cfg.hlY));
        sc.H = Norm3(cfg.hlX, cfg.hlY, hz);
        sc.Rdir = DirFromAzEl(cfg.rimAz, 10);
        sc.Hm = Norm3(sc.L.x, sc.L.y, sc.L.z + 1);
        double L2 = std::hypot(sc.L.x, sc.L.y);
        if (L2 == 0) L2 = 1;
        sc.L2x = sc.L.x / L2;
        sc.L2y = sc.L.y / L2;
        sc.base = cfg.color;
        sc.frameBase = cfg.frameColor;
        sc.lightCol = cfg.lightColor;
        sc.lightCore = LerpRgb(sc.lightCol, 1, 0.6);
        sc.specCol = LerpRgb(sc.base, 1, cfg.specWhite);
        sc.rimCol = LerpRgb(sc.base, 1, 0.45);
        sc.prof = &ProfileFor(cfg.frameProfile);
        sc.Wi = g.ringW * sc.prof->innerFrac;
        sc.Wo = g.ringW * (1 - sc.prof->innerFrac);
        sc.Wh = std::max(1.2 * us, g.skT * 0.42);
        sc.bevel = cfg.bevel * g.sc;
        sc.hexLinePx = cfg.hexLine * std::sqrt(g.sc);
        sc.s = cfg.hexCells;
        sc.glintW = cfg.glintSize * g.Rd * cfg.glintAspect;
        sc.glintH = cfg.glintSize * g.Rd / cfg.glintAspect;
        const double lens = 1.15 + 12 * (1 - cfg.hexLens) * (1 - cfg.hexLens);
        sc.lensD = cfg.hexLens > 0.001 ? lens : 0;
        sc.thMax = cfg.hexLens > 0.001 ? std::acos(1 / lens) : PI / 2;
        sc.curve = std::max(0.3, cfg.hexCurve != 0 ? cfg.hexCurve : 1.0);
        sc.seed = cfg.seed;
        sc.hexCos = std::cos(cfg.hexRot * DEG);
        sc.hexSin = std::sin(cfg.hexRot * DEG);
    }

    // ---------- shading ----------
    // Metal = key light + a fake studio environment (bright sky above, a floor bounce below,
    // a little fresnel on grazing normals) + a tight specular streak.
    void ShadeMetal(double nx, double ny, double nz, const Scene& sc, const DomeForgeRgb& base, double grain,
                    double* out, double sx, double sy, double sz)
    {
        const V3 &L = sc.L, &H = sc.Hm;
        const DomeForgeConfig& c = *sc.cfg;
        const double ndl = std::max(0.0, nx * L.x + ny * L.y + nz * L.z);
        const double ndh = std::max(0.0, sx * H.x + sy * H.y + sz * H.z);
        const double sky = Sstep(-0.6, 0.85, ny);
        const double floor = Sstep(-0.25, -0.95, ny) * 0.5;
        const double fres = std::pow(1 - nz, 3) * 0.3;
        const double E = Lerp(0.72, 0.45 + 0.55 * sky + floor + fres, c.metalEnv);
        // bevels that face the light get a bright painted edge
        const double tl = std::hypot(nx, ny);
        const double edge = tl > 1e-4 ? c.edgeLight * std::max(0.0, (nx * sc.L2x + ny * sc.L2y) / tl) * std::min(1.0, tl * 1.8) : 0;
        const double spec = std::pow(ndh, c.frameShine) * c.frameSpec + edge;
        const double k = c.frameAmbient * E + c.frameDiffuse * ndl + grain * nz * nz;
        out[0] = base.r * k + spec;
        out[1] = base.g * k + spec;
        out[2] = base.b * k + spec;
    }

    double MetalGrain(double lx, double ly, double r, double ang, const Scene& sc, int seed)
    {
        const DomeForgeConfig& c = *sc.cfg;
        const double g = c.grain;
        if (g <= 0 || c.grainStyle == "smooth") return 0;
        const int x = (int)lx, y = (int)ly;   // port: lx | 0 (always positive here)
        if (c.grainStyle == "mottled")
        {
            const double k = (c.grainScale != 0 ? c.grainScale : 1.0) * sc.g.sc;
            const double big = PatchNoise(lx, ly, 8 * k, seed, 0.5) - 0.5;
            const double mid = PatchNoise(lx + 37, ly - 11, 4 * k, seed + 40, 0.55) - 0.5;
            const double fine = HashInt(x, y, seed + 8) - 0.5;
            return (big * 1.5 + mid * 0.7 + fine * 0.06) * g * 2;
        }
        if (c.grainStyle == "brushed")
        {
            return ((HashInt((int)JsRound(r * 1.5), (int)JsRound(ang * 40), seed) - 0.5) * 0.7 +
                    (HashInt(x * 3, y * 3, seed + 6) - 0.5) * 0.3) * g * 2;
        }
        const double n = (HashInt(x, y, seed) - 0.5) * 0.45 + (HashInt(x >> 1, y >> 1, seed + 1) - 0.5) * 0.35 +
                         (HashInt(x >> 2, y >> 2, seed + 2) - 0.5) * 0.2;
        const double speck = HashInt(x, y, seed + 3) < 0.045 ? -0.6 : 0;
        return (n + speck) * g * 2;
    }

    // screen radius (0..1) -> "surface distance" from the dome centre, in radians of a unit sphere.
    double SurfAngle(double rr, const Scene& sc)
    {
        rr = std::min(rr, 1.0);
        double th;
        if (sc.lensD > 0)
        {
            const double d = sc.lensD, k = rr / std::sqrt(d * d - 1);
            th = std::asin(std::min(1.0, k * d / std::sqrt(1 + k * k))) - std::atan(k);
        }
        else th = std::asin(rr);
        return sc.curve == 1 ? th : sc.thMax * std::pow(std::max(0.0, th) / sc.thMax, sc.curve);
    }

    // inverse: surface distance -> screen radius
    double ScreenRadius(double th, const Scene& sc)
    {
        if (th >= sc.thMax) return 1;
        const double t = sc.curve == 1 ? th : sc.thMax * std::pow(th / sc.thMax, 1 / sc.curve);
        if (sc.lensD > 0)
        {
            const double d = sc.lensD;
            return std::min(1.0, std::sin(t) / (d - std::cos(t)) * std::sqrt(d * d - 1));
        }
        return std::sin(t);
    }

    void ShadeGlass(double X, double Y, const Scene& sc, double* out)
    {
        const Geo& g = sc.g;
        const DomeForgeConfig& cfg = *sc.cfg;
        const V3 &L = sc.L, &H = sc.H;
        const DomeForgeRgb& base = sc.base;
        const double Rd = g.Rd;
        const double nx = X / Rd, ny = Y / Rd;
        const double rr2 = nx * nx + ny * ny, rr = std::sqrt(rr2);
        const double nz = std::sqrt(std::max(0.0, 1 - rr2));
        const double ndl = std::max(0.0, nx * L.x + ny * L.y + nz * L.z);
        double k = cfg.ambient + cfg.diffuse * ndl;

        // hex cells mapped onto the curved surface: cells keep their size on the surface, so on
        // screen they squash more and more from the centre toward the edge
        const double theta = SurfAngle(rr, sc);
        const double f = rr > 1e-6 ? theta / rr : (sc.lensD > 0 ? SurfAngle(0.001, sc) / 0.001 : 1);
        double u = nx * f, v = ny * f;
        if (sc.hexSin != 0)
        {
            const double tu = u * sc.hexCos - v * sc.hexSin;
            v = u * sc.hexSin + v * sc.hexCos;
            u = tu;
        }
        const double s = sc.s;
        Cell cell;
        HexCell(u / s, v / s, cell);
        const double rnd = HashInt((int)cell.q, (int)cell.r, sc.seed), rnd2 = HashInt((int)cell.q, (int)cell.r, sc.seed + 77);
        double cellMul = 1 + (rnd2 - 0.5) * cfg.facetVar * 2;
        cellMul += cfg.facetBevel * (cell.lu * L.x + cell.lv * L.y) / 0.866;

        // the cell centre's true normal (through the inverse mapping) drives facet shading and lit facets
        double lit = 0, specC = -1;
        const double cr = std::hypot(cell.cu, cell.cv), ct = cr * s;
        if (ct < sc.thMax)
        {
            const double rrc = ScreenRadius(ct, sc);
            const double cdir = cr > 1e-9 ? rrc / cr : 0;
            double cnx = cell.cu * cdir, cny = cell.cv * cdir;
            if (sc.hexSin != 0)
            {
                const double tx = cnx * sc.hexCos + cny * sc.hexSin;
                cny = -cnx * sc.hexSin + cny * sc.hexCos;
                cnx = tx;
            }
            const double cnz = std::sqrt(std::max(0.0, 1 - cnx * cnx - cny * cny));
            const double cndh = std::max(0.0, cnx * H.x + cny * H.y + cnz * H.z);
            const double p = cfg.litBase + cfg.litNear * std::pow(cndh, cfg.shininess * 0.35 + 2);
            if (rnd < p) lit = cfg.litAmount * (0.55 + 0.45 * rnd2);
            if (cfg.facetShade > 0)
            {
                // shade as a flat tile tilted like the sphere at its centre
                const double cndl = std::max(0.0, cnx * L.x + cny * L.y + cnz * L.z);
                k = Lerp(k, cfg.ambient + cfg.diffuse * cndl, cfg.facetShade);
                specC = std::pow(cndh, cfg.shininess) * cfg.specInt;
            }
        }

        // cell outline: embossed (lighter on the side of the cell that faces the light, darker opposite),
        // thinning and fading toward the limb where the cells squash into slivers
        const double fp = std::min((SurfAngle(rr + 0.002, sc) - SurfAngle(std::max(0.0, rr - 0.002), sc)) / 0.004, 6.0) / Rd;
        const double halfW = std::max(sc.hexLinePx / Rd * 0.5, fp * 0.35);
        const double line = (1 - Sstep(halfW - fp * 0.6, halfW + fp * 0.6, cell.edge * s)) * (0.35 + 0.65 * nz);
        double cl = std::hypot(cell.lu, cell.lv);
        if (cl == 0) cl = 1;
        const double litSide = std::max(0.0, (cell.lu * L.x + cell.lv * L.y) / cl);

        double r = base.r * k * cellMul, gg = base.g * k * cellMul, b = base.b * k * cellMul;
        if (lit > 0)
        {
            r = Lerp(r, r * 1.35 + 0.12, lit);
            gg = Lerp(gg, gg * 1.35 + 0.12, lit);
            b = Lerp(b, b * 1.35 + 0.12, lit);
        }
        const double ld = 1 - cfg.hexLineDark * line * (1 - litSide) + cfg.hexLineLight * line * litSide;
        r *= ld;
        gg *= ld;
        b *= ld;

        // broad specular glow
        const double ndh = std::max(0.0, nx * H.x + ny * H.y + nz * H.z);
        double spec = std::pow(ndh, cfg.shininess) * cfg.specInt;
        if (specC >= 0) spec = Lerp(spec, specC, cfg.facetShade);
        spec = Clamp(spec, 0.0, 1.0);
        r = Lerp(r, sc.specCol.r, spec);
        gg = Lerp(gg, sc.specCol.g, spec);
        b = Lerp(b, sc.specCol.b, spec);

        // limb darkening: the surface turns away from the viewer toward the edge, so it goes dark
        // smoothly from the centre outward, which is what makes the dome read as convex
        const double limb = 1 - cfg.limbDark * std::pow(1 - nz, cfg.limbPow);
        r *= limb;
        gg *= limb;
        b *= limb;

        // contact shadow of the ring lip on the lit side
        const double facing = rr > 1e-6 ? 0.5 + 0.5 * (nx * sc.L2x + ny * sc.L2y) / rr : 0.5;
        const double band = Sstep(1 - cfg.edgeShadowW, 1, rr);
        const double shade = 1 - cfg.edgeShadow * band * facing;
        r *= shade;
        gg *= shade;
        b *= shade;

        // rim / reflected light on the far limb (added after the darkening so it stays bright)
        const V3& Rr = sc.Rdir;
        const double rim = std::pow(1 - nz, cfg.rimPow) * std::max(0.0, nx * Rr.x + ny * Rr.y) * cfg.rimInt;
        r += rim * sc.rimCol.r;
        gg += rim * sc.rimCol.g;
        b += rim * sc.rimCol.b;

        // thin bright glass lip, strongest where it faces the light
        const double edgePx = (1 - rr) * Rd;
        const double el = std::exp(-((edgePx - 0.9) * (edgePx - 0.9)) / 0.7) * cfg.edgeLine * (0.25 + 0.75 * facing);
        r += el * sc.rimCol.r;
        gg += el * sc.rimCol.g;
        b += el * sc.rimCol.b;

        // the diamond glint at the highlight
        const double gx = std::fabs(X - cfg.hlX * Rd) / sc.glintW, gy = std::fabs(Y - cfg.hlY * Rd) / sc.glintH;
        const double ga = (1 - Sstep(0.82, 1.06, gx + gy)) * cfg.glintStrength;
        if (ga > 0)
        {
            const DomeForgeRgb& w0 = sc.specCol;
            r = Lerp(r, Lerp(w0.r, 1, 0.6), ga);
            gg = Lerp(gg, Lerp(w0.g, 1, 0.6), ga);
            b = Lerp(b, Lerp(w0.b, 1, 0.6), ga);
        }

        out[0] = r;
        out[1] = gg;
        out[2] = b;
    }

    void ShadeFrame(double X, double Y, double lx, double ly, double r, double dH, const Scene& sc, double* out)
    {
        const Geo& g = sc.g;
        const DomeForgeConfig& cfg = *sc.cfg;
        const Profile& prof = *sc.prof;
        double m[3];
        MetalSDF(X, Y, g, m);
        const double dOut = -m[0], dIn = r - g.Rd, ang = std::atan2(Y, X);
        double slope = 0, occ = 0, gap = 0, seg = 0, dirx = 0, diry = 0;
        double n[2];
        if (dOut < sc.Wo && dOut <= dH)
        {
            // outer edge of the merged piece: the lip only where the ring outline dominates, fading out on the loops
            Gradient(MetalOnly, X, Y, g, n);
            dirx = n[0];
            diry = n[1];
            const double Wl = std::min(sc.Wo * 0.6, cfg.outerLip * g.ringW);
            const double lipMix = Wl > 0 ? Sstep(-1.5 * g.us, 2.5 * g.us, m[2] - m[1]) : 0;
            const Prof4 pr = EvalProfile(prof.outer, dOut / sc.Wo, sc.Wo);
            slope = pr.slope;
            occ = pr.occ;
            gap = pr.gap;
            seg = pr.seg;
            if (lipMix > 0)
            {
                const Prof4 pl = (dOut < Wl) ? EvalProfile(LIP, dOut / Wl, Wl)
                                             : EvalProfile(prof.outer, (dOut - Wl) / (sc.Wo - Wl), sc.Wo - Wl);
                slope = Lerp(slope, pl.slope, lipMix);
                occ = Lerp(occ, pl.occ, lipMix);
                gap = std::max(gap * (1 - lipMix), pl.gap * lipMix);
            }
        }
        else if (dH < sc.Wh)
        {
            // rim of a socket hollow
            Gradient(HollowSDF, X, Y, g, n);
            dirx = n[0];
            diry = n[1];
            const Prof4 p = EvalProfile(HOLLOW, dH / sc.Wh, sc.Wh);
            slope = -p.slope;
            occ = p.occ;
            gap = p.gap;
        }
        else if (dIn < sc.Wi)
        {
            const Prof4 p = EvalProfile(prof.inner, dIn / sc.Wi, sc.Wi);
            slope = -p.slope;
            occ = p.occ;
            gap = p.gap;
            const double rr = r != 0 ? r : 1;
            dirx = X / rr;
            diry = Y / rr;
        }
        const double sl = slope * sc.bevel;
        const double bl = std::hypot(sl, 1.0), bx = sl * dirx / bl, by = sl * diry / bl, bz = 1 / bl;
        double nx = bx, ny = by, nz = bz, segMul = 1;
        // knurling: the outer tube is cut into short links, each slightly pillowed, with a dark divider between
        if (seg != 0 && cfg.segments > 0 && cfg.segDepth > 0)
        {
            const double sp = cfg.segments * g.sc, arc = ang * g.Rout;
            const double f = arc / sp - std::floor(arc / sp);
            const double dEdge = std::min(f, 1 - f) * sp;
            const double line = 1 - Sstep(0.25 * g.sc, 0.95 * g.sc, dEdge);
            segMul = 1 - cfg.segDepth * 0.5 * line;
            const double tilt = std::sin(2 * PI * f) * cfg.segDepth * 0.28;
            const double tx = bx - diry * tilt, ty = by + dirx * tilt, tl = Hypot3(tx, ty, bz);
            nx = tx / tl;
            ny = ty / tl;
            nz = bz / tl;
        }
        const double grain = MetalGrain(lx, ly, r, ang, sc, sc.seed + 3);
        ShadeMetal(nx, ny, nz, sc, sc.frameBase, grain, out, bx, by, bz);
        const double dark = (1 - occ) * segMul;
        out[0] *= dark;
        out[1] *= dark;
        out[2] *= dark;
        if (gap != 0)
        {
            const double gm = 1 - 0.55 * gap;
            out[0] *= gm;
            out[1] *= gm;
            out[2] *= gm;
        }
        // inked contour along the outer outline and around the hollows
        const double ol = 1 - cfg.outline * (1 - Sstep(0.3 * g.sc, 1.0 * g.sc, std::min(dOut, dH)));
        out[0] *= ol;
        out[1] *= ol;
        out[2] *= ol;
    }
}

// ---------- main render ----------
namespace
{
    // One output row of render(). Split out of the pixel loop so a job can
    // stop between rows; the arithmetic inside is untouched.
    void RenderSpriteRow(const Scene& sc, DomeForgeImage& img, int py)
    {
        const DomeForgeConfig& cfg = *sc.cfg;
        const Geo& g = sc.g;
        const int ps = std::max(1, cfg.pixelSize), ss = std::max(1, std::min(4, cfg.ssaa));
        const int W = img.width;
        const DomeForgeRgb bg = cfg.bg;
        const double aa = ps;   // one output pixel, in logical px
        double col[3], tmp[3];
        const bool hasSock = !g.sockets.empty();
        const double inv = 1.0 / (ss * ss);

        for (int px = 0; px < W; px++)
        {
            double ar = 0, ag = 0, ab = 0, aA = 0;
            for (int sy = 0; sy < ss; sy++)
            {
                for (int sx = 0; sx < ss; sx++)
                {
                    const double lx = (px + (sx + 0.5) / ss) * ps, ly = (py + (sy + 0.5) / ss) * ps;
                    const double X = lx - g.cx, Y = g.cy - ly;
                    const double r = std::hypot(X, Y);

                    // background
                    double R = 0, G = 0, B = 0, A = 0;
                    if (cfg.bgOn)
                    {
                        const double n = (HashInt((int)lx, (int)ly, sc.seed + 1) - 0.5) * cfg.bgNoise;
                        R = bg.r + n;
                        G = bg.g + n;
                        B = bg.b + n;
                        A = 1;
                    }

                    // metal: ring and socket loops are one merged piece, with the hollows cut out
                    const double dU = MetalSDF(X, Y, g, nullptr);
                    const double dH = hasSock ? HollowSDF(X, Y, g) : 1e9;
                    if (hasSock && cfg.hollowDark > 0)
                    {
                        const double hol = (1 - Sstep(-aa * 0.5, aa * 0.5, dH)) * cfg.hollowDark;
                        if (hol > 0)
                        {
                            const double ao = 0.05 + 0.07 * Sstep(0, -3 * g.us, dH);   // shadowed hole, darkest at its edge
                            R = Lerp(R, ao, hol);
                            G = Lerp(G, ao, hol);
                            B = Lerp(B, ao, hol);
                            A = A + hol * (1 - A);
                        }
                    }
                    const double fa = (1 - Sstep(-aa * 0.5, aa * 0.5, dU)) * Sstep(g.Rd - aa * 0.5, g.Rd + aa * 0.5, r) *
                                      Sstep(-aa * 0.5, aa * 0.5, dH);
                    if (fa > 0)
                    {
                        ShadeFrame(X, Y, lx, ly, r, dH, sc, tmp);
                        R = Lerp(R, tmp[0], fa);
                        G = Lerp(G, tmp[1], fa);
                        B = Lerp(B, tmp[2], fa);
                        A = A + fa * (1 - A);
                    }

                    // glass dome
                    const double ga = 1 - Sstep(g.Rd - aa * 0.5, g.Rd + aa * 0.5, r);
                    if (ga > 0)
                    {
                        const double rcl = std::min(r, g.Rd - 0.01);
                        const double k = r > 0 ? rcl / r : 1;
                        ShadeGlass(X * k, Y * k, sc, col);
                        R = Lerp(R, col[0], ga);
                        G = Lerp(G, col[1], ga);
                        B = Lerp(B, col[2], ga);
                        A = A + ga * (1 - A);
                    }

                    // socket lights: a tiny point light in the middle of each cavity with a small halo
                    if (hasSock && cfg.socketLights && cfg.lightGlow > 0)
                    {
                        const double reach = g.ltPsig * 3 + g.ltPt;
                        for (const Socket& so : g.sockets)
                        {
                            const double dx = X - so.c * so.rc, dy = Y - so.s * so.rc;
                            if (dx > reach || dx < -reach || dy > reach || dy < -reach) continue;
                            const double d = std::hypot(dx, dy);
                            const double core = 1 - Sstep(g.ltPt - aa * 0.5, g.ltPt + aa * 0.5, d);
                            const double dd = std::max(d - g.ltPt, 0.0), gl = std::exp(-dd * dd / (2 * g.ltPsig * g.ltPsig)) * cfg.lightGlow;
                            R += sc.lightCol.r * gl * 0.9;
                            G += sc.lightCol.g * gl * 0.9;
                            B += sc.lightCol.b * gl * 0.9;
                            A = A + gl * 0.8 * (1 - A);
                            R = Lerp(R, sc.lightCore.r, core);
                            G = Lerp(G, sc.lightCore.g, core);
                            B = Lerp(B, sc.lightCore.b, core);
                            A = A + core * (1 - A);
                        }
                    }
                    // optional rim bars beside each socket
                    for (const RimLight& li : g.lights)
                    {
                        const double lyy = X * li.c + Y * li.s - li.r0;   // radial offset from the bar
                        if (lyy > g.ltReach || lyy < -g.ltReach) continue;
                        const double lxx = -X * li.s + Y * li.c;          // along the rim
                        if (lxx > g.ltReach + g.ltLen || lxx < -g.ltReach - g.ltLen) continue;
                        const double d = RoundedRectSDF(lxx, lyy, g.ltLen / 2, g.ltW / 2, g.ltW / 2);
                        const double core = 1 - Sstep(-aa * 0.5, aa * 0.5, d);
                        const double dd = std::max(d, 0.0), gl = std::exp(-dd * dd / (2 * g.ltSig * g.ltSig)) * cfg.lightGlow;
                        R += sc.lightCol.r * gl * 0.9;
                        G += sc.lightCol.g * gl * 0.9;
                        B += sc.lightCol.b * gl * 0.9;
                        A = A + gl * 0.85 * (1 - A);
                        R = Lerp(R, sc.lightCore.r, core);
                        G = Lerp(G, sc.lightCore.g, core);
                        B = Lerp(B, sc.lightCore.b, core);
                        A = A + core * (1 - A);
                    }

                    ar += R * A;   // premultiplied accumulate
                    ag += G * A;
                    ab += B * A;
                    aA += A;
                }
            }
            const size_t o = ((size_t)py * W + px) * 4;
            if (aA > 0)
            {
                double R = ar / aA, G = ag / aA, B = ab / aA;
                if (cfg.levels > 1)
                {
                    const double q = cfg.levels - 1;
                    R = JsRound(R * q) / q;
                    G = JsRound(G * q) / q;
                    B = JsRound(B * q) / q;
                }
                img.rgba[o] = ToByte(Clamp(R, 0.0, 1.0) * 255);
                img.rgba[o + 1] = ToByte(Clamp(G, 0.0, 1.0) * 255);
                img.rgba[o + 2] = ToByte(Clamp(B, 0.0, 1.0) * 255);
                img.rgba[o + 3] = ToByte(Clamp(aA * inv, 0.0, 1.0) * 255);
            }
        }
    }
}

struct DomeForgeJob::State
{
    DomeForgeImage img;
    int next = 0;
    std::function<void(DomeForgeImage&, int)> row;   // captures whatever it reads
};

DomeForgeJob DomeForgeJob::FromRows(DomeForgeImage img, std::function<void(DomeForgeImage&, int)> row)
{
    DomeForgeJob job;
    job.st = std::make_shared<State>();
    job.st->img = std::move(img);
    job.st->row = std::move(row);
    return job;
}

DomeForgeJob DomeForgeJob::Sprite(const DomeForgeConfig& cfgIn, DomeForgeKind kind)
{
    auto cfg = std::make_shared<DomeForgeConfig>(cfgIn);
    auto sc = std::make_shared<Scene>();
    BuildScene(*cfg, kind, *sc);   // sc->cfg points at *cfg, which the row function keeps alive
    const int ps = std::max(1, cfg->pixelSize);
    const int W = (int)std::ceil(sc->g.S / ps);
    DomeForgeImage img;
    img.width = W;
    img.height = W;
    img.rgba.assign((size_t)W * W * 4, 0);
    img.cx = sc->g.cx / ps;
    img.cy = sc->g.cy / ps;
    return FromRows(std::move(img), [cfg, sc](DomeForgeImage& out, int py) { RenderSpriteRow(*sc, out, py); });
}

bool DomeForgeJob::Step(double budgetMs)
{
    if (!st || Done()) return true;
    const auto t0 = std::chrono::steady_clock::now();
    do
    {
        st->row(st->img, st->next++);
    }
    while (st->next < st->img.height &&
           std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() < budgetMs);
    return Done();
}

bool DomeForgeJob::Done() const { return !st || st->next >= st->img.height; }
const DomeForgeImage& DomeForgeJob::Image() const { return st->img; }
DomeForgeImage DomeForgeJob::TakeImage() { return std::move(st->img); }

DomeForgeImage DomeForgeRender(const DomeForgeConfig& cfg, DomeForgeKind kind)
{
    DomeForgeJob job = DomeForgeJob::Sprite(cfg, kind);
    job.Step(1e30);
    return job.TakeImage();
}

void DomeForgeBlit(DomeForgeImage& dst, const DomeForgeImage& src, int x0, int y0)
{
    for (int y = 0; y < src.height; y++)
    {
        const int dy = y0 + y;
        if (dy < 0 || dy >= dst.height) continue;
        for (int x = 0; x < src.width; x++)
        {
            const int dx = x0 + x;
            if (dx < 0 || dx >= dst.width) continue;
            const size_t si = ((size_t)y * src.width + x) * 4, di = ((size_t)dy * dst.width + dx) * 4;
            const double a = src.rgba[si + 3] / 255.0;
            if (a <= 0) continue;
            const double ia = 1 - a;
            dst.rgba[di] = ToByte(src.rgba[si] * a + dst.rgba[di] * ia);
            dst.rgba[di + 1] = ToByte(src.rgba[si + 1] * a + dst.rgba[di + 1] * ia);
            dst.rgba[di + 2] = ToByte(src.rgba[si + 2] * a + dst.rgba[di + 2] * ia);
            dst.rgba[di + 3] = ToByte(std::min(255.0, dst.rgba[di + 3] + a * 255));
        }
    }
}
