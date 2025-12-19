#include "colony.h"
#include <iostream>

Colony::Colony() : jurisdiction_radius(SECT_CORE_RADIUS*4),
    research_level(0)
{
    // Initialize strategic reserves to 0
    strategicReserves[ResourceType::H2] = 0.0f;
    strategicReserves[ResourceType::O2] = 0.0f;
    strategicReserves[ResourceType::C] = 0.0f;
    strategicReserves[ResourceType::Fe] = 0.0f;
    strategicReserves[ResourceType::Si] = 0.0f;
    strategicReserves[ResourceType::ENERGY] = 0.0f;
    strategicReserves[ResourceType::WATER] = 0.0f;
    strategicReserves[ResourceType::FOOD] = 0.0f;
    strategicReserves[ResourceType::SCIENCE] = 0.0f;
    strategicReserves[ResourceType::MANPOWER] = 0.0f;

    // Initialize reserve capacities
    reserveCapacity[ResourceType::H2] = COLONY_BASE_RESERVES;
    reserveCapacity[ResourceType::O2] = COLONY_BASE_RESERVES;
    reserveCapacity[ResourceType::C] = COLONY_BASE_RESERVES;
    reserveCapacity[ResourceType::Fe] = COLONY_BASE_RESERVES;
    reserveCapacity[ResourceType::Si] = COLONY_BASE_RESERVES;
    reserveCapacity[ResourceType::ENERGY] = COLONY_BASE_RESERVES;
    reserveCapacity[ResourceType::WATER] = COLONY_BASE_RESERVES;
    reserveCapacity[ResourceType::FOOD] = COLONY_BASE_RESERVES;
    reserveCapacity[ResourceType::SCIENCE] = COLONY_BASE_RESERVES;
    reserveCapacity[ResourceType::MANPOWER] = COLONY_BASE_RESERVES;

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
    roads.emplace_back(sect_a, sect_b);
    std::cout << "New road built between sects. Length: " << roads.back().length
              << ", Travel time: " << roads.back().travelTime << "s" << std::endl;
}

