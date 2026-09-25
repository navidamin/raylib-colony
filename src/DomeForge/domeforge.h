// domeforge.h — the DomeForge lunar-base art set, ported to C++.
//
// A 1:1 port of prototypes/dome-forge/dome-forge-engine.js (dome sprites) and
// dome-forge-base.js (roads, ground, layout). Every shape is a signed distance
// field and every pixel is computed from it, so this is pure CPU math on an
// RGBA buffer: no GL, callable before a window exists.
//
// Deterministic like the JS. Arithmetic is in double, the hash is uint32, and
// JS rounding is reproduced where it differs from C (Math.round rounds halves
// up, Uint8ClampedArray rounds halves to even), so a render diffs against
// `node prototypes/dome-forge/examples/render.js` at the same config.
// tools/domeforge/ runs that diff.
//
// Not ported: the sprite sheet and the lighting presets A-D (a design-tool
// convenience), and the 3D viewer (dome-forge-3d.js). See
// docs/design/sect-view/domeforge-study.md.
//
//     DomeForgeConfig cfg = DomeForgeDefaults();
//     cfg.color = DomeForgeHex("#1fb75b");
//     DomeForgeImage dome = DomeForgeRender(cfg, DomeForgeKind::UNIT);
//     DomeForgeLayout lay = DomeForgeMakeLayout(cfg, 0.5);
//     DomeForgeImage roads = DomeForgeRenderRoads(cfg, w, h, lay.prims, lay.s);
#ifndef DOMEFORGE_H
#define DOMEFORGE_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct DomeForgeRgb
{
    double r = 0.0, g = 0.0, b = 0.0;
};

// '#rrggbb' -> 0..1, as hexToRgb (grey 0.5 on anything malformed).
DomeForgeRgb DomeForgeHex(const std::string& hex);

enum class DomeForgeKind { UNIT, CENTRAL };

// Per-kind geometry, fractions of that sprite's size (the JS `unit` / `central` objects).
struct DomeForgeShape
{
    int size = 256;
    double domeRadius = 0.33;
    double ringWidth = 0.07;
    int sides = 0;                 // 0 = round rim; >= 3 = polygon with rounded corners
    double corner = 0.085;
    bool socketsAtCorners = false;
    int socketCount = 1;
    double socketStart = 270.0;    // degrees, math convention (0 right, 90 up)
};

// DEFAULTS of both JS modules in one struct. Field names follow the JS keys.
struct DomeForgeConfig
{
    DomeForgeShape unit;
    DomeForgeShape central;

    // glass
    DomeForgeRgb color;
    double lightAz = 140.0, lightEl = 45.0;
    double ambient = 0.5, diffuse = 0.55;
    double hlX = -0.02, hlY = 0.55;
    double shininess = 22.0, specInt = 0.8, specWhite = 0.62;
    double glintSize = 0.05, glintAspect = 1.0, glintStrength = 0.95;
    double rimAz = 315.0, rimInt = 0.4, rimPow = 3.5;
    double limbDark = 0.42, limbPow = 2.0;
    double edgeShadow = 0.3, edgeShadowW = 0.25, edgeLine = 0.3;

    // hex cells
    double hexCells = 0.09, hexCurve = 1.3, hexLens = 0.5, facetShade = 0.35;
    double hexRot = 0.0, hexLine = 0.9, hexLineDark = 0.16, hexLineLight = 0.22;
    double facetVar = 0.07, facetBevel = 0.08;
    double litBase = 0.02, litNear = 0.35, litAmount = 0.35;

    // frame
    DomeForgeRgb frameColor;
    double frameAmbient = 0.55, frameDiffuse = 0.5, frameSpec = 0.8, frameShine = 18.0;
    std::string frameProfile = "plate";   // plate | classic | simple | heavy | chamfer
    double bevel = 2.2, metalEnv = 0.8, edgeLight = 0.45, outline = 0.7;
    double outerLip = 0.2;
    std::string grainStyle = "mottled";  // mottled | brushed | speckle | smooth
    double grain = 0.11, grainScale = 1.0;
    double segments = 0.0, segDepth = 0.5;

