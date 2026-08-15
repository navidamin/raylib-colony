#include "sect.h"
#include "colony.h"
#include <iostream>

Sect::Sect(Vector2 &position, ResourceManager& resource, TimeManager& time)
    : resourceManager(resource),
      timeManager(time),
      defaultCoreRadius(50.0f),
      coreRadius(defaultCoreRadius),
      color(CHINAROSE),
      SectPosition(position),
      units(),
      core(nullptr),
      development_percentage(0.0f),
      production_priority(),
      resourceStorage()
{
    // Initialize all resource types to 0
    resourceStorage[ResourceType::H2] = 0.0f;
    resourceStorage[ResourceType::O2] = 0.0f;
    resourceStorage[ResourceType::C] = 0.0f;
    resourceStorage[ResourceType::Fe] = 0.0f;
    resourceStorage[ResourceType::Si] = 0.0f;
    resourceStorage[ResourceType::Ti] = 0.0f;
    resourceStorage[ResourceType::Al] = 0.0f;
    resourceStorage[ResourceType::Ca] = 0.0f;
    resourceStorage[ResourceType::ENERGY] = 0.0f;
    resourceStorage[ResourceType::WATER] = 0.0f;
    resourceStorage[ResourceType::FOOD] = 0.0f;
    resourceStorage[ResourceType::SCIENCE] = 0.0f;
    resourceStorage[ResourceType::MANPOWER] = SECT_BASE_MANPOWER;  // Constant manpower per sect

    // Initialize storage capacities
    storageCapacity[ResourceType::H2] = SECT_BASE_STORAGE;
    storageCapacity[ResourceType::O2] = SECT_BASE_STORAGE;
    storageCapacity[ResourceType::C] = SECT_BASE_STORAGE;
    storageCapacity[ResourceType::Fe] = SECT_BASE_STORAGE;
    storageCapacity[ResourceType::Si] = SECT_BASE_STORAGE;
    storageCapacity[ResourceType::Ti] = SECT_BASE_STORAGE;
    storageCapacity[ResourceType::Al] = SECT_BASE_STORAGE;
    storageCapacity[ResourceType::Ca] = SECT_BASE_STORAGE;
    storageCapacity[ResourceType::ENERGY] = SECT_BASE_STORAGE;
    storageCapacity[ResourceType::WATER] = SECT_BASE_STORAGE;
    storageCapacity[ResourceType::FOOD] = SECT_BASE_STORAGE;
    storageCapacity[ResourceType::SCIENCE] = SECT_BASE_STORAGE;
    storageCapacity[ResourceType::MANPOWER] = SECT_BASE_STORAGE;

    CreateInitialUnits(position);

    // Load textures for visual rendering
    LoadTextures();
}

Sect::~Sect() {
    // Free texture memory
    UnloadTextures();

    // Delete all units
    for (auto unit : units) {
        delete unit;
    }
}

void Sect::AddUnit(Unit* unit) {
    units.push_back(unit);
    std::cout << "New unit added to the sect." << std::endl;
}


void Sect::ConsumeResources() {
    // TODO: Implement resource consumption logic
    std::cout << "Sect resources consumed." << std::endl;
}

void Sect::BuildUnit(std::string unit_type) {
    // TODO: Implement unit building logic
    std::cout << "Building new unit of type: " << unit_type << std::endl;
}

void Sect::UpgradeUnit(Unit* unit) {
    // TODO: Implement unit upgrade logic
    std::cout << "Upgrading unit." << std::endl;
}

void Sect::Update(float deltaTime) {
    static int lastCollectionDay = 1;
    int currentDay = timeManager.GetCurrentDay();

    // Update all units
    for (Unit* unit : units) {
        if (unit) {
            unit->Update(deltaTime);
        }
    }

    // Generate ambient solar energy
    GenerateAmbientEnergy(deltaTime, timeManager.GetTimeOfDay());

    // Regenerate manpower toward base level
    float currentManpower = resourceStorage[ResourceType::MANPOWER];
    if (currentManpower < SECT_BASE_MANPOWER)
    {
        float regenRate = SECT_BASE_MANPOWER * 0.1f;  // 10% of base per second
        resourceStorage[ResourceType::MANPOWER] = std::min(SECT_BASE_MANPOWER,
            currentManpower + regenRate*deltaTime);
    }

    // Update road construction if any are in progress
    UpdateRoadConstruction(deltaTime);
}



void Sect::UpdateRoadConstruction(float deltaTime) {
    // Update each road under construction
    auto it = roadsUnderConstruction.begin();
    while (it != roadsUnderConstruction.end()) {
        it->progress += deltaTime;

        if (it->progress >= it->totalTime) {
            // Road construction complete
            // Add to completed roads list (implementation depends on your road system)
            it = roadsUnderConstruction.erase(it);
        } else {
            ++it;
        }
    }
}

void Sect::CreateInitialUnits(Vector2& position) {
    std::vector<std::string> unit_types = {
        "Extraction", "Farming", "Manufacture", "Transport", "Communication", "Research","Energy", "Construction"
    };

    // Initialize and stary each unit
    for (const auto& type : unit_types) {
        Unit* unit = new Unit(type, position, resourceManager, timeManager, resourceStorage, storageCapacity);
        if (type == "Extraction") {
            unit->Start();
            core = unit; // Set the Extraction unit as the core
        } else {
            unit->Stop();
        }
        AddUnit(unit);
    }

    std::cout << "All initial units created for the sect." << std::endl;
}


