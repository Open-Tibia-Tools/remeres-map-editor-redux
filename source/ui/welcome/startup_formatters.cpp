#include "ui/welcome/startup_formatters.h"

#include <filesystem>

#include "app/client_version.h"
#include "ui/theme.h"
#include "util/image_manager.h"

namespace {
wxString fallbackValue(const wxString& value) {
	return value.empty() ? wxString("-") : value;
}

wxString formatUnsigned(uint32_t value) {
	return value == 0 ? wxString("-") : wxString::Format("%u", value);
}

wxString formatMapDescription(const OTBMStartupPeekResult* info) {
	if (!info || info->description.empty()) {
		return "-";
	}
	return info->description;
}

wxString formatMapClientVersion(const OTBMStartupPeekResult* info) {
	if (!info || info->has_error) {
		return "-";
	}

	ClientVersion* matched_client = ClientVersion::getByItemsVersion(info->items_major_version, info->items_minor_version);
	if (!matched_client) {
		matched_client = ClientVersion::getBestMatch(info->items_minor_version);
	}
	if (!matched_client) {
		return "-";
	}

	const wxString client_name = wxstr(matched_client->getName());
	if (!client_name.empty()) {
		return client_name;
	}

	return matched_client->getVersion() > 0 ? wxString::Format("%u", matched_client->getVersion()) : wxString("-");
}

wxString formatMapDimensions(const OTBMStartupPeekResult* info) {
	if (!info || info->width == 0 || info->height == 0) {
		return "-";
	}

	return wxString::Format("%u x %u tiles", static_cast<unsigned>(info->width), static_cast<unsigned>(info->height));
}

wxString formatOtbmVersions(ClientVersion* client) {
	if (!client) {
		return "-";
	}

	wxString value;
	for (const auto map_version : client->getMapVersionsSupported()) {
		if (!value.empty()) {
			value += ", ";
		}
		value += wxString::Format("%d", static_cast<int>(map_version) + 1);
	}
	return value.empty() ? wxString("-") : value;
}

wxString formatClientDescription(ClientVersion* client) {
	if (!client) {
		return "-";
	}

	const wxString description = wxstr(client->getDescription());
	return description.empty() ? wxString("-") : description;
}

wxString formatClientConfigType(ClientVersion* client) {
	if (!client) {
		return "-";
	}

	const wxString config_type = wxstr(client->getConfigType());
	return config_type.empty() ? wxString("-") : config_type;
}
}

wxColour StartupStatusColour(StartupCompatibilityStatus status) {
	switch (status) {
		case StartupCompatibilityStatus::Compatible:
			return Theme::Get(Theme::Role::Success);
		case StartupCompatibilityStatus::Forced:
		case StartupCompatibilityStatus::ForceRequired:
		case StartupCompatibilityStatus::MapError:
			return Theme::Get(Theme::Role::Warning);
		case StartupCompatibilityStatus::MissingSelection:
		default:
			return Theme::Get(Theme::Role::TextSubtle);
	}
}

std::vector<StartupInfoField> BuildStartupMapFields(const OTBMStartupPeekResult* info) {
	const wxString error_description = (info != nullptr && info->has_error && !info->error_message.empty()) ? info->error_message : wxString("-");
	const wxColour error_colour = (info != nullptr && info->has_error) ? Theme::Get(Theme::Role::Warning) : wxNullColour;

	if (!info || info->has_error) {
		return {
			{ "Map Name", "-", std::string(ICON_FILE), wxNullColour, false },
			{ "Client Version", "-", std::string(ICON_CIRCLE_INFO), wxNullColour, false },
			{ "Dimensions", "-", std::string(ICON_LIST), wxNullColour, true },
			{ "OTBM Version", "-", std::string(ICON_LIST), wxNullColour, false },
			{ "Items Major Version", "-", std::string(ICON_LIST), wxNullColour, false },
			{ "Items Minor Version", "-", std::string(ICON_LIST), wxNullColour, true },
			{ "House File", "-", std::string(ICON_FILE), wxNullColour, false },
			{ "Spawn File", "-", std::string(ICON_FILE), wxNullColour, false },
			{ "Description", error_description, std::string(ICON_FILE_LINES), error_colour, false },
			{ "Compression", "-", std::string(ICON_FILE_ZIPPER), wxNullColour, false },
		};
	}

	return {
		{ "Map Name", fallbackValue(info->map_name), std::string(ICON_FILE), wxNullColour, false },
		{ "Client Version", formatMapClientVersion(info), std::string(ICON_CIRCLE_INFO), wxNullColour, false },
		{ "Dimensions", formatMapDimensions(info), std::string(ICON_LIST), wxNullColour, true },
		{ "OTBM Version", info->otbm_version > 0 ? wxString::Format("%d", info->otbm_version) : wxString("-"), std::string(ICON_LIST), wxNullColour, false },
		{ "Items Major Version", formatUnsigned(info->items_major_version), std::string(ICON_LIST), wxNullColour, false },
		{ "Items Minor Version", formatUnsigned(info->items_minor_version), std::string(ICON_LIST), wxNullColour, true },
		{ "House File", fallbackValue(info->house_xml_file), std::string(ICON_FILE), wxNullColour, false },
		{ "Spawn File", fallbackValue(info->spawn_xml_file), std::string(ICON_FILE), wxNullColour, false },
		{ "Description", formatMapDescription(info), std::string(ICON_FILE_LINES), wxNullColour, false },
		{ "Compression", info->is_compressed ? "GZIP" : "None", std::string(ICON_FILE_ZIPPER), wxNullColour, false },
	};
}