    // sockets (px at unit size 256)
    bool socketOn = true;
    double socketW = 34.0, socketH = 24.0, socketT = 7.5, socketCorner = 5.0;
    bool socketChamfer = true;           // socketCorners: 'chamfer' (true) | 'round'
    double hollowDark = 0.8, socketInset = 3.5, socketFillet = 5.0;
    bool socketLights = true;
    DomeForgeRgb lightColor;
    double lightGlow = 0.9, lightGlowR = 5.0, lightSize = 1.5;
    bool rimLights = false;
    double lightAngle = 36.0, lightRadial = 0.5, lightLen = 12.0, lightW = 3.0;

    // output
    int pixelSize = 1, ssaa = 2, levels = 0;
    bool bgOn = true;
    DomeForgeRgb bg;
    double bgNoise = 0.02;
    int seed = 7;

    // ---- base (px at a 1254 px base) ----
    double baseSize = 1254.0;
    double roadW = 30.0, roadOuterW = 14.0, curbW = 5.0, fillet = 20.0, filletDome = 16.0;
    DomeForgeRgb roadColor;
    double roadMottle = 0.08, roadGrain = 0.05;
    DomeForgeRgb curbColor;
    double curbSeg = 9.0, curbSegDepth = 0.3, curbBevel = 1.4, curbShine = 0.6, curbShadow = 0.5, curbOutline = 0.7;
    double bankW = 5.0, bankLight = 0.28, bankShadow = 0.5, domeShadowW = 6.0, domeShadow = 0.7;
    bool laneOn = true;
    DomeForgeRgb laneColor;
    double laneW = 1.4, laneDash = 18.0, laneGap = 6.0, laneAlpha = 0.6;
    DomeForgeRgb groundColor;
    double groundMottle = 0.12, groundGrain = 0.07;
    double craterBig = 0.9, craterSmall = 0.85, craterDepth = 2.4, craterRim = 1.5;
    double orbit = 340.0, ringRoadR = 485.0, centralSize = 475.0, unitSize = 236.0, offsetY = -42.0;
    DomeForgeRgb centralColor, cardinalColor, diagonalColor;
    bool spokesBeyond = false, domeRoads = true;
    bool unitSockets = true, centralSockets = true, socketsToCentre = true;

    // ---- extensions: not in the JS ----
    // Off by default, so the port still diffs bit-exact against the prototype
    // (tools/domeforge/domeforge_diff.sh). The sect view switches them on to
    // match the concept art (samples/base-compare-reference.png, left half):
    // each dome sits in a collar of road that the spokes flow into, and amber
    // light bars run on the road centre line.
    double domeCollar = 0.0;        // px at 1254: road band around every dome rim
    bool roadLights = false;        // light bars on the centre line
    double roadLightLen = 20.0, roadLightW = 4.0;   // px at 1254
    double roadLightGlowR = 7.0, roadLightGlow = 0.9;
    int ringLights = 8;             // on the ring road, midway between spokes
    int collarLights = 0;           // small lamps round each unit dome's collar
    int coreCollarLights = 0;       // ... and round the core's
    double collarLightSize = 3.0;   // px at 1254, their diameter
    double spokesBeyondLen = 0.0;   // px at 1254 past the ring; 0 = to the base's edge (the JS)
    int exitRoads = 15;             // which of those: bits N=1 W=2 S=4 E=8 (15 = all four, the JS)
    double exitFade = 0.0;          // px at 1254: the exit roads fade into the ground at their end
};

// Set one config field by its JS name ("roadW", "roadColor", "laneOn", ...),
// value as text ("34", "#4a4744", "0"). False if the key is unknown. For
// tuning tools; the game sets fields directly.
bool DomeForgeSetParam(DomeForgeConfig& cfg, const std::string& key, const std::string& value);

DomeForgeConfig DomeForgeDefaults();

// Straight-alpha RGBA, row-major, top row first. (cx, cy) is the dome centre in
// the sprite, in output pixels (sprites are auto-centred so rim + sockets fit).
struct DomeForgeImage
{
    int width = 0, height = 0;
    std::vector<uint8_t> rgba;
    double cx = 0.0, cy = 0.0;
};