void Sect::DrawInColonyView(Vector2 pos) {
    coreRadius = defaultCoreRadius; // Use constant world-space radius

    // Draw main sect (dome texture or fallback circle)
    if (domeTexture.id != 0) {
        float textureDiameter = coreRadius * 2.0f;
        Rectangle source = {0.0f, 0.0f, (float)domeTexture.width, (float)domeTexture.height};
        Rectangle dest = {
            pos.x - coreRadius,
            pos.y - coreRadius,
            textureDiameter,
            textureDiameter
        };
        Vector2 origin = {0.0f, 0.0f};
        DrawTexturePro(domeTexture, source, dest, origin, 0.0f, WHITE);
    } else {
        // Fallback to circle if texture not loaded
        DrawCircle(pos.x, pos.y, coreRadius, color);
    }

    // Draw active units indicator as small images around the sect
    float indicatorRadius = coreRadius * 0.35f;
    float orbitRadius = coreRadius * 1.3f;

    for (size_t i = 0; i < units.size(); i++) {
        float angle = (90.0f - (i * 45.0f)) * DEG2RAD;  // 8 units, 45 degrees apart
        Vector2 indicatorPos = {
            pos.x + orbitRadius * cosf(angle),
            pos.y - orbitRadius * sinf(angle)
        };

        // Get unit type for texture lookup
        std::string unitType = units[i]->GetUnitType();
        auto texIt = unitTextures.find(unitType);

        // Draw unit (texture or fallback circle)
        if (texIt != unitTextures.end() && texIt->second.id != 0) {
            float textureDiameter = indicatorRadius * 2.0f;
            Rectangle source = {0.0f, 0.0f, (float)texIt->second.width, (float)texIt->second.height};
            Rectangle dest = {
                indicatorPos.x - indicatorRadius,
                indicatorPos.y - indicatorRadius,
                textureDiameter,
                textureDiameter
            };
            Vector2 origin = {0.0f, 0.0f};
            DrawTexturePro(texIt->second, source, dest, origin, 0.0f, WHITE);

            // Add green glow ring for active units
            if (units[i]->GetStatus() == "active") {
                DrawCircleLines(indicatorPos.x, indicatorPos.y, indicatorRadius * 1.15f, GREEN);
            }
        } else {
            // Fallback to circle if texture not available
            if (units[i]->GetStatus() == "active") {
                DrawCircle(indicatorPos.x, indicatorPos.y, indicatorRadius, GREEN);
            } else {
                DrawCircle(indicatorPos.x, indicatorPos.y, indicatorRadius, CHINAROSE);
            }
        }
    }

    // Draw development percentage as a progress arc
    if (development_percentage > 0) {
        DrawRing(
            pos,
            coreRadius * 1.1f,
            coreRadius * 1.2f,
            0,
            development_percentage * 360,
            32,
            Fade(GREEN, 0.5f)
        );
    }
}


// ---------------------------------------------------------------------------
// Sect view "orbital layout" visuals — everything below is drawn procedurally
// with raylib primitives (no sprites required).
// ---------------------------------------------------------------------------
namespace
{
    Color MixColor(Color a, Color b, float t)
    {
        return Color{
            (unsigned char)(a.r + (b.r - a.r) * t),
            (unsigned char)(a.g + (b.g - a.g) * t),
            (unsigned char)(a.b + (b.b - a.b) * t),
            (unsigned char)(a.a + (b.a - a.a) * t)
        };
    }

    Color UnitAccentColor(const std::string& type)
    {
        if (type == "Extraction")    return Color{255, 168, 64, 255};   // amber
        if (type == "Farming")       return Color{124, 220, 92, 255};   // green
        if (type == "Manufacture")   return Color{255, 122, 84, 255};   // coral
        if (type == "Transport")     return Color{72, 208, 190, 255};   // teal
        if (type == "Communication") return Color{84, 200, 255, 255};   // cyan
        if (type == "Research")      return Color{168, 214, 255, 255};  // ice blue
        if (type == "Energy")        return Color{64, 150, 255, 255};   // blue
        if (type == "Construction")  return Color{255, 210, 80, 255};   // yellow
        return Color{200, 200, 200, 255};
    }

    // Small glowing status light (green LED look)
    void DrawLed(Vector2 p, float r, Color c)
    {
        DrawCircleV(p, r * 2.0f, Fade(c, 0.25f));
        DrawCircleV(p, r, c);
        DrawCircleV(Vector2{p.x - r * 0.3f, p.y - r * 0.3f}, r * 0.35f, Fade(WHITE, 0.7f));
    }

