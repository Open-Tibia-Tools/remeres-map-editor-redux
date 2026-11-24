#ifndef RME_MAP_LOAD_DECISION_H_
#define RME_MAP_LOAD_DECISION_H_

#include "editor/persistence/map_load_options.h"
#include <cstdint>
#include <string>

enum class MapLoadResolution {
	ProceedDirectly,
	PromptMismatch,
	UnsupportedVersion
};

struct MapLoadDecisionInput {
	std::string loaded_client_id;
	std::string resolved_target_id;
    int loaded_protocol = 0;
    uint32_t loaded_items_major = 0;
    int map_protocol = 0;
    uint32_t map_items_major = 0;
    MapLoadOptions options;
};

struct MapLoadDecisionResult {
    MapLoadResolution resolution = MapLoadResolution::ProceedDirectly;
    bool requires_switch = false;
};

// Shared OTB compatibility rule: protocol must match, and non-zero items_major must match.
[[nodiscard]] constexpr bool
IsClientCompatibleWithMap(int loaded_protocol, uint32_t loaded_items_major, int map_protocol, uint32_t map_items_major) noexcept
{
    if (loaded_protocol != map_protocol) {
        return false;
    }
    if (map_items_major != 0 && loaded_items_major != 0 && loaded_items_major != map_items_major) {
        return false;
    }
    return true;
}

[[nodiscard]] inline bool MatchesLoadedClient(const MapLoadDecisionInput& input) noexcept
{
    if (input.loaded_protocol != 0 || input.map_protocol != 0) {
        return IsClientCompatibleWithMap(input.loaded_protocol, input.loaded_items_major, input.map_protocol, input.map_items_major);
    }
    return input.loaded_client_id == input.resolved_target_id;
}

[[nodiscard]] inline MapLoadDecisionResult DecideMapLoadAction(const MapLoadDecisionInput& input) {
	if (input.resolved_target_id.empty()) {
        return MapLoadDecisionResult {
            .resolution = MapLoadResolution::UnsupportedVersion,
            .requires_switch = false,
        };
    }

	// If no client version is currently loaded in the editor, load the target version directly.
	if (input.loaded_client_id.empty()) {
        return MapLoadDecisionResult {
            .resolution = MapLoadResolution::ProceedDirectly,
            .requires_switch = true,
        };
    }

	// If the user already provided an explicit client version in options, respect it.
	if (input.options.hasSelectedClient()) {
        return MapLoadDecisionResult {
            .resolution = MapLoadResolution::ProceedDirectly,
            .requires_switch = (input.options.selected_client_id != input.loaded_client_id),
        };
    }

    // If the user explicitly requested to force client mismatch, keep the active client version.
    if (input.options.force_client_mismatch) {
        return MapLoadDecisionResult {
            .resolution = MapLoadResolution::ProceedDirectly,
            .requires_switch = false,
        };
    }

    // If the active client is compatible with the map, proceed directly without switching.
    if (MatchesLoadedClient(input)) {
        return MapLoadDecisionResult {
            .resolution = MapLoadResolution::ProceedDirectly,
            .requires_switch = false,
        };
    }

    // Version mismatch: prompt the user to choose between Switch, Force Load, or Cancel.
    return MapLoadDecisionResult {
        .resolution = MapLoadResolution::PromptMismatch,
        .requires_switch = true,
    };
}

#endif // RME_MAP_LOAD_DECISION_H_
