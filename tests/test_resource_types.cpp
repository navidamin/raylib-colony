#include <catch2/catch_test_macros.hpp>
#include "resource_types.h"

TEST_CASE("ResourceDescriptors table is complete", "[resource_types]")
{
    const auto& descriptors = GetResourceDescriptors();

    SECTION("Has expected number of resource types")
    {
        // Should have all 18 resource types
        REQUIRE(descriptors.size() >= 18);
    }

    SECTION("Each descriptor has a non-empty name")
    {
        for (const auto& desc : descriptors)
        {
            REQUIRE(std::string(desc.name).length() > 0);
        }
    }

    SECTION("Typed resources have subtypes defined")
    {
        for (const auto& desc : descriptors)
        {
            if (desc.category == ResourceCategory::TYPED)
            {
                REQUIRE(desc.subtypes.size() > 0);
            }
        }
    }

    SECTION("Singular resources have no subtypes")
    {
        for (const auto& desc : descriptors)
        {
            if (desc.category == ResourceCategory::SINGULAR)
            {
                REQUIRE(desc.subtypes.size() == 0);
            }
        }
    }
}

TEST_CASE("ResourceTypeToString returns correct names", "[resource_types]")
{
    REQUIRE(ResourceTypeToString(ResourceType::Fe) == "Fe");
    REQUIRE(ResourceTypeToString(ResourceType::H2) == "H2");
    REQUIRE(ResourceTypeToString(ResourceType::ENERGY) == "ENERGY");
    REQUIRE(ResourceTypeToString(ResourceType::FOOD) == "FOOD");
    REQUIRE(ResourceTypeToString(ResourceType::WATER) == "WATER");
    REQUIRE(ResourceTypeToString(ResourceType::MACHINERY) == "MACHINERY");
}

TEST_CASE("GetResourceCategory classifies correctly", "[resource_types]")
{
    REQUIRE(GetResourceCategory(ResourceType::Fe) == ResourceCategory::SINGULAR);
    REQUIRE(GetResourceCategory(ResourceType::H2) == ResourceCategory::SINGULAR);
    REQUIRE(GetResourceCategory(ResourceType::ENERGY) == ResourceCategory::SINGULAR);
    REQUIRE(GetResourceCategory(ResourceType::MACHINERY) == ResourceCategory::TYPED);
    REQUIRE(GetResourceCategory(ResourceType::ELECTRONICS) == ResourceCategory::TYPED);
    REQUIRE(GetResourceCategory(ResourceType::ALLOYS) == ResourceCategory::TYPED);
}

TEST_CASE("ResourceUtils subtype validation", "[resource_types]")
{
    SECTION("Valid subtypes accepted")
    {
        REQUIRE(ResourceUtils::IsValidSubtype(ResourceType::MACHINERY, "HeavyDrill"));
        REQUIRE(ResourceUtils::IsValidSubtype(ResourceType::ELECTRONICS, "Sensor"));
        REQUIRE(ResourceUtils::IsValidSubtype(ResourceType::ALLOYS, "Steel"));
    }

    SECTION("Invalid subtypes rejected")
    {
        REQUIRE_FALSE(ResourceUtils::IsValidSubtype(ResourceType::MACHINERY, "InvalidItem"));
        REQUIRE_FALSE(ResourceUtils::IsValidSubtype(ResourceType::Fe, "anything"));
    }
}

TEST_CASE("GetResourceDescriptor lookup", "[resource_types]")
{
    const auto& desc = GetResourceDescriptor(ResourceType::Fe);
    REQUIRE(std::string(desc.name) == "Fe");
    REQUIRE(desc.category == ResourceCategory::SINGULAR);
    REQUIRE(desc.type == ResourceType::Fe);
}
