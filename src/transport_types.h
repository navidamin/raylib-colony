#ifndef TRANSPORT_TYPES_H
#define TRANSPORT_TYPES_H

#include "raylib.h"
#include "game_enums.h"
#include "resource_types.h"
#include "game_constants.h"
#include <cmath>

// Forward declaration
class Sect;


// Road connecting two sects
struct Road {
    Sect* sectA;
    Sect* sectB;
    float length;                   // Calculated distance between sects
    float travelTime;               // Time to traverse (length / speed)
    TransportMode mode;             // Current transport mode for this road
    bool isConstructed;             // False during construction phase
    float lastTransportTime;        // Game time when last transport job was created
    int activePacketCount;          // Current number of packets on this road

    Road(Sect* a, Sect* b);

    // Calculate travel time based on speed modifier
    float GetTravelTime(float speedModifier = 1.0f) const {
        return travelTime / speedModifier;
    }

    // Check if road can accept a new transport job (rate limiting)
    bool CanAcceptNewJob(float currentTime) const {
        return (currentTime - lastTransportTime >= MIN_TRANSPORT_INTERVAL) &&
               (activePacketCount < MAX_PACKETS_PER_ROAD);
    }
};


// Transport job for moving resources
struct TransportJob {
    Road* road;
    Sect* source;
    Sect* destination;
    ResourceType resourceType;
    float amount;
    float progress;                 // 0.0 to 1.0 (position along road)
    TransportStatus status;

    TransportJob(Road* r, Sect* src, Sect* dest, ResourceType type, float amt)
        : road(r), source(src), destination(dest),
          resourceType(type), amount(amt), progress(0.0f),
          status(TransportStatus::PENDING) {}

    // Update progress based on delta time
    void Update(float deltaTime) {
        if (status == TransportStatus::IN_TRANSIT && road) {
            float travelTime = road->GetTravelTime();
            if (travelTime > 0.0f) {
                progress += deltaTime / travelTime;
                if (progress >= 1.0f) {
                    progress = 1.0f;
                    status = TransportStatus::COMPLETED;
                }
            }
        }
    }

    // Get current world position of the packet
    Vector2 GetCurrentPosition() const;
};


#endif // TRANSPORT_TYPES_H
