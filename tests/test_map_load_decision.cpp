#include "editor/persistence/map_load_decision.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("IsClientCompatibleWithMap checks protocol and non-zero items_major", "[map_load_decision]") {
	REQUIRE(IsClientCompatibleWithMap(63, 3, 63, 3));
	REQUIRE_FALSE(IsClientCompatibleWithMap(63, 3, 62, 3));
	REQUIRE_FALSE(IsClientCompatibleWithMap(62, 3, 62, 4));
	REQUIRE(IsClientCompatibleWithMap(62, 3, 62, 0));
	REQUIRE(IsClientCompatibleWithMap(62, 0, 62, 3));
}

TEST_CASE("DecideMapLoadAction handles unsupported and unloaded editor states", "[map_load_decision]") {
	const auto unsupported = DecideMapLoadAction(MapLoadDecisionInput {
		.loaded_client_id = "13.10",
		.resolved_target_id = "",
	});
	REQUIRE(unsupported.resolution == MapLoadResolution::UnsupportedVersion);
	REQUIRE_FALSE(unsupported.requires_switch);

	const auto unloaded = DecideMapLoadAction(MapLoadDecisionInput {
		.loaded_client_id = "",
		.resolved_target_id = "12.86",
		.map_protocol = 62,
		.map_items_major = 3,
	});
	REQUIRE(unloaded.resolution == MapLoadResolution::ProceedDirectly);
	REQUIRE(unloaded.requires_switch);
}

TEST_CASE("DecideMapLoadAction honors explicit client selection and force_client_mismatch", "[map_load_decision]") {
	const auto explicit_same = DecideMapLoadAction(MapLoadDecisionInput {
		.loaded_client_id = "13.10",
		.resolved_target_id = "12.86",
		.loaded_protocol = 63,
		.loaded_items_major = 3,
		.map_protocol = 62,
		.map_items_major = 3,
		.options = MapLoadOptions { .selected_client_id = "13.10", .force_client_mismatch = false },
	});
	REQUIRE(explicit_same.resolution == MapLoadResolution::ProceedDirectly);
	REQUIRE_FALSE(explicit_same.requires_switch);

	const auto explicit_different = DecideMapLoadAction(MapLoadDecisionInput {
		.loaded_client_id = "13.10",
		.resolved_target_id = "12.86",
		.loaded_protocol = 63,
		.loaded_items_major = 3,
		.map_protocol = 62,
		.map_items_major = 3,
		.options = MapLoadOptions { .selected_client_id = "12.86", .force_client_mismatch = false },
	});
	REQUIRE(explicit_different.resolution == MapLoadResolution::ProceedDirectly);
	REQUIRE(explicit_different.requires_switch);

	const auto forced = DecideMapLoadAction(MapLoadDecisionInput {
		.loaded_client_id = "13.10",
		.resolved_target_id = "12.86",
		.loaded_protocol = 63,
		.loaded_items_major = 3,
		.map_protocol = 62,
		.map_items_major = 3,
		.options = MapLoadOptions { .selected_client_id = "", .force_client_mismatch = true },
	});
	REQUIRE(forced.resolution == MapLoadResolution::ProceedDirectly);
	REQUIRE_FALSE(forced.requires_switch);
}

TEST_CASE("DecideMapLoadAction checks OTB compatibility rather than client ID alone", "[map_load_decision]") {
	const auto compatible_variant = DecideMapLoadAction(MapLoadDecisionInput {
		.loaded_client_id = "10.98-custom",
		.resolved_target_id = "10.98",
		.loaded_protocol = 57,
		.loaded_items_major = 3,
		.map_protocol = 57,
		.map_items_major = 3,
	});
	REQUIRE(compatible_variant.resolution == MapLoadResolution::ProceedDirectly);
	REQUIRE_FALSE(compatible_variant.requires_switch);

	const auto fallback_incompatible_major = DecideMapLoadAction(MapLoadDecisionInput {
		.loaded_client_id = "12.86",
		.resolved_target_id = "12.86",
		.loaded_protocol = 62,
		.loaded_items_major = 4,
		.map_protocol = 62,
		.map_items_major = 3,
	});
	REQUIRE(fallback_incompatible_major.resolution == MapLoadResolution::PromptMismatch);
	REQUIRE(fallback_incompatible_major.requires_switch);

	const auto mismatch = DecideMapLoadAction(MapLoadDecisionInput {
		.loaded_client_id = "13.10",
		.resolved_target_id = "12.86",
		.loaded_protocol = 63,
		.loaded_items_major = 3,
		.map_protocol = 62,
		.map_items_major = 3,
	});
	REQUIRE(mismatch.resolution == MapLoadResolution::PromptMismatch);
	REQUIRE(mismatch.requires_switch);
}
