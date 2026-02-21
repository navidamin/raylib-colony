#include "resource_manager.h"


ResourceManager::ResourceManager(int gridSize, float cellSize)
    : gridSize(gridSize), cellSize(cellSize) {
    resourceGrid.resize(gridSize, std::vector<ResourceTile>(gridSize));
    surveyGrid.resize(gridSize, std::vector<OrbitalSurveyData>(gridSize));
    layeredGrid.resize(gridSize, std::vector<LayeredResourceTile>(gridSize));
}

void ResourceManager::GenerateResourceMap() {
    std::cout << "Starting resource map generation for grid size: " << gridSize << std::endl;

    // Clear existing resources
    resourceGrid = std::vector<std::vector<ResourceTile>>(
        gridSize, std::vector<ResourceTile>(gridSize)
    );

    // Reset the grid
    for (auto& row : resourceGrid) {
        for (auto& tile : row) {
            tile.resources.clear();
            tile.isExploited = false;
        }
    }

    // Random number generation
    std::random_device rd;
    std::mt19937 gen(rd());
    // Use more conservative bounds for cluster centers
    // Ensure clusters stay within grid even with radius
    int margin = 1;  // Larger margin to prevent overflow
    std::uniform_int_distribution<> positionDist(margin, gridSize - margin - 1);
    std::uniform_real_distribution<> radiusDist(2.0f, 5.0f);  // Reduced radius

    // Generate clusters for each resource type
    for (int i = 0; i < gridSize; i++) {  // Generate multiple clusters per resource
        // Generate center coordinates as integers
        int centerX = positionDist(gen);
        int centerY = positionDist(gen);

        Vector2 center = {
            static_cast<float>(centerX),
            static_cast<float>(centerY)
        };

        float radius = radiusDist(gen);

        std::cout << "\nGenerating cluster set " << i + 1
                  << " at position (" << centerX << ", " << centerY
                  << ") with base radius " << radius << std::endl;


        GenerateResourceCluster(ResourceType::H2, center, radius, 5000.0f);
        GenerateResourceCluster(ResourceType::O2, center, radius * 0.8f, 4000.0f);
        GenerateResourceCluster(ResourceType::C, center, radius * 1.2f, 3000.0f);
        GenerateResourceCluster(ResourceType::Fe, center, radius * 0.6f, 6000.0f);
        GenerateResourceCluster(ResourceType::Si, center, radius, 2000.0f);
        GenerateResourceCluster(ResourceType::Ti, center, radius * 0.5f, 4000.0f);
        GenerateResourceCluster(ResourceType::Al, center, radius * 0.9f, 3500.0f);
        GenerateResourceCluster(ResourceType::Ca, center, radius * 0.7f, 2500.0f);
    }

    // Generate depth-layered resources from flat grid
    GenerateLayeredResources();

    // Generate orbital survey data derived from resource clusters
    GenerateOrbitalSurveyData();
}

void ResourceManager::GenerateResourceCluster(ResourceType type, Vector2 center, float radius, float maxAbundance) {
    // Strict bounds checking
    int startX = std::max(0, static_cast<int>(center.x - radius));
    int endX = std::min(gridSize - 1, static_cast<int>(center.x + radius));
    int startY = std::max(0, static_cast<int>(center.y - radius));
    int endY = std::min(gridSize - 1, static_cast<int>(center.y + radius));

    std::cout << "Cluster bounds: (" << startX << "," << startY
              << ") to (" << endX << "," << endY << ")" << std::endl;

    // Add extra validation in the drawing loop
    for (int x = startX; x <= endX; x++) {
        for (int y = startY; y <= endY; y++) {
            // Double-check bounds
            if (x < 0 || x >= gridSize || y < 0 || y >= gridSize) {
                std::cout << "ERROR: Attempted to access out-of-bounds position: ("
                          << x << "," << y << ")" << std::endl;
                continue;
            }

            Vector2 pos = {static_cast<float>(x), static_cast<float>(y)};
            float dist = Vector2Distance(center, pos);

            if (dist <= radius) {
                float abundance = maxAbundance * (1.0f - (dist / radius));
                resourceGrid[y][x].resources[type] = std::max(
                    abundance,
                    resourceGrid[y][x].resources[type]
                );
            }
        }
    }
}

