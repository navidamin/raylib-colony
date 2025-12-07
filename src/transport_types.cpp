#include "transport_types.h"
#include "sect.h"


Road::Road(Sect* a, Sect* b)
    : sectA(a), sectB(b), mode(TransportMode::AUTO_BALANCE), isConstructed(true)
{
    // Calculate length from sect positions
    if (sectA && sectB) {
        Vector2 posA = sectA->GetPosition();
        Vector2 posB = sectB->GetPosition();
        float dx = posB.x - posA.x;
        float dy = posB.y - posA.y;
        length = std::sqrt(dx*dx + dy*dy);
    }
    else {
        length = 0.0f;
    }

    // Calculate travel time based on base speed
    travelTime = (BASE_TRANSPORT_SPEED > 0.0f) ? (length / BASE_TRANSPORT_SPEED) : 0.0f;
}


Vector2 TransportJob::GetCurrentPosition() const {
    if (!road || !road->sectA || !road->sectB) {
        return {0.0f, 0.0f};
    }

    Vector2 posA = source->GetPosition();
    Vector2 posB = destination->GetPosition();

    // Linear interpolation based on progress
    return {
        posA.x + (posB.x - posA.x) * progress,
        posA.y + (posB.y - posA.y) * progress
    };
}