    // Procedural icon for each unit type, drawn inside a [-1,1] box scaled by s
    void DrawUnitGlyph(const std::string& type, Vector2 c, float s, Color col)
    {
        auto P = [&](float x, float y) { return Vector2{c.x + x * s, c.y + y * s}; };
        float lw = s * 0.22f;
        float thin = s * 0.14f;
        Color faceDark = Color{20, 24, 22, 255};

        if (type == "Extraction")
        {
            // Drill derrick over a bore hole
            DrawLineEx(P(-0.6f, 0.75f), P(0.0f, -0.75f), lw, col);
            DrawLineEx(P(0.6f, 0.75f), P(0.0f, -0.75f), lw, col);
            DrawLineEx(P(-0.14f, -0.15f), P(0.14f, -0.15f), thin * 0.8f, col);
            DrawLineEx(P(-0.38f, 0.4f), P(0.38f, 0.4f), thin * 0.8f, col);
            DrawLineEx(P(0.0f, -0.75f), P(0.0f, 0.25f), thin * 0.8f, col);
            DrawTriangle(P(0.16f, 0.25f), P(-0.16f, 0.25f), P(0.0f, 0.62f), col);
            DrawLineEx(P(-0.8f, 0.8f), P(0.8f, 0.8f), thin, col);
        }
        else if (type == "Farming")
        {
            // Sprout with two side leaves
            DrawLineEx(P(0.0f, 0.7f), P(0.0f, -0.25f), lw, col);
            DrawTriangle(P(0.0f, -0.45f), P(-0.7f, -0.6f), P(0.0f, 0.0f), col);
            DrawTriangle(P(0.0f, 0.0f), P(0.7f, -0.6f), P(0.0f, -0.45f), col);
            DrawTriangle(P(0.0f, -0.95f), P(-0.22f, -0.35f), P(0.22f, -0.35f), col);
            DrawLineEx(P(-0.55f, 0.7f), P(0.55f, 0.7f), thin, col);
        }
        else if (type == "Manufacture")
        {
            // Factory with sawtooth roof and chimney
            DrawRectangleRec(Rectangle{c.x - 0.72f * s, c.y + 0.02f * s, 1.44f * s, 0.62f * s}, col);
            for (int k = 0; k < 3; k++)
            {
                float x0 = -0.72f + k * 0.48f;
                DrawTriangle(P(x0, -0.42f), P(x0, 0.05f), P(x0 + 0.44f, 0.05f), col);
            }
            DrawRectangleRec(Rectangle{c.x + 0.30f * s, c.y - 0.78f * s, 0.18f * s, 0.85f * s}, col);
            for (int k = 0; k < 3; k++)
            {
                DrawRectangleRec(Rectangle{c.x + (-0.55f + k * 0.42f) * s, c.y + 0.18f * s,
                                           0.22f * s, 0.28f * s}, faceDark);
            }
        }
        else if (type == "Transport")
        {
            // Cargo truck
            DrawRectangleRec(Rectangle{c.x - 0.78f * s, c.y - 0.35f * s, 1.0f * s, 0.62f * s}, col);
            DrawRectangleRec(Rectangle{c.x + 0.28f * s, c.y - 0.28f * s, 0.44f * s, 0.55f * s}, col);
            DrawRectangleRec(Rectangle{c.x + 0.36f * s, c.y - 0.20f * s, 0.24f * s, 0.18f * s}, faceDark);
            float wheelY = 0.42f;
            float wheelXs[3] = {-0.5f, -0.05f, 0.5f};
            for (float wx : wheelXs)
            {
                DrawCircleV(P(wx, wheelY), 0.17f * s, col);
                DrawCircleV(P(wx, wheelY), 0.07f * s, faceDark);
            }
        }
        else if (type == "Communication")
        {
            // Broadcast tower with beacon and signal dots
            DrawLineEx(P(-0.42f, 0.7f), P(0.0f, -0.55f), lw * 0.8f, col);
            DrawLineEx(P(0.42f, 0.7f), P(0.0f, -0.55f), lw * 0.8f, col);
            DrawLineEx(P(-0.30f, 0.35f), P(0.30f, 0.35f), thin * 0.8f, col);
            DrawLineEx(P(-0.20f, 0.05f), P(0.20f, 0.05f), thin * 0.8f, col);
            DrawLineEx(P(-0.10f, -0.25f), P(0.10f, -0.25f), thin * 0.8f, col);
            DrawCircleV(P(0.0f, -0.68f), 0.10f * s, col);
            DrawCircleV(P(-0.30f, -0.88f), 0.05f * s, col);
            DrawCircleV(P(0.30f, -0.88f), 0.05f * s, col);
            DrawCircleV(P(-0.48f, -0.68f), 0.04f * s, col);
            DrawCircleV(P(0.48f, -0.68f), 0.04f * s, col);
        }
        else if (type == "Research")
        {
            // Erlenmeyer flask with liquid
            DrawRectangleRec(Rectangle{c.x - 0.12f * s, c.y - 0.85f * s, 0.24f * s, 0.5f * s}, col);
            DrawTriangle(P(-0.12f, -0.35f), P(-0.55f, 0.62f), P(0.55f, 0.62f), col);
            DrawTriangle(P(-0.12f, -0.35f), P(0.55f, 0.62f), P(0.12f, -0.35f), col);
            DrawLineEx(P(-0.22f, -0.85f), P(0.22f, -0.85f), thin, col);
            DrawTriangle(P(-0.40f, 0.28f), P(-0.55f, 0.62f), P(0.55f, 0.62f), Fade(WHITE, 0.28f));
            DrawTriangle(P(-0.40f, 0.28f), P(0.55f, 0.62f), P(0.40f, 0.28f), Fade(WHITE, 0.28f));
            DrawCircleV(P(0.05f, 0.12f), 0.05f * s, Fade(WHITE, 0.5f));
        }
        else if (type == "Energy")
        {
            // Lightning bolt
            DrawTriangle(P(0.45f, -0.95f), P(-0.4f, 0.15f), P(0.12f, 0.15f), col);
            DrawTriangle(P(0.4f, -0.15f), P(-0.12f, -0.15f), P(-0.45f, 0.95f), col);
        }
        else if (type == "Construction")
        {
            // Tower crane lifting a block
            DrawLineEx(P(-0.3f, 0.75f), P(-0.3f, -0.6f), lw, col);
            DrawLineEx(P(-0.65f, -0.6f), P(0.65f, -0.6f), lw, col);
            DrawLineEx(P(-0.3f, -0.25f), P(0.5f, -0.6f), thin * 0.8f, col);
            DrawLineEx(P(0.5f, -0.6f), P(0.5f, 0.05f), thin * 0.7f, col);
            DrawRectangleRec(Rectangle{c.x + 0.38f * s, c.y + 0.05f * s, 0.24f * s, 0.24f * s}, col);
            DrawLineEx(P(-0.6f, 0.78f), P(0.05f, 0.78f), thin, col);
        }
        else
        {
            // Unknown unit type: simple diamond placeholder
            DrawTriangle(P(0.0f, -0.7f), P(-0.7f, 0.0f), P(0.7f, 0.0f), col);
            DrawTriangle(P(0.7f, 0.0f), P(-0.7f, 0.0f), P(0.0f, 0.7f), col);
        }
    }

