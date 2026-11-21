#ifndef RME_MAP_LOAD_DECISION_H_
#define RME_MAP_LOAD_DECISION_H_

#include "editor/persistence/map_load_options.h"
#include <string>

enum class MapLoadResolution {
	ProceedDirectly,
	PromptMismatch,
	UnsupportedVersion
};

struct MapLoadDecisionInput {
	std::string loaded_client_id;
	std::string resolved_target_id;
	MapLoadOptions options;
};

struct MapLoadDecisionResult {
	MapLoadResolution resolution = MapLoadResolution::ProceedDirectly;
	std::string effective_client_id;
	bool requires_switch = false;
};

[[nodiscard]] inline MapLoadDecisionResult DecideMapLoadAction(const MapLoadDecisionInput& input) {
	if (input.resolved_target_id.empty()) {
		return MapLoadDecisionResult{
			.resolution = MapLoadResolution::UnsupportedVersion,
			.effective_client_id = {},
			.requires_switch = false,
		};
	}

	// If no client version is currently loaded in the editor, load the target version directly.
	if (input.loaded_client_id.empty()) {
		return MapLoadDecisionResult{
			.resolution = MapLoadResolution::ProceedDirectly,
			.effective_client_id = input.resolved_target_id,
			.requires_switch = true,
		};
	}

	// If the user already provided an explicit client version in options, respect it.
	if (input.options.hasSelectedClient()) {
		const bool requires_switch = (input.options.selected_client_id != input.loaded_client_id);
		return MapLoadDecisionResult{
			.resolution = MapLoadResolution::ProceedDirectly,
			.effective_client_id = input.options.selected_client_id,
			.requires_switch = requires_switch,
		};
	}

	// If the user explicitly requested to force client mismatch, use the active client version.
	if (input.options.force_client_mismatch) {
		return MapLoadDecisionResult{
			.resolution = MapLoadResolution::ProceedDirectly,
			.effective_client_id = input.loaded_client_id,
			.requires_switch = false,
		};
	}

	// If the map matches the active client version, proceed directly without switching.
	if (input.loaded_client_id == input.resolved_target_id) {
		return MapLoadDecisionResult{
			.resolution = MapLoadResolution::ProceedDirectly,
			.effective_client_id = input.loaded_client_id,
			.requires_switch = false,
		};
	}

	// Version mismatch: prompt the user to choose between Force Load, Switch, or Cancel.
	return MapLoadDecisionResult{
		.resolution = MapLoadResolution::PromptMismatch,
		.effective_client_id = input.resolved_target_id,
		.requires_switch = true,
	};
}

#endif // RME_MAP_LOAD_DECISION_H_