void ResourceManager::EnsureBasicResources(int x, int y) {
    ResourceTile& tile = resourceGrid[y][x];
    std::map<ResourceType, float> minValues = {
        {ResourceType::H2, 100.0f},
        {ResourceType::O2, 100.0f},
        {ResourceType::C, 100.0f},
        {ResourceType::Fe, 100.0f},
        {ResourceType::Si, 100.0f},
        {ResourceType::Ti, 50.0f},
        {ResourceType::Al, 75.0f},
        {ResourceType::Ca, 50.0f}
    };

    for (const auto& [type, minValue] : minValues) {
        tile.resources[type] = std::max(tile.resources[type], minValue);
    }
}

std::vector<std::pair<ResourceType, float>> ResourceManager::GetResourcesAt(Vector2 worldPos) const{
    Vector2 gridPos = WorldToGrid(worldPos);
    return GetResourcesAtGrid(static_cast<int>(gridPos.x), static_cast<int>(gridPos.y));
}

std::vector<std::pair<ResourceType, float>> ResourceManager::GetResourcesAtGrid(int gridX, int gridY) const {
    if (gridX < 0 || gridX >= gridSize || gridY < 0 || gridY >= gridSize) {
        return {};
    }

    std::vector<std::pair<ResourceType, float>> result;
    for (const auto& [type, abundance] : resourceGrid[gridY][gridX].resources) {
        if (abundance > 0.0f) {
            result.push_back({type, abundance});
        }
    }
    return result;
}

Vector2 ResourceManager::GridToWorld(int x, int y) const {
    // Clamp grid coordinates
    x = Clamp(x, 0, gridSize - 1);
    y = Clamp(y, 0, gridSize - 1);

    return {x * cellSize, y * cellSize};
}

Vector2 ResourceManager::WorldToGrid(Vector2 worldPos) const {
    // Ensure world coordinates are within bounds
    worldPos.x = Clamp(worldPos.x, 0.0f, cellSize * (gridSize - 1));
    worldPos.y = Clamp(worldPos.y, 0.0f, cellSize * (gridSize - 1));

    // Convert to grid coordinates
    return {
        std::floor(worldPos.x / cellSize),
        std::floor(worldPos.y / cellSize)
    };
}

void ResourceManager::UpdateResourceDepletion(int x , int y, ResourceType type, float amount) {
    if (x >= 0 && x < gridSize && y >= 0 && y < gridSize) {
        resourceGrid[y][x].resources[type] =
            std::max(0.0f, resourceGrid[y][x].resources[type] - amount);
    }
    //std::cout << "Resource " << type << " was depleted " << amount << "units" << std::endl;
}

void ResourceManager::GenerateLayeredResources() {
    // Depth bias multipliers per layer per resource
    // Surface (0-10cm), Shallow (10-30cm), Mid (30-100cm), Deep (100-300cm)
    struct DepthBias {
        ResourceType type;
        float bias[4]; // SURFACE, SHALLOW, MID, DEEP
    };

    static const DepthBias depthBiases[] = {
        {ResourceType::H2, {1.5f, 1.0f, 0.5f, 0.3f}},
        {ResourceType::O2, {1.3f, 1.0f, 0.8f, 0.6f}},
        {ResourceType::C,  {1.4f, 1.0f, 0.7f, 0.5f}},
        {ResourceType::Fe, {0.6f, 1.0f, 0.9f, 1.8f}},
        {ResourceType::Si, {0.8f, 1.0f, 1.3f, 0.7f}},
        {ResourceType::Ti, {0.4f, 1.0f, 1.0f, 2.0f}},
        {ResourceType::Al, {0.8f, 1.0f, 1.2f, 0.9f}},
        {ResourceType::Ca, {0.9f, 1.0f, 1.3f, 0.8f}},
    };

    for (int y = 0; y < gridSize; y++)
    {
        for (int x = 0; x < gridSize; x++)
        {
            const auto& tile = resourceGrid[y][x];
            auto& layered = layeredGrid[y][x];

            for (const auto& db : depthBiases)
            {
                auto it = tile.resources.find(db.type);
                float baseAbundance = (it != tile.resources.end()) ? it->second : 0.0f;

                for (int layer = 0; layer < 4; layer++)
                {
                    layered.layers[layer][db.type] = baseAbundance * db.bias[layer];
                }
            }
        }
    }

    std::cout << "Layered resource data generated for " << gridSize << "x" << gridSize << " grid" << std::endl;
}