    // Honeycomb "glass" pattern clipped to a circle
    void DrawHexPattern(Vector2 c, float radius, float cell, Color col)
    {
        float dx = cell * 1.732f;
        float dy = cell * 1.5f;
        int nx = (int)(radius / dx) + 1;
        int ny = (int)(radius / dy) + 1;

        for (int gy = -ny; gy <= ny; gy++)
        {
            float offset = ((gy & 1) != 0) ? dx * 0.5f : 0.0f;
            for (int gx = -nx; gx <= nx; gx++)
            {
                Vector2 hc = {c.x + gx * dx + offset, c.y + gy * dy};
                float ddx = hc.x - c.x;
                float ddy = hc.y - c.y;
                if (sqrtf(ddx * ddx + ddy * ddy) > radius - cell) continue;
                DrawPolyLinesEx(hc, 6, cell, 90.0f, 1.0f, col);
            }
        }
    }

    // Glossy hex-glass dome sphere in an arbitrary tint
    void DrawDomeSphere(Vector2 center, float radius, Color base)
    {
        Color edge = MixColor(base, Color{0, 0, 0, 255}, 0.62f);
        Color bright = MixColor(base, Color{255, 255, 255, 255}, 0.30f);

        DrawCircleV(center, radius, edge);

        const int layers = 22;
        for (int i = 0; i < layers; i++)
        {
            float t = (float)i / (float)(layers - 1);
            float layerRadius = radius * (0.97f - 0.62f * t);
            Vector2 layerCenter = {center.x - radius * 0.14f * t,
                                   center.y - radius * 0.18f * t};
            DrawCircleV(layerCenter, layerRadius, MixColor(base, bright, t * t));
        }

        // Hex glass pattern
        DrawHexPattern(center, radius * 0.94f, radius * 0.13f,
                       Fade(MixColor(base, Color{0, 0, 0, 255}, 0.45f), 0.55f));

        // Specular highlight (strong glare, top-left)
        DrawEllipse((int)(center.x - radius * 0.30f), (int)(center.y - radius * 0.42f),
                    radius * 0.36f, radius * 0.19f, Fade(WHITE, 0.30f));
        DrawEllipse((int)(center.x - radius * 0.35f), (int)(center.y - radius * 0.47f),
                    radius * 0.15f, radius * 0.08f, Fade(WHITE, 0.50f));

        // Depth shading toward the rim
        DrawRing(center, radius * 0.88f, radius, 0.0f, 360.0f, 72, Fade(BLACK, 0.35f));
    }

    // Riveted metal bezel ring
    void DrawBezel(Vector2 center, float rIn, float rOut)
    {
        DrawRing(center, rIn, rOut, 0.0f, 360.0f, 96, Color{52, 55, 58, 255});
        DrawRing(center, rOut - 2.0f, rOut, 0.0f, 360.0f, 96, Color{80, 85, 89, 255});
        DrawRing(center, rIn, rIn + 2.0f, 0.0f, 360.0f, 96, Color{30, 32, 34, 255});

        float rb = (rIn + rOut) / 2.0f;
        float bolt = (rOut - rIn) * 0.16f;
        for (int k = 0; k < 12; k++)
        {
            float a = (15.0f + k * 30.0f) * DEG2RAD;
            Vector2 p = {center.x + rb * cosf(a), center.y - rb * sinf(a)};
            DrawCircleV(p, bolt, Color{88, 93, 98, 255});
            DrawCircleV(Vector2{p.x - bolt * 0.3f, p.y - bolt * 0.3f}, bolt * 0.45f,
                        Color{130, 135, 140, 255});
        }
    }

    // Mechanical connector arm between the hub collar and a unit bezel
    void DrawConnectorArm(Vector2 a, Vector2 b, float width, bool active)
    {
        Vector2 d = {b.x - a.x, b.y - a.y};
        float len = sqrtf(d.x * d.x + d.y * d.y);
        if (len < 1.0f) return;
        Vector2 n = {-d.y / len, d.x / len};

        DrawLineEx(a, b, width, Color{46, 49, 52, 255});
        for (int sgn = -1; sgn <= 1; sgn += 2)
        {
            Vector2 e1 = {a.x + n.x * sgn * width / 2.0f, a.y + n.y * sgn * width / 2.0f};
            Vector2 e2 = {b.x + n.x * sgn * width / 2.0f, b.y + n.y * sgn * width / 2.0f};
            DrawLineEx(e1, e2, 2.0f, Color{76, 81, 85, 255});
        }

        // Crossbars
        int steps = (int)(len / 9.0f);
        for (int k = 1; k < steps; k++)
        {
            float t = (float)k / (float)steps;
            Vector2 p1 = {a.x + d.x * t + n.x * width * 0.42f,
                          a.y + d.y * t + n.y * width * 0.42f};
            Vector2 p2 = {a.x + d.x * t - n.x * width * 0.42f,
                          a.y + d.y * t - n.y * width * 0.42f};
            DrawLineEx(p1, p2, 1.5f, Color{60, 64, 68, 255});
        }

        // Center conduit: glows green when the unit is active
        Color glow = active ? Color{92, 230, 120, 255} : Color{58, 62, 66, 255};
        DrawLineEx(a, b, width * 0.18f, glow);
        if (active)
        {
            DrawLineEx(a, b, width * 0.40f, Fade(glow, 0.20f));
        }
    }

    // Socket where an arm docks to the collar: LED shows the unit status
    void DrawSocket(Vector2 p, float r, bool active)
    {
        DrawCircleV(p, r, Color{40, 43, 46, 255});
        DrawRing(p, r * 0.8f, r, 0.0f, 360.0f, 32, Color{68, 72, 76, 255});
        if (active)
        {
            DrawLed(p, r * 0.55f, Color{92, 230, 120, 255});
        }
        else
        {
            DrawCircleV(p, r * 0.5f, Color{30, 33, 35, 255});
            DrawCircleLines((int)p.x, (int)p.y, r * 0.5f, Color{60, 64, 68, 255});
        }
    }

