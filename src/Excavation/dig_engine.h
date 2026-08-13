#pragma once

#include "excavation_types.h"

class ProspectingGrid;
class SiteView;

// Turns "a machine working a spot at a pace" into material.
//
// Pure logic: data in, DigResult out. It reads the grid and the site's worked
// state but mutates neither -- the caller applies depletion, so the engine can
// be run speculatively (which is what the AI in Phase 6 needs).
//
// Two mechanics carry the design here:
//
//   Precision    A machine below 1.0 precision digs wider than you aimed and
//                averages your spot with its neighbours. That is what makes a
//                blunt machine throw away a survey you paid for -- and what
//                makes it the RIGHT tool on ground you never surveyed, because
//                covering ground beats aiming when you cannot aim.
//
//   Selectivity  How much surrounding waste the machine leaves in the ground
//                rather than loading. It shows up in the composition handed
//                onward, not as a separate purity number -- beneficiation's
//                separation chain is already the purity system, and a dirtier
//                dig simply carries more of everything that is not the target.
class DigEngine
{
public:
    // Look up a machine by id. Always returns something valid.
    static const Machine& GetMachine(MachineId id);

    // Is this machine available at this tier, with the techs unlocked?
    static bool IsMachineAvailable(MachineId id, int tier);

    // Deepest layer this machine can work.
    static bool CanMachineWorkDepth(MachineId id, DepthLayer depth);

    // One tick of work. `pace` and `powerCap` are the player's two dials;
    // everything else comes from the world and the machine.
    //
    // `externalMultiplier` carries the modifiers that already exist in
    // ProcessExtraction -- operations efficiency, directives, module tier and
    // survey gating -- so those keep their current meaning.
    DigResult Dig(const ProspectingGrid& grid, const SiteView& site,
                  const DigSite& worked,
                  int x, int y, DepthLayer depth,
                  ResourceType target,
                  MachineId machineId, int machineCount,
                  float pace, float powerCap,
                  float externalMultiplier, float deltaTime) const;

    // The composition a machine would actually pull up at a spot, before mass
    // is applied: the neighbour blend, then waste rejection. Exposed because
    // the panel wants to show it and the AI wants to compare it.
    std::map<ResourceType, float> BlendedComposition(const ProspectingGrid& grid,
                                                     const SiteView& site,
                                                     int x, int y, DepthLayer depth,
                                                     float precision) const;
};
