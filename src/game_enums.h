#ifndef GAME_ENUMS_H
#define GAME_ENUMS_H


enum class UnitType {
    Extraction,
    Farming,
    Energy,
    Construction,
    Transport,
    Manufacture,
    Research,
    Commerce
};


enum class View {
    Menu,
    Planet,
    Colony,
    Sect,
    Unit
};


// Transport modes (player-selectable per road)
enum class TransportMode {
    AUTO_BALANCE,       // System auto-transports to balance resources across sects
    MANUAL,             // Player manually requests specific transport jobs
    DEFICIT_TRIGGERED   // Low sect auto-requests from nearby surplus sect
};


// Transport job status
enum class TransportStatus {
    PENDING,            // Waiting to start
    IN_TRANSIT,         // Currently moving
    COMPLETED,          // Arrived at destination
    CANCELLED           // Job was cancelled
};


#endif // GAME_ENUMS_H
