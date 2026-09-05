#include <catch2/catch_test_macros.hpp>
#include "sect.h"
#include "unit.h"
#include "resource_manager.h"
#include "time_manager.h"
#include "excavation_system.h"

// Who updates a unit.
//
// GameManager::Update used to call sect->Update(dt) AND then loop the same
// sect's GetUnits() calling unit->Update(dt) again -- so every active unit ran
// a full tick twice per frame: dug twice, consumed twice, produced twice.
// Sect::Update already walks its units; the second loop was pure duplication.
//
// This pins the fact that made the removal safe: a sect updates its own units,
// so nobody holding a sect needs to reach past it.

TEST_CASE("a sect updates its own units", "[updateownership]")
{
    ResourceManager rm(20, 100.0f);
    rm.GenerateResourceMap(4242u);
    TimeManager tm;
    Vector2 pos = { 1000.0f, 1000.0f };
    Sect sect(pos, rm, tm);

    Unit* extraction = nullptr;
    for (Unit* u : sect.GetUnits())
    {
        if (u && u->GetUnitType() == "Extraction") extraction = u;
    }
    REQUIRE(extraction != nullptr);

    ExcavationSystem* es = extraction->GetExcavationSystem();
    REQUIRE(es != nullptr);
    REQUIRE(es->GetLastResult().dtSeconds == 0.0f);   // nothing has run yet

    // One call on the SECT, nothing touching the unit directly.
    sect.Update(1.0f);

    // The unit ran: the dig engine was reached through the sect alone.
    REQUIRE(es->GetLastResult().dtSeconds > 0.0f);
}
