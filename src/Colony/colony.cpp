#include "colony.h"
#include <iostream>

Colony::Colony() : jurisdiction_radius(3.0f), research_level(0) {
    // Initialize other members as needed
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

void Colony::Draw(float scale) {
    // Translate the drawing to center the colony
    Vector2 screenCenter = { GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f };
    Vector2 translation = { screenCenter.x - centroid.x, screenCenter.y - centroid.y };

    // Draw each sect inside the colony
    for (const auto& sect : sects) {
        Vector2 sectPos = { sect->GetPosition().x + translation.x, sect->GetPosition().y + translation.y };
        sect->DrawInColonyView(sectPos, scale);
    }
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

