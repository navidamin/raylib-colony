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
    Core,
    Communication
};


enum class View {
    Menu,
    Planet,
    SITE_SELECTION,
    Colony,
    Sect,
    Unit,
    Orbital   // Moon-from-orbit view, sits above Planet in the zoom hierarchy
};


// Depth layers for resource profiling
enum class DepthLayer { SURFACE = 0, SHALLOW = 1, MID = 2, DEEP = 3 };

// Site archetypes for colony placement
enum class SiteArchetype {
    MARE_INDUSTRIAL,        // High Fe/Ti - industrial/manufacturing focus
    HIGHLAND_CONSTRUCTION,  // High Si/Al/Ca - construction materials focus
    POLAR_VOLATILE,         // High hydrogen signal - water/volatile extraction
    KREEP_SCIENTIFIC,       // High Th/K - science generation focus
    LAVA_TUBE,              // Protected site - general bonus
    MIXED                   // No dominant archetype
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
