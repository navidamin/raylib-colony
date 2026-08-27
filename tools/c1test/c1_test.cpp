// C1 behavioural test: does the materials split actually bite?
//
// Not a compile check -- it runs the real Unit production loop against a
// real Sect's shared storage and asserts the economics the design
// claims: both branches produce, each starves independently on its own
// elements, and a tier upgrade needs BOTH outputs.
//
//   cmake --build build --target c1_test && ./build/src/c1_test

#include "raylib.h"
#include "Sect/sect.h"
#include "Unit/unit.h"
#include "ResourceManager/resource_manager.h"
#include "TimeManager/time_manager.h"
#include "game_constants.h"
#include "UnlockRegistry/unlock_registry.h"

#include <cstdio>
#include <map>

static int failures = 0;
static void Check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static Unit* FindUnit(Sect& sect, const std::string& type)
{
    for (Unit* u : sect.GetUnits())
    {
        if (u && u->GetUnitType() == type) return u;
    }
    return nullptr;
}

int main()
{
    SetTraceLogLevel(LOG_ERROR);
    ResourceManager rm(PLANET_SIZE, SECT_CORE_RADIUS * 2.0f);
    TimeManager tm;
    Vector2 pos = { 500.0f, 500.0f };
    Sect sect(pos, rm, tm);

    Unit* mfg = FindUnit(sect, "Manufacture");
    Check(mfg != nullptr, "sect has a Manufacture unit");
    if (!mfg) return 1;

    auto& store = const_cast<std::map<ResourceType, float>&>(
        mfg->GetResourceStorage());

    // --- 1. both branches run when fed ---
    store[ResourceType::Fe] = 500.0f;
    store[ResourceType::Ti] = 500.0f;
    store[ResourceType::Al] = 500.0f;
    store[ResourceType::Ca] = 500.0f;
    store[ResourceType::ENERGY] = 5000.0f;
    store[ResourceType::ALLOYS] = 0.0f;
    store[ResourceType::CONSTRUCTION_MATERIALS] = 0.0f;

    mfg->Start();
    for (int i = 0; i < 60; i++) mfg->Update(1.0f);

    float alloys = store[ResourceType::ALLOYS];
    float constr = store[ResourceType::CONSTRUCTION_MATERIALS];
    std::printf("   after 60s fed: alloys %.1f  construction %.1f\n",
                alloys, constr);
    Check(alloys > 0.0f, "metals branch produces ALLOYS");
    Check(constr > 0.0f, "construction branch produces CONSTRUCTION_MATERIALS");
    Check(store[ResourceType::Fe] < 500.0f, "metals branch consumed Fe");
    Check(store[ResourceType::Al] < 500.0f, "construction branch consumed Al");

    // --- 2. a mare region: Fe/Ti rich, Al/Ca poor ---
    store[ResourceType::Fe] = 500.0f;
    store[ResourceType::Ti] = 500.0f;
    store[ResourceType::Al] = 0.0f;      // highland rock absent
    store[ResourceType::Ca] = 0.0f;
    store[ResourceType::ENERGY] = 5000.0f;
    store[ResourceType::ALLOYS] = 0.0f;
    store[ResourceType::CONSTRUCTION_MATERIALS] = 0.0f;
    for (int i = 0; i < 60; i++) mfg->Update(1.0f);
    float mareAlloys = store[ResourceType::ALLOYS];
    float mareConstr = store[ResourceType::CONSTRUCTION_MATERIALS];
    std::printf("   mare  (no Al/Ca): alloys %.1f  construction %.1f\n",
                mareAlloys, mareConstr);
    Check(mareAlloys > 0.0f, "mare region still makes alloys");
    Check(mareConstr < constr * 0.75f,
          "mare region is starved of construction stock");

    // --- 3. a highland region: Al/Ca rich, Fe/Ti poor ---
    store[ResourceType::Fe] = 0.0f;
    store[ResourceType::Ti] = 0.0f;
    store[ResourceType::Al] = 500.0f;
    store[ResourceType::Ca] = 500.0f;
    store[ResourceType::ENERGY] = 5000.0f;
    store[ResourceType::ALLOYS] = 0.0f;
    store[ResourceType::CONSTRUCTION_MATERIALS] = 0.0f;
    for (int i = 0; i < 60; i++) mfg->Update(1.0f);
    float hiAlloys = store[ResourceType::ALLOYS];
    float hiConstr = store[ResourceType::CONSTRUCTION_MATERIALS];
    std::printf("   highland (no Fe/Ti): alloys %.1f  construction %.1f\n",
                hiAlloys, hiConstr);
    Check(hiConstr > 0.0f, "highland region still makes construction stock");
    Check(hiAlloys < alloys * 0.75f, "highland region is starved of alloys");

    // --- 4. the trade-off is symmetric, not a quality ranking ---
    Check(mareAlloys > hiAlloys && hiConstr > mareConstr,
          "each region leads on its own branch");

    // --- 5. a tier upgrade needs BOTH branches ---
    // Unlock every tech first, so this section tests the COST gate and
    // not the tech gate -- otherwise a refusal proves nothing.
    for (const char* tech : {"Spectroscopy", "MechanizedDrilling",
                             "MagneticSeparation", "ShiftScheduling",
                             "BasicDirectives", "Geophysics", "SwarmAI"})
    {
        UnlockRegistry::Instance().Unlock(tech);
    }

    Unit* ext = FindUnit(sect, "Extraction");
    Check(ext != nullptr, "sect has an Extraction unit");
    if (ext)
    {
        auto& es = const_cast<std::map<ResourceType, float>&>(
            ext->GetResourceStorage());
        const auto& t1 = MODULE_TIER_UPGRADE_COSTS.at(1);
        // alloys only -> refused
        es[ResourceType::ENERGY] = 10000.0f;
        es[ResourceType::ALLOYS] = t1.at(ResourceType::ALLOYS) * 4.0f;
        es[ResourceType::CONSTRUCTION_MATERIALS] = 0.0f;
        int tierBefore = ext->GetModules()[0].tier;
        bool okAlloysOnly = ext->UpgradeModuleTier(0);
        Check(!okAlloysOnly && ext->GetModules()[0].tier == tierBefore,
              "tier upgrade refused with alloys but no construction stock");

        // construction only -> refused
        es[ResourceType::ALLOYS] = 0.0f;
        es[ResourceType::CONSTRUCTION_MATERIALS] =
            t1.at(ResourceType::CONSTRUCTION_MATERIALS) * 4.0f;
        bool okConstrOnly = ext->UpgradeModuleTier(0);
        Check(!okConstrOnly && ext->GetModules()[0].tier == tierBefore,
              "tier upgrade refused with construction stock but no alloys");

        // both -> allowed, and both are spent
        es[ResourceType::ALLOYS] = t1.at(ResourceType::ALLOYS) * 4.0f;
        bool okBoth = ext->UpgradeModuleTier(0);
        Check(okBoth && ext->GetModules()[0].tier == tierBefore + 1,
              "tier upgrade allowed with both branches");
        Check(es[ResourceType::ALLOYS] < t1.at(ResourceType::ALLOYS) * 4.0f &&
              es[ResourceType::CONSTRUCTION_MATERIALS] <
                  t1.at(ResourceType::CONSTRUCTION_MATERIALS) * 4.0f,
              "both branches were actually spent");
    }

    std::printf("\n%s (%d failures)\n", failures ? "FAILED" : "ALL PASS",
                failures);
    return failures ? 1 : 0;
}