DomeForgeImage DomeForgeRender(const DomeForgeConfig& cfg, DomeForgeKind kind);

// ---- base ----

// A road primitive, in math coordinates (y up) at the layout's scale.
struct DomeForgePrim
{
    enum Type { SEG, RING, DISC } t = SEG;
    double ax = 0, ay = 0, bx = 0, by = 0, w = 0;   // SEG
    double fade = 0;            // SEG, extension: fade into the ground over this many px before b
    double cx = 0, cy = 0, r = 0;                   // RING (w too) / DISC
};

struct DomeForgeDome
{
    DomeForgeKind kind = DomeForgeKind::UNIT;
    double x = 0, y = 0;        // math coords (y up), px
    double size = 0, rout = 0;
    double angle = 0;           // degrees; unit domes only
    bool cardinal = false;
};

// A light bar on the road centre line (extension; cfg.roadLights).
struct DomeForgeLight
{
    double x = 0, y = 0;        // math coords (y up), px
    double dx = 1, dy = 0;      // unit direction along the road
    double len = 0;             // px; 0 = cfg.roadLightLen (a bar), else a lamp this long
};

struct DomeForgeLayout
{
    double A = 0, s = 0;        // base size in px, and px per 1254-px unit
    std::vector<DomeForgeDome> domes;   // [0] central, then 8 units from 90 deg in 45 deg steps
    std::vector<DomeForgePrim> prims;
    std::vector<DomeForgeLight> lights;   // empty unless cfg.roadLights
};

DomeForgeLayout DomeForgeMakeLayout(const DomeForgeConfig& cfg, double scale);

// The road layer alone, transparent elsewhere (kerbs, banks, lane dashes, rim shadows).
DomeForgeImage DomeForgeRenderRoads(const DomeForgeConfig& cfg, int W, int H,
                                    const std::vector<DomeForgePrim>& prims, double scale,
                                    const std::vector<DomeForgeLight>& lights = {});

// Cratered regolith. The game draws real terrain instead; ported so the full base diffs.
DomeForgeImage DomeForgeRenderGround(const DomeForgeConfig& cfg, int W, int H, double scale);

// The per-dome sprite config renderBase derives: colour, size and socket aim.
DomeForgeConfig DomeForgeSpriteConfig(const DomeForgeConfig& cfg, const DomeForgeLayout& lay,
                                      const DomeForgeDome& d, const DomeForgeRgb& color);

// ground + roads + nine domes, as renderBase (for the diff harness).
DomeForgeImage DomeForgeRenderBase(const DomeForgeConfig& cfg, double scale);

// Alpha-over blit, exactly the JS `blit`.
void DomeForgeBlit(DomeForgeImage& dst, const DomeForgeImage& src, int x0, int y0);

// ---- rendering a row at a time ----
// A bake of the whole set takes about a second of CPU at 1280x720 in an
// optimised build and several in a debug one, too long to spend in one frame.
// A job renders the same pixels as the one-shot calls above, but a slice of
// rows per Step(), so the game can spread it across frames (no threads: the
// web build has none).
class DomeForgeJob
{
public:
    static DomeForgeJob Sprite(const DomeForgeConfig& cfg, DomeForgeKind kind);
    static DomeForgeJob Roads(const DomeForgeConfig& cfg, int W, int H,
                              const std::vector<DomeForgePrim>& prims, double scale,
                              const std::vector<DomeForgeLight>& lights = {});

    // Renders rows until budgetMs is spent (at least one row). True when finished.
    bool Step(double budgetMs);
    bool Done() const;
    const DomeForgeImage& Image() const;
    DomeForgeImage TakeImage();

    // A job from any per-row renderer (how Roads is built).
    static DomeForgeJob FromRows(DomeForgeImage img, std::function<void(DomeForgeImage&, int)> row);

    struct State;

private:
    std::shared_ptr<State> st;
};

#endif // DOMEFORGE_H