    // Outer ring road with warm running lights
    void DrawRingRoad(Vector2 center, float radius)
    {
        DrawRing(center, radius - 5.0f, radius + 5.0f, 0.0f, 360.0f, 180, Color{48, 51, 54, 255});
        DrawRing(center, radius + 4.0f, radius + 6.0f, 0.0f, 360.0f, 180, Color{78, 83, 87, 255});
        DrawRing(center, radius - 6.0f, radius - 4.0f, 0.0f, 360.0f, 180, Color{78, 83, 87, 255});

        for (int k = 0; k < 12; k++)
        {
            float a = (15.0f + k * 30.0f) * DEG2RAD;
            Vector2 p = {center.x + radius * cosf(a), center.y - radius * sinf(a)};
            DrawCircleV(p, 5.0f, Fade(Color{255, 200, 120, 255}, 0.25f));
            DrawCircleV(p, 2.2f, Fade(Color{255, 214, 150, 255}, 0.95f));
        }
    }

    // Vertical entry rail with crossties and a gate box
    void DrawEntryRail(float x, float yTop, float yBottom)
    {
        for (float y = yTop; y < yBottom; y += 12.0f)
        {
            DrawLineEx(Vector2{x - 9.0f, y}, Vector2{x + 9.0f, y}, 3.0f, Color{33, 35, 37, 255});
        }
        for (int sgn = -1; sgn <= 1; sgn += 2)
        {
            float rx = x + sgn * 5.0f;
            DrawLineEx(Vector2{rx, yTop}, Vector2{rx, yBottom}, 3.5f, Color{46, 49, 52, 255});
            DrawLineEx(Vector2{rx, yTop}, Vector2{rx, yBottom}, 1.2f, Color{78, 83, 87, 255});
        }

        // Gate box (past the bottom station so it stays visible)
        float gy = yTop + 42.0f;
        DrawRectangleRounded(Rectangle{x - 10.0f, gy, 20.0f, 26.0f}, 0.3f, 4,
                             Color{50, 54, 58, 255});
        DrawRectangleRounded(Rectangle{x - 7.0f, gy + 4.0f, 14.0f, 8.0f}, 0.4f, 4,
                             Color{34, 37, 39, 255});
        DrawCircleV(Vector2{x - 6.0f, gy + 21.0f}, 1.8f, Fade(Color{255, 214, 150, 255}, 0.95f));
        DrawCircleV(Vector2{x + 6.0f, gy + 21.0f}, 1.8f, Fade(Color{255, 214, 150, 255}, 0.95f));
    }

    // A unit station: riveted bezel + tinted hex-glass dome + glyph + label
    void DrawUnitDomeStation(Vector2 center, float radius, const std::string& type, bool active)
    {
        // Drop shadow onto the terrain
        DrawCircleV(Vector2{center.x + radius * 0.08f, center.y + radius * 0.14f},
                    radius * 1.18f, Fade(BLACK, 0.40f));

        DrawBezel(center, radius * 1.02f, radius * 1.18f);

        // Active domes glow in the unit's accent tint; idle domes go dark slate
        Color base = active ? MixColor(UnitAccentColor(type), Color{20, 24, 26, 255}, 0.35f)
                            : Color{44, 52, 64, 255};
        DrawDomeSphere(center, radius, base);

        // Unit glyph + label on the dome glass
        Color glyphCol = active ? Color{240, 245, 248, 255} : Color{140, 150, 160, 255};
        DrawUnitGlyph(type, Vector2{center.x, center.y - radius * 0.18f}, radius * 0.34f, glyphCol);

        const char* label = type.c_str();
        int fontSize = (int)(radius * 0.24f);
        if (fontSize < 10) fontSize = 10;
        while (fontSize > 8 && MeasureText(label, fontSize) > (int)(radius * 1.5f))
        {
            fontSize--;
        }
        int tw = MeasureText(label, fontSize);
        DrawText(label, (int)(center.x - tw / 2.0f) + 1, (int)(center.y + radius * 0.32f) + 1,
                 fontSize, Fade(BLACK, 0.5f));
        DrawText(label, (int)(center.x - tw / 2.0f), (int)(center.y + radius * 0.32f),
                 fontSize, glyphCol);
    }
}