std::vector<std::pair<ResourceType, float>> ResourceManager::GetResourcesAtGridLayer(
    int gridX, int gridY, DepthLayer layer) const
{
    if (gridX < 0 || gridX >= gridSize || gridY < 0 || gridY >= gridSize)
    {
        return {};
    }

    int layerIdx = static_cast<int>(layer);
    std::vector<std::pair<ResourceType, float>> result;
    for (const auto& [type, abundance] : layeredGrid[gridY][gridX].layers[layerIdx])
    {
        if (abundance > 0.0f)
        {
            result.push_back({type, abundance});
        }
    }
    return result;
}

void ResourceManager::GenerateOrbitalSurveyData() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> noiseDist(-0.05f, 0.05f);
    std::uniform_real_distribution<float> slopeDist(0.0f, 15.0f);

    for (int y = 0; y < gridSize; y++)
    {
        for (int x = 0; x < gridSize; x++)
        {
            OrbitalSurveyData& survey = surveyGrid[y][x];
            const auto& tile = resourceGrid[y][x];

            // Derive elemental composition from resource abundances
            // Normalize to percentages (0-1 range)
            float totalAbundance = 0.0f;
            for (const auto& [type, abundance] : tile.resources)
            {
                totalAbundance += abundance;
            }
            float normFactor = (totalAbundance > 0.0f) ? (1.0f / totalAbundance) : 0.0f;

            auto GetNorm = [&](ResourceType type) -> float
            {
                auto it = tile.resources.find(type);
                if (it != tile.resources.end())
                {
                    return std::clamp(it->second * normFactor + noiseDist(gen), 0.0f, 1.0f);
                }
                return std::clamp(noiseDist(gen) + 0.02f, 0.0f, 1.0f);
            };

            survey.fePercent = GetNorm(ResourceType::Fe);
            survey.tiPercent = GetNorm(ResourceType::Ti);
            survey.siPercent = GetNorm(ResourceType::Si);
            survey.alPercent = GetNorm(ResourceType::Al);
            survey.caPercent = GetNorm(ResourceType::Ca);

            // Thorium and potassium: correlated with KREEP terrain (high Fe + high Ca)
            float kreepFactor = (survey.fePercent + survey.caPercent) * 0.5f;
            survey.thPpm = std::clamp(kreepFactor * 15.0f + noiseDist(gen) * 5.0f, 0.0f, 20.0f);
            survey.kPpm = std::clamp(kreepFactor * 1500.0f + noiseDist(gen) * 300.0f, 0.0f, 2000.0f);

            // Hydrogen signal: derived from H2 abundance, stronger near edges (polar proxy)
            float h2Abundance = 0.0f;
            auto h2It = tile.resources.find(ResourceType::H2);
            if (h2It != tile.resources.end())
            {
                h2Abundance = h2It->second;
            }
            float polarFactor = 1.0f - std::abs(static_cast<float>(y) - gridSize * 0.5f) / (gridSize * 0.5f);
            polarFactor = 1.0f - polarFactor;  // Higher at edges (poles)
            survey.hydrogenSignal = std::clamp(
                h2Abundance * normFactor * 0.5f + polarFactor * 0.4f + noiseDist(gen),
                0.0f, 1.0f
            );

            // Solar illumination: higher near equator, lower near poles
            float latFactor = 1.0f - std::abs(static_cast<float>(y) - gridSize * 0.5f) / (gridSize * 0.5f);
            survey.solarIllumination = std::clamp(
                0.3f + latFactor * 0.6f + noiseDist(gen),
                0.0f, 1.0f
            );

            // Terrain slope: mostly random with slight correlation to highlands
            float highlandFactor = (survey.siPercent + survey.alPercent) * 0.5f;
            survey.terrainSlope = std::clamp(
                slopeDist(gen) + highlandFactor * 10.0f,
                0.0f, 45.0f
            );

            // Earth visibility: depends on longitude (x position) - near-side vs far-side
            float lonFactor = 1.0f - std::abs(static_cast<float>(x) - gridSize * 0.5f) / (gridSize * 0.5f);
            survey.earthVisibility = std::clamp(
                lonFactor * 0.8f + 0.1f + noiseDist(gen),
                0.0f, 1.0f
            );
        }
    }

    std::cout << "Orbital survey data generated for " << gridSize << "x" << gridSize << " grid" << std::endl;
}