std::vector<StartupInfoField> BuildStartupClientFields(ClientVersion* client) {
	if (!client) {
		return {
			{ "Client Name", "-", std::string(ICON_HARD_DRIVE), wxNullColour, false },
			{ "Client Version", "-", std::string(ICON_CIRCLE_INFO), wxNullColour, false },
			{ "Data Directory", "-", std::string(ICON_FOLDER), wxNullColour, true },
			{ "OTBM Version", "-", std::string(ICON_LIST), wxNullColour, false },
			{ "Items Major Version", "-", std::string(ICON_LIST), wxNullColour, false },
			{ "Items Minor Version", "-", std::string(ICON_LIST), wxNullColour, true },
			{ "DAT Signature", "-", std::string(ICON_FILE), wxNullColour, false },
			{ "SPR Signature", "-", std::string(ICON_FILE), wxNullColour, false },
			{ "Description", "-", std::string(ICON_FILE_LINES), wxNullColour, true },
			{ "Configuration Type", "-", std::string(ICON_GEAR), wxNullColour, false },
		};
	}

	return {
		{ "Client Name", wxstr(client->getName()), std::string(ICON_HARD_DRIVE), wxNullColour, false },
		{ "Client Version", wxString::Format("%u", client->getVersion()), std::string(ICON_CIRCLE_INFO), wxNullColour, false },
		{ "Data Directory", wxstr(client->getDataDirectory()), std::string(ICON_FOLDER), wxNullColour, true },
		{ "OTBM Version", formatOtbmVersions(client), std::string(ICON_LIST), wxNullColour, false },
		{ "Items Major Version", wxString::Format("%u", client->getOtbMajor()), std::string(ICON_LIST), wxNullColour, false },
		{ "Items Minor Version", wxString::Format("%u", client->getOtbId()), std::string(ICON_LIST), wxNullColour, true },
		{ "DAT Signature", wxString::Format("%X", client->getDatSignature()), std::string(ICON_FILE), wxNullColour, false },
		{ "SPR Signature", wxString::Format("%X", client->getSprSignature()), std::string(ICON_FILE), wxNullColour, false },
		{ "Description", formatClientDescription(client), std::string(ICON_FILE_LINES), wxNullColour, true },
		{ "Configuration Type", formatClientConfigType(client), std::string(ICON_GEAR), wxNullColour, false },
	};
}

wxString FormatStartupClientPath(const wxString& full_path) {
	if (full_path.empty()) {
		return "-";
	}

	std::filesystem::path path(nstr(full_path));
	if (path.empty()) {
		return full_path;
	}
	if (path.filename().empty()) {
		path = path.parent_path();
	}

	const std::string leaf = path.filename().string();
	const std::string parent = path.parent_path().filename().string();
	if (!parent.empty() && !leaf.empty()) {
		return wxstr(parent + std::string(1, std::filesystem::path::preferred_separator) + leaf);
	}
	if (!leaf.empty()) {
		return wxstr(leaf);
	}
	return full_path;
}
