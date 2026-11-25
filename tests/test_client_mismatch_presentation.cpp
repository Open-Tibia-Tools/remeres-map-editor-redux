#include "ui/dialogs/client_mismatch_presentation.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("FormatMismatchVersionMeta formats OTBM and OTB major.minor inline", "[client_mismatch_presentation]")
{
    REQUIRE(FormatOtbmDisplay(0) == "OTBM 1");
    REQUIRE(FormatOtbmDisplay(1) == "OTBM 2");
    REQUIRE(FormatOtbmDisplay(2) == "OTBM 3");
    REQUIRE(FormatOtbmDisplay(3) == "OTBM 4");
    REQUIRE(FormatOtbmDisplay(4) == "OTBM 5");
    REQUIRE(FormatOtbmDisplay(-1) == "OTBM ?");
    REQUIRE(FormatOtbmDisplay(9) == "OTBM ?");

    REQUIRE(FormatOtbDisplay(2, 7) == "OTB 2.7");
    REQUIRE(FormatOtbDisplay(3, 57) == "OTB 3.57");
    REQUIRE(FormatMismatchVersionMeta(1, 2, 7) == "OTBM 2 \xC2\xB7 OTB 2.7");
    REQUIRE(FormatMismatchVersionMeta(2, 3, 57) == "OTBM 3 \xC2\xB7 OTB 3.57");
}

TEST_CASE(
    "BuildClientMismatchPresentation formats Ready state warnings and tooltips by open map count",
    "[client_mismatch_presentation]"
)
{
    const ClientMismatchVersionSummary loaded {.name = "10.98", .otbm_id = 2, .otb_major = 3, .otb_minor = 57};
    const ClientMismatchVersionSummary map {.name = "8.00", .otbm_id = 1, .otb_major = 2, .otb_minor = 7};

    const auto zero_tabs = BuildClientMismatchPresentation(loaded, map, "realera.otbm", 0, SwitchPreconditionStatus::Ready);
    REQUIRE(zero_tabs.loaded_header == "LOADED CLIENT");
    REQUIRE(zero_tabs.loaded_name == "10.98");
    REQUIRE(zero_tabs.loaded_meta == "OTBM 3 \xC2\xB7 OTB 3.57");
    REQUIRE(zero_tabs.map_header == "realera.otbm");
    REQUIRE(zero_tabs.map_name == "8.00");
    REQUIRE(zero_tabs.map_meta == "OTBM 2 \xC2\xB7 OTB 2.7");
    REQUIRE_FALSE(zero_tabs.show_tabs_warning);
    REQUIRE(zero_tabs.tabs_warning_text.empty());
    REQUIRE_FALSE(zero_tabs.show_switch_unavailable_note);
    REQUIRE(zero_tabs.switch_enabled);
    REQUIRE(zero_tabs.switch_tooltip == "Switch active client version to 8.00.");
    REQUIRE(zero_tabs.force_is_default);

    const auto with_tabs = BuildClientMismatchPresentation(loaded, map, "", 2, SwitchPreconditionStatus::Ready);
    REQUIRE(with_tabs.map_header == "Opening Map");
    REQUIRE(with_tabs.show_tabs_warning);
    REQUIRE(with_tabs.tabs_warning_text == "Switching clients will close all map tabs.");
    REQUIRE_FALSE(with_tabs.show_switch_unavailable_note);
    REQUIRE(with_tabs.switch_enabled);
    REQUIRE(with_tabs.switch_tooltip == "Switch active client version to 8.00 (closes all open map tabs).");
    REQUIRE(with_tabs.force_is_default);
}

TEST_CASE(
    "BuildClientMismatchPresentation surfaces switch unavailability reasons when preconditions fail",
    "[client_mismatch_presentation]"
)
{
    const ClientMismatchVersionSummary loaded {.name = "10.98", .otbm_id = 2, .otb_major = 3, .otb_minor = 57};
    const ClientMismatchVersionSummary map {.name = "8.00", .otbm_id = 1, .otb_major = 2, .otb_minor = 7};

    const auto unconfigured
        = BuildClientMismatchPresentation(loaded, map, "night.otbm", 2, SwitchPreconditionStatus::ClientPathsNotConfigured);
    REQUIRE_FALSE(unconfigured.switch_enabled);
    REQUIRE_FALSE(unconfigured.show_tabs_warning);
    REQUIRE(unconfigured.show_switch_unavailable_note);
    REQUIRE(unconfigured.switch_unavailable_text.find("8.00") != std::string::npos);
    REQUIRE(unconfigured.switch_unavailable_text.find("Preferences") != std::string::npos);
    REQUIRE(unconfigured.switch_tooltip == unconfigured.switch_unavailable_text);

    const auto missing_data
        = BuildClientMismatchPresentation(loaded, map, "night.otbm", 1, SwitchPreconditionStatus::MissingModularData);
    REQUIRE_FALSE(missing_data.switch_enabled);
    REQUIRE(missing_data.show_switch_unavailable_note);
    REQUIRE(missing_data.switch_unavailable_text.find("8.00") != std::string::npos);
    REQUIRE(missing_data.switch_tooltip == missing_data.switch_unavailable_text);

    const auto same_client
        = BuildClientMismatchPresentation(loaded, map, "night.otbm", 1, SwitchPreconditionStatus::SameClientConflict);
    REQUIRE_FALSE(same_client.switch_enabled);
    REQUIRE(same_client.show_switch_unavailable_note);
    REQUIRE(same_client.switch_unavailable_text.find("already active") != std::string::npos);
    REQUIRE(same_client.force_is_default);
}