void Colony::ManageResources() {
    // First: sects push surplus to colony reserves
    for (auto* sect : sects) {
        if (sect) {
            sect->PushSurplusToColony(this);
        }
    }

    // Second: sects pull deficit from colony reserves
    for (auto* sect : sects) {
        if (sect) {
            sect->PullDeficitFromColony(this);
        }
    }
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

    // Draw each sect inside the colony
    for (const auto& sect : sects) {
        Vector2 worldPos = sect->GetPosition();  // This should already be in world coordinates
        sect->DrawInColonyView(worldPos);
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

float Colony::GetReserveUsage(ResourceType type) const {
    auto reserveIt = strategicReserves.find(type);
    auto capacityIt = reserveCapacity.find(type);

    if (reserveIt == strategicReserves.end() || capacityIt == reserveCapacity.end()) {
        return 0.0f;
    }

    if (capacityIt->second <= 0.0f) {
        return 0.0f;
    }

    return reserveIt->second / capacityIt->second;
}

bool Colony::CanAcceptResource(ResourceType type, float amount) const {
    auto reserveIt = strategicReserves.find(type);
    auto capacityIt = reserveCapacity.find(type);

    if (reserveIt == strategicReserves.end() || capacityIt == reserveCapacity.end()) {
        return false;
    }

    return (reserveIt->second + amount) <= capacityIt->second;
}

bool Colony::ReceiveSurplus(ResourceType type, float amount) {
    if (!CanAcceptResource(type, amount)) {
        std::cout << "Colony reserves full for resource " << static_cast<int>(type) << std::endl;
        return false;
    }

    strategicReserves[type] += amount;
    std::cout << "Colony received " << amount << " of resource "
             << static_cast<int>(type) << " (Total: " << strategicReserves[type] << ")" << std::endl;
    return true;
}

float Colony::ProvideResource(ResourceType type, float requestedAmount) {
    auto it = strategicReserves.find(type);
    if (it == strategicReserves.end() || it->second <= 0.0f) {
        return 0.0f;  // No reserves available
    }

    // Provide what we can (up to requested amount)
    float available = it->second;
    float provided = std::min(available, requestedAmount);

    strategicReserves[type] -= provided;
    std::cout << "Colony provided " << provided << " of "
             << ResourceTypeToString(type) << " (Remaining: " << strategicReserves[type] << ")" << std::endl;

    return provided;
}

// Typed resource methods
bool Colony::AddTypedReserve(const TypedResource& resource) {
    // Validate resource category
    if (GetResourceCategory(resource.baseType) != ResourceCategory::TYPED) {
        std::cout << "Error: Cannot add non-typed resource to typed reserves" << std::endl;
        return false;
    }

    // Check capacity
    auto& reserves = typedReserves[resource.baseType];
    if (static_cast<int>(reserves.size()) >= TYPED_RESERVE_CAPACITY) {
        std::cout << "Warning: Colony typed reserves full for "
                 << ResourceTypeToString(resource.baseType) << std::endl;
        return false;
    }

    reserves.push_back(resource);
    std::cout << "Colony received " << resource.subType << " ("
             << ResourceTypeToString(resource.baseType) << ")" << std::endl;
    return true;
}

bool Colony::RemoveTypedReserve(ResourceType type, const std::string& subtype) {
    auto it = typedReserves.find(type);
    if (it == typedReserves.end()) {
        return false;
    }

    auto& reserves = it->second;
    for (auto resIt = reserves.begin(); resIt != reserves.end(); ++resIt) {
        if (resIt->subType == subtype) {
            reserves.erase(resIt);
            std::cout << "Colony removed " << subtype << " from "
                     << ResourceTypeToString(type) << " reserves" << std::endl;
            return true;
        }
    }

    return false;
}

bool Colony::HasTypedReserve(ResourceType type, const std::string& subtype) const {
    auto it = typedReserves.find(type);
    if (it == typedReserves.end()) {
        return false;
    }

    for (const auto& res : it->second) {
        if (res.subType == subtype) {
            return true;
        }
    }
    return false;
}

int Colony::GetTypedReserveCount(ResourceType type, const std::string& subtype) const {
    auto it = typedReserves.find(type);
    if (it == typedReserves.end()) {
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

int Colony::GetTotalTypedReserveCount(ResourceType type) const {
    auto it = typedReserves.find(type);
    if (it == typedReserves.end()) {
        return 0;
    }
    return static_cast<int>(it->second.size());
}


// Transport management methods

Road* Colony::GetRoad(Sect* sectA, Sect* sectB) {
    for (auto& road : roads) {
        if ((road.sectA == sectA && road.sectB == sectB) ||
            (road.sectA == sectB && road.sectB == sectA)) {
            return &road;
        }
    }
    return nullptr;
}

void Colony::SetRoadTransportMode(Road* road, TransportMode mode) {
    if (road) {
        road->mode = mode;
        std::cout << "Road transport mode set to " << static_cast<int>(mode) << std::endl;
    }
}

void Colony::CreateTransportJob(Sect* source, Sect* dest, ResourceType type, float amount) {
    Road* road = GetRoad(source, dest);
    if (!road) {
        std::cout << "[TRANSPORT] Error: No road exists between these sects" << std::endl;
        return;
    }

    // Rate limiting check
    float currentTime = GetTime();
    if (!road->CanAcceptNewJob(currentTime)) {
        // Rate limited - silently skip (don't spam console)
        return;
    }

    // Cap amount to packet size
    float actualAmount = std::min(amount, TRANSPORT_PACKET_SIZE);

    // Check if source has enough resources
    float available = source->GetResourceStorage(type);
    if (available < actualAmount) {
        actualAmount = available;
    }

    if (actualAmount <= 0.0f) {
        return;  // Nothing to transport
    }

    // Remove resources from source
    source->ConsumeResource(type, actualAmount);

    // Create the job
    transportJobs.emplace_back(road, source, dest, type, actualAmount);
    transportJobs.back().status = TransportStatus::IN_TRANSIT;

    // Update road tracking
    road->lastTransportTime = currentTime;
    road->activePacketCount++;

    std::cout << "[TRANSPORT] Job created: " << actualAmount << " of "
              << ResourceTypeToString(type) << " | Packets on road: "
              << road->activePacketCount << "/" << MAX_PACKETS_PER_ROAD << std::endl;
}

void Colony::ProcessTransportJobs(float deltaTime) {
    // Update all in-transit jobs
    for (auto& job : transportJobs) {
        job.Update(deltaTime);
    }

    // Process completed jobs and deliver resources
    for (auto it = transportJobs.begin(); it != transportJobs.end(); ) {
        if (it->status == TransportStatus::COMPLETED) {
            // Deliver resources to destination
            if (it->destination) {
                it->destination->AddResource(it->resourceType, it->amount);
                std::cout << "[TRANSPORT] Completed: " << it->amount << " of "
                          << ResourceTypeToString(it->resourceType) << " delivered" << std::endl;
            }
            // Decrement road packet count
            if (it->road) {
                it->road->activePacketCount = std::max(0, it->road->activePacketCount - 1);
            }
            it = transportJobs.erase(it);
        }
        else if (it->status == TransportStatus::CANCELLED) {
            // Return resources to source on cancellation
            if (it->source) {
                it->source->AddResource(it->resourceType, it->amount);
            }
            // Decrement road packet count
            if (it->road) {
                it->road->activePacketCount = std::max(0, it->road->activePacketCount - 1);
            }
            it = transportJobs.erase(it);
        }
        else {
            ++it;
        }
    }

    // Process automatic transport modes
    ProcessAutoBalance();
    ProcessDeficitTriggered();
}

void Colony::ProcessAutoBalance() {
    // For roads in AUTO_BALANCE mode, balance resources between connected sects
    for (auto& road : roads) {
        if (road.mode != TransportMode::AUTO_BALANCE || !road.sectA || !road.sectB) {
            continue;
        }

        // Check each resource type
        for (int i = 0; i < static_cast<int>(ResourceType::MANPOWER); ++i) {
            ResourceType type = static_cast<ResourceType>(i);

            float storageA = road.sectA->GetResourceStorage(type);
            float storageB = road.sectB->GetResourceStorage(type);
            float capacityA = road.sectA->GetStorageCapacity(type);
            float capacityB = road.sectB->GetStorageCapacity(type);

            if (capacityA <= 0.0f || capacityB <= 0.0f) continue;

            float ratioA = storageA / capacityA;
            float ratioB = storageB / capacityB;
            float difference = ratioA - ratioB;

            // Only balance if difference exceeds threshold
            if (std::abs(difference) > AUTO_BALANCE_THRESHOLD) {
                Sect* source = (difference > 0) ? road.sectA : road.sectB;
                Sect* dest = (difference > 0) ? road.sectB : road.sectA;

                // Calculate transfer amount to balance (simplified)
                float avgRatio = (ratioA + ratioB) / 2.0f;
                float targetA = capacityA * avgRatio;
                float transferAmount = (storageA - targetA);

                if (transferAmount > 0 && source == road.sectA) {
                    CreateTransportJob(source, dest, type, std::min(transferAmount, TRANSPORT_PACKET_SIZE));
                }
                else if (transferAmount < 0 && source == road.sectB) {
                    CreateTransportJob(source, dest, type, std::min(-transferAmount, TRANSPORT_PACKET_SIZE));
                }
            }
        }
    }
}

void Colony::ProcessDeficitTriggered() {
    // For roads in DEFICIT_TRIGGERED mode, transport when destination is in deficit
    for (auto& road : roads) {
        if (road.mode != TransportMode::DEFICIT_TRIGGERED || !road.sectA || !road.sectB) {
            continue;
        }

        // Check each resource type for deficits
        for (int i = 0; i < static_cast<int>(ResourceType::MANPOWER); ++i) {
            ResourceType type = static_cast<ResourceType>(i);

            // Check if sectA is in deficit and sectB has surplus (or vice versa)
            bool deficitA = road.sectA->IsDeficit(type);
            bool deficitB = road.sectB->IsDeficit(type);
            bool surplusA = road.sectA->IsSurplus(type);
            bool surplusB = road.sectB->IsSurplus(type);

            if (deficitA && surplusB) {
                float needed = road.sectA->GetStorageCapacity(type) * DEFICIT_REQUEST_AMOUNT -
                              road.sectA->GetResourceStorage(type);
                if (needed > 0) {
                    CreateTransportJob(road.sectB, road.sectA, type, std::min(needed, TRANSPORT_PACKET_SIZE));
                }
            }
            else if (deficitB && surplusA) {
                float needed = road.sectB->GetStorageCapacity(type) * DEFICIT_REQUEST_AMOUNT -
                              road.sectB->GetResourceStorage(type);
                if (needed > 0) {
                    CreateTransportJob(road.sectA, road.sectB, type, std::min(needed, TRANSPORT_PACKET_SIZE));
                }
            }
        }
    }
}

