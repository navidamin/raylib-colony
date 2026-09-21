#ifndef COLONY_H
#define COLONY_H

#include "raylib.h"
#include <vector>
#include <utility>
#include <map>
#include "sect.h"
#include "resource_types.h"
#include "transport_types.h"
#include "game_enums.h"
#include "game_structs.h"
#include "lunar_frame.h"

class Colony {
public:
    Colony();
    ~Colony();

    // The colony's place on the Moon: the centre of its 25 km window and
    // the origin of the local frame its sects are drawn in. Fixed when the
    // first sect is adopted (or set explicitly), so the window does not
    // drift as sects are added.
    const LunarPoint& GetCentre() const { return centre; }
    void SetCentre(const LunarPoint& point);
    bool HasCentre() const { return hasCentre; }
    LocalFrame GetFrame() const { LocalFrame f; f.centre = centre; return f; }
    // Is the point inside this colony's territory?
    bool Contains(const LunarPoint& point) const;
    // Re-derive every sect's local position from the frame.
    void RefreshLocalPositions();

    // Adopting a sect lays it into the frame (and centres the colony on it
    // if it is the first).
    void AddSect(Sect* sect);
    void BuildRoad(Sect* sect_a, Sect* sect_b);
    void ManageResources();
    void UnlockResearch();
    void CalculateCentroid();
    void CalculateRadius();

    // Archetype
    void SetArchetype(SiteArchetype type) { archetype = type; }
    SiteArchetype GetArchetype() const { return archetype; }
    float GetArchetypeBonus(ResourceType resource) const;

    // Getters. Centroid and radius are in the local frame (units of 50 m).
    Vector2 GetCentroid() const {return centroid;}
    float GetRadius() const {return jurisdiction_radius;}
    double GetRadiusKm() const { return jurisdiction_radius / LOCAL_UNITS_PER_KM; }
    const std::vector<Sect*>& GetSects() const {return sects;}
    const std::map<ResourceType, float>& GetStrategicReserves() const {return strategicReserves;}
    const std::map<ResourceType, float>& GetReserveCapacity() const {return reserveCapacity;}
    float GetReserveUsage(ResourceType type) const;

    // Typed resource getters
    const std::map<ResourceType, std::vector<TypedResource>>& GetTypedReserves() const { return typedReserves; }
    int GetTypedReserveCount(ResourceType type, const std::string& subtype) const;
    int GetTotalTypedReserveCount(ResourceType type) const;

    // Resource management (singular)
    bool ReceiveSurplus(ResourceType type, float amount);
    bool CanAcceptResource(ResourceType type, float amount) const;
    float ProvideResource(ResourceType type, float requestedAmount);  // Returns actual amount provided

    // Typed resource management
    bool AddTypedReserve(const TypedResource& resource);
    bool RemoveTypedReserve(ResourceType type, const std::string& subtype);
    bool HasTypedReserve(ResourceType type, const std::string& subtype) const;

    // Typed resource flow (surplus/deficit between sects and colony)
    bool ReceiveTypedSurplus(const TypedResource& resource);
    bool ProvideTypedResource(ResourceType type, TypedResource& outResource);

    // Reserve upgrades
    int GetReserveLevel() const { return reserveLevel; }
    bool CanUpgradeReserves() const;
    void UpgradeReserves();

    // Transport management
    Road* GetRoad(Sect* sectA, Sect* sectB);
    const std::vector<Road>& GetRoads() const { return roads; }
    const std::vector<TransportJob>& GetTransportJobs() const { return transportJobs; }
    void SetRoadTransportMode(Road* road, TransportMode mode);
    void CreateTransportJob(Sect* source, Sect* dest, ResourceType type, float amount);
    void ProcessTransportJobs(float deltaTime);
    void ProcessAutoBalance();
    void ProcessDeficitTriggered();


private:
    SiteArchetype archetype = SiteArchetype::MIXED;
    LunarPoint centre;
    bool hasCentre = false;
    std::vector<Sect*> sects;
    Vector2 centroid;
    float jurisdiction_radius;
    std::map<std::string, int> available_resources;
    std::vector<Road> roads;
    std::vector<TransportJob> transportJobs;
    int research_level;

    // Strategic resource reserves (singular resources)
    std::map<ResourceType, float> strategicReserves;
    std::map<ResourceType, float> reserveCapacity;
    int reserveLevel = 0;  // Reserve upgrade level (0-3)

    // Typed resource reserves (MACHINERY, ELECTRONICS, ALLOYS, CONSTRUCTION_MATERIALS)
    std::map<ResourceType, std::vector<TypedResource>> typedReserves;
    static const int TYPED_RESERVE_CAPACITY = 100;  // Max items per type at colony level
};

#endif // COLONY_H