ResourceManager::OrbitalSurveyData ResourceManager::GetOrbitalSurveyAt(int gridX, int gridY) const {
    if (gridX < 0 || gridX >= gridSize || gridY < 0 || gridY >= gridSize)
    {
        return OrbitalSurveyData();
    }
    return surveyGrid[gridY][gridX];
}

SiteArchetype ResourceManager::GetSiteArchetype(int gridX, int gridY) const {
    if (gridX < 0 || gridX >= gridSize || gridY < 0 || gridY >= gridSize)
    {
        return SiteArchetype::MIXED;
    }

    const OrbitalSurveyData& survey = surveyGrid[gridY][gridX];

    // Classification thresholds
    float mareScore = (survey.fePercent + survey.tiPercent) * 0.5f;
    float highlandScore = (survey.siPercent + survey.alPercent + survey.caPercent) / 3.0f;
    float volatileScore = survey.hydrogenSignal;
    float kreepScore = survey.thPpm / 20.0f;  // Normalize to 0-1

    // Check for lava tube: low slope + high earth visibility + moderate resources
    if (survey.terrainSlope < 3.0f && survey.earthVisibility > 0.7f &&
        mareScore > 0.2f && highlandScore > 0.2f)
    {
        return SiteArchetype::LAVA_TUBE;
    }

    // Find dominant archetype
    float maxScore = 0.25f;  // Minimum threshold to classify
    SiteArchetype result = SiteArchetype::MIXED;

    if (mareScore > maxScore)
    {
        maxScore = mareScore;
        result = SiteArchetype::MARE_INDUSTRIAL;
    }
    if (highlandScore > maxScore)
    {
        maxScore = highlandScore;
        result = SiteArchetype::HIGHLAND_CONSTRUCTION;
    }
    if (volatileScore > maxScore)
    {
        maxScore = volatileScore;
        result = SiteArchetype::POLAR_VOLATILE;
    }
    if (kreepScore > maxScore)
    {
        maxScore = kreepScore;
        result = SiteArchetype::KREEP_SCIENTIFIC;
    }

    return result;
}

void ResourceManager::DrawResourceDebug(float scale) {
    for (int y = 0; y < gridSize; y++) {
        for (int x = 0; x < gridSize; x++) {
            // Add bounds checking and debug output
            if (x >= gridSize || y >= gridSize) {
                std::cout << "ERROR: Attempting to draw outside grid bounds at ("
                          << x << "," << y << ")" << std::endl;
                continue;
            }

            Vector2 pos = GridToWorld(x, y);
            const auto& tile = resourceGrid[y][x];

            // Debug print for non-empty tiles
            if (!tile.resources.empty()) {
                std::cout << "Resources at (" << x << "," << y << "): ";
                for (const auto& [type, abundance] : tile.resources) {
                    std::cout << static_cast<int>(type) << ":" << abundance << " ";
                }
                std::cout << std::endl;
            }

            // Draw the most abundant resource in this tile
            if (!tile.resources.empty()) {
                auto maxResource = std::max_element(
                    tile.resources.begin(),
                    tile.resources.end(),
                    [](const auto& a, const auto& b) { return a.second < b.second; }
                );

                Color color = ResourceUtils::GetResourceColor(maxResource->first);
                color.a = static_cast<unsigned char>(255 * maxResource->second);

                DrawRectangle(
                    pos.x, pos.y,
                    cellSize, cellSize,
                    color
                );
            }
        }
    }
}
