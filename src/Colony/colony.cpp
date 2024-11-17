#include "colony.h"
#include <iostream>

Colony::Colony() : jurisdiction_radius(SECT_CORE_RADIUS*4),
    research_level(0)
{
    CalculateCentroid();
    CalculateRadius();
}

Colony::~Colony() {
    for (auto sect : sects) {
        delete sect;
    }
}

void Colony::AddSect(Sect* sect) {
    sects.push_back(sect);
    std::cout << "New sect added to the colony." << std::endl;
    CalculateCentroid();
    CalculateRadius();
}


void Colony::BuildRoad(Sect* sect_a, Sect* sect_b) {
    roads.push_back(std::make_pair(sect_a, sect_b));
    std::cout << "New road built between sects." << std::endl;
}

void Colony::ManageResources() {
    // TODO: Implement resource management logic
    std::cout << "Colony resources managed." << std::endl;
}

void Colony::UnlockResearch() {
    research_level++;
    std::cout << "Colony research level increased to " << research_level << std::endl;
    // TODO: Implement unlocking of new technologies based on research level
}



void Colony::Draw(Camera2D& camera) {
    // Translate the drawing to center the colony
//    Vector2 screenCenter = { GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f };
//    Vector2 translation = { screenCenter.x - centroid.x, screenCenter.y - centroid.y };

    float scale = camera.zoom;
    // Draw each sect inside the colony
    for (const auto& sect : sects) {
        Vector2 worldPos = sect->GetPosition();  // This should already be in world coordinates
        sect->DrawInColonyView(worldPos, scale);
        DrawText(TextFormat("R_c: %f", GetRadius()), worldPos.x-10, worldPos.y-20, 20, GRAY);

    }

    // Draw jurisdiction circle when mouse is hovering over it
    Vector2 mouseScreenPos = GetMousePosition();
    Vector2 mouseWorldPos = GetScreenToWorld2D(mouseScreenPos, camera);

    if (CheckCollisionPointCircle(mouseWorldPos, centroid, jurisdiction_radius)) {
        DrawJurisdiction();
    }


}

void Colony::DrawJurisdiction() {
    // Draw dashed circle showing jurisdiction area
    const int numSegments = 36;  // Number of segments in the circle
    const float angleStep = 2.0f * PI / numSegments;
    const float dashLength = 10.0f;  // Length of each dash in the circle

    for (int i = 0; i < numSegments; i++) {
        float startAngle = i * angleStep;
        float endAngle = startAngle + angleStep / 2;  // Draw half of each segment for dashed effect

        Vector2 start = {
            centroid.x + jurisdiction_radius * cosf(startAngle),
            centroid.y + jurisdiction_radius * sinf(startAngle)
        };
        Vector2 end = {
            centroid.x + jurisdiction_radius * cosf(endAngle),
            centroid.y + jurisdiction_radius * sinf(endAngle)
        };

        // Draw the dash in red with transparency
        DrawLineEx(start, end, 2.0f, ColorAlpha(RED, 0.5f));
    }

    // Optional: Draw a transparent fill
    DrawCircleV(centroid, jurisdiction_radius, ColorAlpha(RED, 0.1f));
}

void Colony::CalculateCentroid() {
    // If there are no sects, return zero vector
    if (sects.empty()) {
        centroid = { 0.0f, 0.0f };
    }

    // If there's only one sect, return its position
    if (sects.size() == 1) {
        centroid = sects[0]->GetPosition();
    }

    // Calculate the average of all sect positions
    float sumX = 0.0f;
    float sumY = 0.0f;

    for (const auto& sect : sects) {
        Vector2 sectPos = sect->GetPosition();
        sumX += sectPos.x;
        sumY += sectPos.y;
    }

    // Update colony position with the calculated centroid
    centroid.x = sumX / static_cast<float>(sects.size());
    centroid.y = sumY / static_cast<float>(sects.size());

}

void Colony::CalculateRadius() {
    if (sects.empty()) {
        jurisdiction_radius = 0.0f;
    }

    if (sects.size() == 1) {
        jurisdiction_radius = SECT_CORE_RADIUS * 4;
    }
    // Calculate the average of all sect positions
    float centX = GetCentroid().x;
    float centY = GetCentroid().y;


    float maxDistance = 0.0f;

    for (const auto& sect : sects) {
        Vector2 distVec = {sect->GetPosition().x - centX,
                   sect->GetPosition().y - centY};
        float distance = std::sqrt(std::pow(distVec.x,2) + std::pow(distVec.y,2));
        maxDistance = std::max(maxDistance, distance);
    }
    jurisdiction_radius = maxDistance + SECT_CORE_RADIUS * 2;
}

