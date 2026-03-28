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


void Sect::DrawInSectView(Vector2 position) {
    float coreRadius = GetScreenHeight() * 0.38f;  // Core takes ~76% of screen height diameter

    // Draw the main core (dome texture or fallback circle)
    if (domeTexture.id != 0) {
        // Draw dome texture centered on position, scaled to fit coreRadius
        float textureDiameter = coreRadius * 2.0f;
        Rectangle source = {0.0f, 0.0f, (float)domeTexture.width, (float)domeTexture.height};
        Rectangle dest = {
            position.x - coreRadius,
            position.y - coreRadius,
            textureDiameter,
            textureDiameter
        };
        Vector2 origin = {0.0f, 0.0f};
        DrawTexturePro(domeTexture, source, dest, origin, 0.0f, WHITE);
    } else {
        // Fallback to circle if texture not loaded
        DrawCircle(position.x, position.y, coreRadius, GRAY);
        DrawCircleLines(position.x, position.y, coreRadius, BLACK);
    }

    // Draw core information
    DrawText(TextFormat("Development: %.1f%%", development_percentage * 100),
            position.x - MeasureText("Development: 100.0%", 20)/2,
            position.y - 10,
            20,
            BLACK);

    // Draw resource stats in the core
    DrawResourceStats(position, coreRadius);

    // Draw the units around the core
    float unitRadius = coreRadius * 0.32f;  // Units are 32% the size of core (larger)
    float orbitRadius = coreRadius * 1.12f; // Distance from core center to unit center (closer)

    for (size_t i = 0; i < units.size(); ++i) {
        // Start from 90 degrees (top) and go clockwise
        float angle = (90.0f - (i * 45.0f)) * DEG2RAD;  // 8 units, 45 degrees apart

        Vector2 unitPos = {
            position.x + orbitRadius * cosf(angle),
            position.y - orbitRadius * sinf(angle)  // Subtract because Y grows downward
        };

        // Store the position for click detection
        units[i]->SetUnitPosInSectView(unitPos);
        units[i]->SetUnitRadiusInSectView(unitRadius);

        // Get unit type for texture lookup
        std::string unitType = units[i]->GetUnitType();
        auto texIt = unitTextures.find(unitType);

        // All units render at full brightness for now (no deactivated look)
        Color tint = WHITE;

        // Draw the unit (texture or fallback circle)
        if (texIt != unitTextures.end() && texIt->second.id != 0) {
            // Draw unit texture
            float textureDiameter = unitRadius * 2.0f;
            Rectangle source = {0.0f, 0.0f, (float)texIt->second.width, (float)texIt->second.height};
            Rectangle dest = {
                unitPos.x - unitRadius,
                unitPos.y - unitRadius,
                textureDiameter,
                textureDiameter
            };
            Vector2 origin = {0.0f, 0.0f};
            DrawTexturePro(texIt->second, source, dest, origin, 0.0f, tint);

            // Add green glow ring for active units
            if (units[i]->GetStatus() == "active") {
                DrawCircleLines(unitPos.x, unitPos.y, unitRadius * 1.1f, GREEN);
            }
        } else {
            // Fallback to circle if texture not available
            Color fillColor = units[i]->GetStatus() == "active" ? GREEN : GRAY;
            DrawCircle(unitPos.x, unitPos.y, unitRadius, fillColor);
            DrawCircleLines(unitPos.x, unitPos.y, unitRadius, BLACK);

            // Draw first letter of unit type (only for fallback)
            const char* unitTypeStr = unitType.c_str();
            char firstLetter[2] = {unitTypeStr[0], '\0'};

            // Center the letter in the circle
            int fontSize = (int)(unitRadius);
            Vector2 textSize = MeasureTextEx(GetFontDefault(), firstLetter, fontSize, 1);
            Vector2 textPos = {
                unitPos.x - textSize.x/2,
                unitPos.y - textSize.y/2
            };

            DrawText(firstLetter, textPos.x, textPos.y, fontSize, BLACK);
        }
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
    DrawRectangleRec(panel, Fade(WHITE, 0.5f));

    // Draw panel content (e.g., notifications, alerts)
    DrawText("Updates",
            GetScreenWidth() - panelWidth + 10,
            10,
            20,
            BLACK);
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