void Sect::DrawInSectView(Vector2 position) {
    // Dome-station layout: hex-glass domes, connector arms, ring road, entry rails
    float h = (float)GetScreenHeight();
    Vector2 center = {position.x, position.y - h * 0.04f};
    float domeRadius = h * 0.15f;                    // Central dome
    float collarOut = domeRadius * 1.22f;            // Hub bezel outer edge
    float unitRadius = h * 0.085f;                   // Unit dome
    float orbitRadius = h * 0.325f;                  // Unit centers
    float roadRadius = h * 0.443f;                   // Outer ring road (clears unit bezels)

    // Precompute node positions (8 units, start at top, clockwise)
    std::vector<Vector2> nodePositions(units.size());
    for (size_t i = 0; i < units.size(); ++i)
    {
        float angle = (90.0f - (i * 45.0f)) * DEG2RAD;
        nodePositions[i] = Vector2{
            center.x + orbitRadius * cosf(angle),
            center.y - orbitRadius * sinf(angle)   // Y grows downward
        };
    }

    // 1. Outer ring road and the entry rails leading off-screen
    DrawRingRoad(center, roadRadius);
    float railTop = center.y + roadRadius - 8.0f;
    DrawEntryRail(center.x - unitRadius * 0.5f, railTop, h);
    DrawEntryRail(center.x + unitRadius * 0.5f, railTop, h);

    // 2. Connector arms from the hub collar to each unit bezel
    for (size_t i = 0; i < units.size(); ++i)
    {
        bool active = units[i]->GetStatus() == "active";
        Vector2 d = {nodePositions[i].x - center.x, nodePositions[i].y - center.y};
        float len = sqrtf(d.x * d.x + d.y * d.y);
        Vector2 dir = {d.x / len, d.y / len};
        Vector2 a = {center.x + dir.x * collarOut * 0.98f,
                     center.y + dir.y * collarOut * 0.98f};
        Vector2 b = {nodePositions[i].x - dir.x * unitRadius * 1.05f,
                     nodePositions[i].y - dir.y * unitRadius * 1.05f};
        DrawConnectorArm(a, b, unitRadius * 0.30f, active);
    }

    // 3. Hub bezel with a soft green halo
    DrawCircleV(center, collarOut, Color{38, 41, 44, 255});
    DrawBezel(center, domeRadius * 1.02f, collarOut);
    DrawRing(center, collarOut, collarOut * 1.03f, 0.0f, 360.0f, 96,
             Fade(Color{110, 255, 150, 255}, 0.20f));

    // 4. Sockets on the collar, LED per unit status
    for (size_t i = 0; i < units.size(); ++i)
    {
        float angle = (90.0f - (i * 45.0f)) * DEG2RAD;
        Vector2 socketPos = {
            center.x + collarOut * 1.02f * cosf(angle),
            center.y - collarOut * 1.02f * sinf(angle)
        };
        DrawSocket(socketPos, unitRadius * 0.17f, units[i]->GetStatus() == "active");
    }

    // 5. Central hex-glass dome with the development readout
    DrawDomeSphere(center, domeRadius, Color{24, 130, 66, 255});

    const char* devText = TextFormat("Development: %.1f%%", development_percentage * 100);
    int devFont = (int)(domeRadius * 0.17f);
    if (devFont < 14) devFont = 14;
    int devWidth = MeasureText(devText, devFont);
    DrawText(devText, (int)(center.x - devWidth / 2.0f) + 1,
             (int)(center.y - devFont / 2.0f) + 1, devFont, Fade(BLACK, 0.45f));
    DrawText(devText, (int)(center.x - devWidth / 2.0f),
             (int)(center.y - devFont / 2.0f), devFont, Color{225, 240, 228, 255});

    // 6. Unit dome stations
    for (size_t i = 0; i < units.size(); ++i)
    {
        // Store the position for click detection
        units[i]->SetUnitPosInSectView(nodePositions[i]);
        units[i]->SetUnitRadiusInSectView(unitRadius * 1.18f);

        DrawUnitDomeStation(nodePositions[i], unitRadius,
                            units[i]->GetUnitType(),
                            units[i]->GetStatus() == "active");
    }

    // Draw the transparent right panel
    DrawTransparentRightPanel();
}

void Sect::DrawResourceStats(Vector2 position, float coreRadius) {
    // Draw storage bars showing capacity
    const float barWidth = 120.0f;
    const float barHeight = 15.0f;
    const float barSpacing = 20.0f;
    const float startY = position.y - coreRadius * 0.6f;

    // Resources to show with storage bars
    std::vector<std::pair<const char*, ResourceType>> statsToShow = {
        {"Energy", ResourceType::ENERGY},
        {"Iron", ResourceType::Fe},
        {"Food", ResourceType::FOOD},
        {"Water", ResourceType::WATER}
    };

    for (size_t i = 0; i < statsToShow.size(); i++) {
        const char* name = statsToShow[i].first;
        ResourceType type = statsToShow[i].second;

        float stored = resourceStorage[type];
        float capacity = storageCapacity[type];
        float usage = (capacity > 0.0f) ? (stored / capacity) : 0.0f;

        float barX = position.x - barWidth / 2.0f;
        float barY = startY + i * barSpacing;

        // Draw background bar (empty)
        DrawRectangle(barX, barY, barWidth, barHeight, DARKGRAY);

        // Draw filled portion (storage usage)
        float filledWidth = barWidth * usage;
        Color fillColor = usage > 0.8f ? RED : (usage > 0.5f ? ORANGE : GREEN);
        DrawRectangle(barX, barY, filledWidth, barHeight, fillColor);

        // Draw border
        DrawRectangleLines(barX, barY, barWidth, barHeight, BLACK);

        // Draw label and values
        const char* label = TextFormat("%s: %.0f/%.0f", name, stored, capacity);
        DrawText(label, barX, barY - 15, 12, BLACK);
    }
}



void Sect::DrawTransparentRightPanel() {
    int panelWidth = 100;
    Rectangle panel = {
        (float)GetScreenWidth() - panelWidth,
        0,
        (float)panelWidth,
        (float)GetScreenHeight()
    };
    DrawRectangleRec(panel, Fade(Color{12, 15, 17, 255}, 0.72f));
    DrawLineEx(Vector2{panel.x, 0.0f}, Vector2{panel.x, panel.height}, 1.0f,
               Fade(Color{92, 230, 120, 255}, 0.45f));

    // Draw panel content (e.g., notifications, alerts)
    DrawText("UPDATES",
            GetScreenWidth() - panelWidth + 10,
            10,
            16,
            Color{180, 230, 200, 255});
}

float Sect::GetStorageUsage(ResourceType type) const {
    auto storageIt = resourceStorage.find(type);
    auto capacityIt = storageCapacity.find(type);

    if (storageIt == resourceStorage.end() || capacityIt == storageCapacity.end()) {
        return 0.0f;
    }

    if (capacityIt->second <= 0.0f) {
        return 0.0f;
    }

    return storageIt->second / capacityIt->second;
}

bool Sect::CanAcceptResource(ResourceType type, float amount) const {
    auto storageIt = resourceStorage.find(type);
    auto capacityIt = storageCapacity.find(type);

    if (storageIt == resourceStorage.end() || capacityIt == storageCapacity.end()) {
        return false;
    }

    return (storageIt->second + amount) <= capacityIt->second;
}

