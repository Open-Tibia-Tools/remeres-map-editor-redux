#include "editor/persistence/switch_client_preflight.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("EvaluateSwitchPrecondition prioritizes conflicts cleanly", "[switch_client_preflight]")
{
    SECTION("Same client always reports SameClientConflict regardless of other flags")
    {
        REQUIRE(
            EvaluateSwitchPrecondition({.is_same_client = true, .has_modular_data = false, .has_configured_client_paths = false})
            == SwitchPreconditionStatus::SameClientConflict
        );
        REQUIRE(
            EvaluateSwitchPrecondition({.is_same_client = true, .has_modular_data = true, .has_configured_client_paths = true})
            == SwitchPreconditionStatus::SameClientConflict
        );
    }

    SECTION("Missing modular data takes precedence over unconfigured client paths")
    {
        REQUIRE(
            EvaluateSwitchPrecondition({.is_same_client = false, .has_modular_data = false, .has_configured_client_paths = false})
            == SwitchPreconditionStatus::MissingModularData
        );
        REQUIRE(
            EvaluateSwitchPrecondition({.is_same_client = false, .has_modular_data = false, .has_configured_client_paths = true})
            == SwitchPreconditionStatus::MissingModularData
        );
    }

    SECTION("Missing client path reports ClientPathsNotConfigured when modular data exists")
    {
        REQUIRE(
            EvaluateSwitchPrecondition({.is_same_client = false, .has_modular_data = true, .has_configured_client_paths = false})
            == SwitchPreconditionStatus::ClientPathsNotConfigured
        );
    }

    SECTION("Ready only when different client has both modular data and configured paths")
    {
        REQUIRE(
            EvaluateSwitchPrecondition({.is_same_client = false, .has_modular_data = true, .has_configured_client_paths = true})
            == SwitchPreconditionStatus::Ready
        );
    }
}
