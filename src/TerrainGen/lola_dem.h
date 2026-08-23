#ifndef LOLA_DEM_H
#define LOLA_DEM_H

// Real lunar elevation from the LOLA LDEM_16 model (C++ port of
// prototypes/planet_visuals/elevation.py).
//
// Data: NASA CGI Moon Kit ldem_16_uint.tif, derived from the Lunar
// Orbiter Laser Altimeter (LRO). 5760x2880 equirectangular, 16 pixels
// per degree (~1.895 km/px at the equator), unsigned 16-bit values in
// HALF-METRE units: metres = raw * 0.5 - 10000, relative to the
// 1737.4 km reference radius. The 10 km offset was calibrated
// empirically against LOLA elevations of Apollo 11 and Chang'e 4;
// decoded global range -8982..+10686 m matches the real lunar range
// at 16 ppd smoothing.
//
// The TIFF is read by a minimal built-in parser (little-endian,
// uncompressed, strip-organised, one 16-bit sample per pixel) so the
// game gains no image-library dependency.
//
// This is what binds the zoom-anywhere view to physical ground truth:
// any (lat, lon) can be asked for real elevation, relief and slope.

#include <cstdint>
#include <string>
#include <vector>

const double LOLA_MOON_RADIUS_M = 1737400.0;
const double LOLA_M_PER_DEG = 30322.68;    // pi * 1737.4 km / 180

// A resampled window of real terrain: elevation (metres vs the
// reference radius) and slope (degrees), row-major, north at row 0.
struct LolaWindow
{
    int resolution = 0;             // pixels across (square)
    double latDeg = 0.0;            // window centre
    double lonDeg = 0.0;
    double spanKm = 0.0;            // north-south extent
    std::vector<float> elevationM;  // resolution * resolution
    std::vector<float> slopeDeg;    // resolution * resolution
    float minElevationM = 0.0f;
    float maxElevationM = 0.0f;
};

class LolaDem
{
public:
    // Loads the DEM. Returns false (with a TraceLog-style message on
    // stderr) if the file is missing or not the expected layout.
    bool Load(const std::string& path);

    bool IsLoaded() const { return width > 0; }
    int Width() const { return width; }
    int Height() const { return height; }

    // Bilinear-interpolated elevation in metres vs the reference radius.
    float ElevationM(double latDeg, double lonDeg) const;

    // Resample a window square in km (lon span widened by 1/cos(lat))
    // to res x res. Slope is computed at the DEM's native resolution,
    // then resampled — resampling first would flatten it.
    //
    // detailStrength > 0 synthesizes surface detail below the DEM's
    // ~1.9 km/px floor: a fractal regolith spectrum plus a scattered
    // small-crater population, deterministic per location (anchored to
    // global coordinates, so the same ground regenerates identically
    // whatever the window framing). The real LOLA landforms stay the
    // backbone; synthesis only fades in at wavelengths the data cannot
    // resolve. 1.0 is the calibrated look, 0 disables.
    LolaWindow Window(double latDeg, double lonDeg, double spanKm,
                      int res, float detailStrength = 0.0f) const;

    // Resample an explicit lat/lon rectangle (degrees) to outW x outH.
    // Used for the full near side (lat -90..90, lon -90..90), where a
    // square-km window makes no sense. Slope handled as above.
    LolaWindow WindowDegrees(double lat0, double lat1,
                             double lon0, double lon1,
                             int outW, int outH) const;

private:
    float Decode(uint16_t raw) const
    {
        return (float)raw * 0.5f - 10000.0f;
    }
    float Sample(int x, int y) const;    // decoded, wrapped/clamped

    int width = 0;
    int height = 0;
    std::vector<uint16_t> raw;
};

#endif // LOLA_DEM_H