void Sect::PushSurplusToColony(class Colony* colony) {
    if (!colony) return;

    // Check each singular resource type for surplus
    for (auto& [type, amount] : resourceStorage) {
        float usage = GetStorageUsage(type);

        // If storage is above threshold, push surplus to colony
        if (usage > STORAGE_SURPLUS_THRESHOLD) {
            auto capacityIt = storageCapacity.find(type);
            if (capacityIt != storageCapacity.end()) {
                // Calculate surplus amount (everything above 50% capacity)
                float targetAmount = capacityIt->second * 0.5f;
                float surplus = amount - targetAmount;

                if (surplus > 0.0f) {
                    // Try to send surplus to colony
                    if (colony->ReceiveSurplus(type, surplus)) {
                        // Successfully transferred, reduce local storage
                        resourceStorage[type] -= surplus;
                    }
                }
            }
        }
    }

    // Push typed resource surplus to colony
    for (const auto& desc : GetResourceDescriptors())
    {
        if (desc.category != ResourceCategory::TYPED) continue;
        ResourceType type = desc.type;

        auto it = typedResourceStorage.find(type);
        if (it == typedResourceStorage.end()) continue;

        int count = static_cast<int>(it->second.size());
        int threshold = TYPED_RESOURCE_CAPACITY / 2;  // 50% capacity

        // Push excess items when above threshold
        while (count > threshold)
        {
            TypedResource item = it->second.back();
            if (colony->ReceiveTypedSurplus(item))
            {
                it->second.pop_back();
                count--;
            }
            else
            {
                break;  // Colony can't accept more
            }
        }
    }
}

void Sect::PullDeficitFromColony(class Colony* colony) {
    if (!colony) return;

    // Check each singular resource type for deficit
    for (auto& [type, amount] : resourceStorage) {
        if (IsDeficit(type)) {
            auto capacityIt = storageCapacity.find(type);
            if (capacityIt != storageCapacity.end()) {
                // Calculate how much we need to reach target (30%)
                float targetAmount = capacityIt->second * DEFICIT_REQUEST_AMOUNT;
                float needed = targetAmount - amount;

                if (needed > 0.0f) {
                    // Request from colony
                    float received = colony->ProvideResource(type, needed);
                    if (received > 0.0f) {
                        resourceStorage[type] += received;
                    }
                }
            }
        }
    }

    // Pull typed resources when below deficit threshold
    for (const auto& desc : GetResourceDescriptors())
    {
        if (desc.category != ResourceCategory::TYPED) continue;
        ResourceType type = desc.type;

        int count = GetTotalTypedResourceCount(type);
        int deficitThreshold = TYPED_RESOURCE_CAPACITY / 10;  // 10% capacity
        int targetCount = (TYPED_RESOURCE_CAPACITY * 3) / 10; // 30% capacity

        if (count < deficitThreshold)
        {
            // Pull items until we reach the target
            while (count < targetCount)
            {
                TypedResource item;
                if (colony->ProvideTypedResource(type, item))
                {
                    AddTypedResource(item);
                    count++;
                }
                else
                {
                    break;  // Colony has no more
                }
            }
        }
    }
}

bool Sect::IsDeficit(ResourceType type) const {
    return GetStorageUsage(type) < STORAGE_DEFICIT_THRESHOLD;
}

bool Sect::IsSurplus(ResourceType type) const {
    return GetStorageUsage(type) > STORAGE_SURPLUS_THRESHOLD;
}

float Sect::GetResourceStorage(ResourceType type) const {
    auto it = resourceStorage.find(type);
    return (it != resourceStorage.end()) ? it->second : 0.0f;
}

float Sect::GetStorageCapacity(ResourceType type) const {
    auto it = storageCapacity.find(type);
    return (it != storageCapacity.end()) ? it->second : 0.0f;
}

void Sect::AddResource(ResourceType type, float amount) {
    auto storageIt = resourceStorage.find(type);
    auto capacityIt = storageCapacity.find(type);

    if (storageIt != resourceStorage.end() && capacityIt != storageCapacity.end()) {
        float newAmount = storageIt->second + amount;
        resourceStorage[type] = std::min(newAmount, capacityIt->second);
    }
}

void Sect::ConsumeResource(ResourceType type, float amount) {
    auto it = resourceStorage.find(type);
    if (it != resourceStorage.end()) {
        resourceStorage[type] = std::max(0.0f, it->second - amount);
    }
}

void Sect::LoadTextures() {
    // Load dome texture for the central sect core
    domeTexture = LoadTexture("src/assets/Unit_Thumbnails/Dome_off.png");
    if (domeTexture.id == 0) {
        std::cout << "Warning: Failed to load Dome_off.png, will use fallback rendering" << std::endl;
    }

    // Map unit type names to their texture file paths
    std::map<std::string, std::string> textureFiles = {
        {"Extraction", "src/assets/Unit_Thumbnails/extractionX256.png"},
        {"Farming", "src/assets/Unit_Thumbnails/FarmX256.png"},
        {"Energy", "src/assets/Unit_Thumbnails/powerX256.png"},
        {"Manufacture", "src/assets/Unit_Thumbnails/manufacturingX256.png"},
        {"Construction", "src/assets/Unit_Thumbnails/constructionUnitX256.png"},
        {"Transport", "src/assets/Unit_Thumbnails/TransportX256.png"},
        {"Research", "src/assets/Unit_Thumbnails/Researchx256.png"},
        {"Communication", "src/assets/Unit_Thumbnails/commX256.png"}
    };

    // Load unit textures
    for (const auto& pair : textureFiles) {
        Texture2D tex = LoadTexture(pair.second.c_str());
        if (tex.id == 0) {
            std::cout << "Warning: Failed to load texture for " << pair.first
                     << " from " << pair.second << ", will use fallback rendering" << std::endl;
        } else {
            unitTextures[pair.first] = tex;
            std::cout << "Loaded texture for " << pair.first << std::endl;
        }
    }
}

