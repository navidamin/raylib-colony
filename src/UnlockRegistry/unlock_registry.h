#ifndef UNLOCK_REGISTRY_H
#define UNLOCK_REGISTRY_H

#include <set>
#include <string>
#include <vector>
#include <iostream>

class UnlockRegistry {
public:
    static UnlockRegistry& Instance()
    {
        static UnlockRegistry instance;
        return instance;
    }

    void Unlock(const std::string& tech)
    {
        unlockedTechs.insert(tech);
        std::cout << "UNLOCKED: " << tech << std::endl;
    }

    bool IsUnlocked(const std::string& tech) const
    {
        return unlockedTechs.count(tech) > 0;
    }

    std::vector<std::string> GetAll() const
    {
        return std::vector<std::string>(unlockedTechs.begin(), unlockedTechs.end());
    }

    void PrintStatus() const
    {
        std::cout << "\n=== UNLOCK REGISTRY ===" << std::endl;
        if (unlockedTechs.empty())
        {
            std::cout << "  No technologies unlocked." << std::endl;
        }
        else
        {
            for (const auto& tech : unlockedTechs)
            {
                std::cout << "  [x] " << tech << std::endl;
            }
        }
        std::cout << "======================\n" << std::endl;
    }

    // Available tech names for reference
    static const std::vector<std::string>& GetAvailableTechs()
    {
        static const std::vector<std::string> techs = {
            "Spectroscopy",
            "Geophysics",
            "SwarmAI",
            "MechanizedDrilling",
            "HeavyEquipment",
            "AutonomousFleet",
            "MagneticSeparation",
            "ProcessingChain",
            "RefineryComplex",
            "ShiftScheduling",
            "AIScheduling",
            "BasicDirectives",
            "AdvancedDirectives",
            "AIGovernance"
        };
        return techs;
    }

private:
    UnlockRegistry() = default;
    UnlockRegistry(const UnlockRegistry&) = delete;
    UnlockRegistry& operator=(const UnlockRegistry&) = delete;

    std::set<std::string> unlockedTechs;
};

#endif // UNLOCK_REGISTRY_H
