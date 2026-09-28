#include "game/material_server_ids.h"

#include "item_definitions/core/item_definition_fragments.h"
#include "item_definitions/formats/otb/otb_item_parser.h"

#include <unordered_map>
#include <wx/filename.h>

namespace {

std::unordered_map<uint16_t, uint16_t> server_to_client;

// Temporary field id -> persistent id, then that row's client id.
// Same pairs as the server client-id cutover.
uint16_t persistentFieldId(uint16_t id) {
	switch (id) {
		case 1487:
			return 1492;
		case 1488:
			return 1493;
		case 1489:
			return 1494;
		case 1490:
			return 1496;
		case 1491:
			return 1495;
		case 1497:
			return 1498;
		case 1499:
			return 2721;
		default:
			return id;
	}
}

} // namespace

uint16_t materialClientId(uint16_t server_id) {
	if (server_to_client.empty()) {
		return server_id;
	}
	const uint16_t persistent = persistentFieldId(server_id);
	const auto it = server_to_client.find(persistent);
	if (it == server_to_client.end() || it->second == 0) {
		return persistent;
	}
	return it->second;
}

void clearMaterialServerIds() {
	server_to_client.clear();
}

bool loadMaterialServerIds(const wxFileName& otb_path, std::vector<std::string>& warnings) {
	clearMaterialServerIds();
	if (!otb_path.FileExists()) {
		warnings.push_back("items.otb is missing; brush and tileset ids cannot be translated.");
		return false;
	}

	ItemDefinitionLoadInput input;
	input.otb_path = otb_path;
	ItemDefinitionFragments fragments;
	wxString error;
	std::vector<std::string> otb_warnings;
	OtbItemParser parser;
	if (!parser.parse(input, fragments, error, otb_warnings)) {
		warnings.push_back("Couldn't read items.otb for brush id translation: " + error.ToStdString());
		return false;
	}

	server_to_client.reserve(fragments.otb.size());
	for (const auto& [server_id, fragment] : fragments.otb) {
		if (server_id > 0xFFFF) {
			continue;
		}
		const uint16_t client_id = fragment.client_id == 0 ? static_cast<uint16_t>(server_id) : static_cast<uint16_t>(fragment.client_id);
		server_to_client.emplace(static_cast<uint16_t>(server_id), client_id);
	}
	return true;
}