void Sect::UnloadTextures() {
    // Unload dome texture
    if (domeTexture.id != 0) {
        UnloadTexture(domeTexture);
        domeTexture.id = 0;
    }

    // Unload all unit textures
    for (auto& pair : unitTextures) {
        if (pair.second.id != 0) {
            UnloadTexture(pair.second);
        }
    }
    unitTextures.clear();
}

// Typed resource methods
bool Sect::AddTypedResource(const TypedResource& resource) {
    // Validate resource category
    if (GetResourceCategory(resource.baseType) != ResourceCategory::TYPED) {
        std::cout << "Error: Cannot add non-typed resource to typed storage" << std::endl;
        return false;
    }

    // Check capacity
    auto& storage = typedResourceStorage[resource.baseType];
    if (static_cast<int>(storage.size()) >= TYPED_RESOURCE_CAPACITY) {
        std::cout << "Warning: Typed resource storage full for "
                 << ResourceTypeToString(resource.baseType) << std::endl;
        return false;
    }

    storage.push_back(resource);
    std::cout << "Added " << resource.subType << " to "
             << ResourceTypeToString(resource.baseType) << " storage" << std::endl;
    return true;
}

bool Sect::RemoveTypedResource(ResourceType type, const std::string& subtype) {
    auto it = typedResourceStorage.find(type);
    if (it == typedResourceStorage.end()) {
        return false;
    }

    auto& storage = it->second;
    for (auto resIt = storage.begin(); resIt != storage.end(); ++resIt) {
        if (resIt->subType == subtype) {
            storage.erase(resIt);
            std::cout << "Removed " << subtype << " from "
                     << ResourceTypeToString(type) << " storage" << std::endl;
            return true;
        }
    }

    return false;
}

bool Sect::HasTypedResource(ResourceType type, const std::string& subtype) const {
    auto it = typedResourceStorage.find(type);
    if (it == typedResourceStorage.end()) {
        return false;
    }

    for (const auto& res : it->second) {
        if (res.subType == subtype) {
            return true;
        }
    }
    return false;
}

int Sect::GetTypedResourceCount(ResourceType type, const std::string& subtype) const {
    auto it = typedResourceStorage.find(type);
    if (it == typedResourceStorage.end()) {
        return 0;
    }

    int count = 0;
    for (const auto& res : it->second) {
        if (res.subType == subtype) {
            count++;
        }
    }
    return count;
}

int Sect::GetTotalTypedResourceCount(ResourceType type) const {
    auto it = typedResourceStorage.find(type);
    if (it == typedResourceStorage.end()) {
        return 0;
    }
    return static_cast<int>(it->second.size());
}

void Sect::GenerateAmbientEnergy(float deltaTime, float timeOfDay) {
    // timeOfDay is 0.0-1.0 where 0.5 is noon
    // Calculate solar multiplier based on time of day (sine curve)
    float solarPhase = timeOfDay * 2.0f * PI;
    float solarMultiplier = (sinf(solarPhase - PI/2.0f) + 1.0f) / 2.0f;  // 0-1 range

    // Scale between min and peak multipliers
    float effectiveMultiplier = SOLAR_MIN_MULTIPLIER +
        (SOLAR_PEAK_MULTIPLIER - SOLAR_MIN_MULTIPLIER) * solarMultiplier;

    // Generate ambient energy
    float energyGenerated = BASE_AMBIENT_ENERGY * effectiveMultiplier * deltaTime;

    // Add to storage (respecting capacity)
    float currentEnergy = resourceStorage[ResourceType::ENERGY];
    float energyCapacity = storageCapacity[ResourceType::ENERGY];

    if (currentEnergy + energyGenerated <= energyCapacity) {
        resourceStorage[ResourceType::ENERGY] += energyGenerated;
    } else {
        resourceStorage[ResourceType::ENERGY] = energyCapacity;
    }
}

bool Sect::CanUpgradeStorage() const {
    if (storageLevel >= MAX_STORAGE_LEVEL) return false;

    int nextLevel = storageLevel + 1;
    float feCost = SECT_UPGRADE_COST_FE[nextLevel];
    float siCost = SECT_UPGRADE_COST_SI[nextLevel];
    float energyCost = SECT_UPGRADE_COST_ENERGY[nextLevel];

    auto feIt = resourceStorage.find(ResourceType::Fe);
    auto siIt = resourceStorage.find(ResourceType::Si);
    auto enIt = resourceStorage.find(ResourceType::ENERGY);

    float feAvail = (feIt != resourceStorage.end()) ? feIt->second : 0.0f;
    float siAvail = (siIt != resourceStorage.end()) ? siIt->second : 0.0f;
    float enAvail = (enIt != resourceStorage.end()) ? enIt->second : 0.0f;

    return feAvail >= feCost && siAvail >= siCost && enAvail >= energyCost;
}

void Sect::UpgradeStorage() {
    if (!CanUpgradeStorage()) return;

    int nextLevel = storageLevel + 1;

    // Deduct costs
    resourceStorage[ResourceType::Fe] -= SECT_UPGRADE_COST_FE[nextLevel];
    resourceStorage[ResourceType::Si] -= SECT_UPGRADE_COST_SI[nextLevel];
    resourceStorage[ResourceType::ENERGY] -= SECT_UPGRADE_COST_ENERGY[nextLevel];

    storageLevel = nextLevel;

    // Update all capacities with new multiplier
    float multiplier = STORAGE_LEVEL_MULTIPLIERS[storageLevel];
    for (auto& [type, cap] : storageCapacity)
    {
        cap = SECT_BASE_STORAGE * multiplier;
    }

    std::cout << "Sect storage upgraded to level " << storageLevel
              << " (capacity: " << SECT_BASE_STORAGE * multiplier << ")" << std::endl;
}
