//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_AI_CONTROL_SERVICE_H
#define RME_AI_CONTROL_SERVICE_H

#include "ai/ai_control_contract.h"
#include "util/json.h"

namespace AiControl {

	struct Response {
		int status = 200;
		nlohmann::json body;
	};

	// Runs one routed, gated request on the UI thread. Handlers receive the open Editor explicitly;
	// map changes go through DrawOperations or ActionQueue batches only, so each is undoable and
	// isolated from the user's own history entries.
	[[nodiscard]] Response Dispatch(const RouteSpec& route, const nlohmann::json& body);

	// True while the user is in the middle of a mouse gesture (drawing, dragging, box-selecting,
	// panning, any mouse capture) or holds a paste, so the busy gate can hold mutations back.
	[[nodiscard]] bool IsUserInteracting();

} // namespace AiControl

#endif
