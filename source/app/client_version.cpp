//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#include <toml++/toml.h>
#include <charconv>
#include <string>
#include <format>
#include <fstream>
#include "app/definitions.h"
#include "app/settings.h"
#include "io/filehandle.h"

#include "util/file_system.h"
#include "app/client_version.h"
#include <spdlog/spdlog.h>

// Static methods to load/save

ClientVersion::VersionList ClientVersion::client_versions;
ClientVersion* ClientVersion::latest_version = nullptr;
std::string ClientVersion::loaded_file_path;

void ClientVersion::loadVersions() {
	// Clean up old stuff
	ClientVersion::unloadVersions();

	loadVersionsFromTOML("clients.toml");

	// Assign a default if there isn't one.
	if (!latest_version && !client_versions.empty()) {
		latest_version = client_versions.front().get();
	}

	// Load user-specific paths from config.toml
	toml::table& table = g_settings.getTable();
	bool changed = false;
	if (auto clients = table["clients"].as_array()) {
		for (auto it = clients->begin(); it != clients->end();) {
			if (auto client = (*it).as_table()) {
				std::string name = (*client)["name"].value_or("");
				if (name.empty()) {
					++it;
					continue;
				}

				std::string client_path_str = (*client)["clientPath"].value_or("");
				std::filesystem::path client_path = std::filesystem::u8path(client_path_str);
				if (!client_path.empty() && !std::filesystem::is_directory(client_path)) {
					it = clients->erase(it);
					changed = true;
					continue;
				}

				ClientVersion* version = get(name);
				if (version) {
					// ONLY override the user-specific path
					version->setClientPath(client_path);

					bool isDefault = (*client)["default"].value_or(false);
					if (isDefault) {
						latest_version = version;
					}
				}
				++it;
			} else {
				++it;
			}
		}
	}

	if (changed) {
		g_settings.save();
	}
}

void ClientVersion::loadVersionsFromTOML(const std::string& configName) {
	std::filesystem::path file_to_load;

	std::filesystem::path exec_dir_client_toml = FileSystem::GetExecDirectory() / configName;
	std::filesystem::path data_dir_client_toml = FileSystem::GetDataDirectory() / configName;
	std::filesystem::path work_dir_client_toml = FileSystem::GetFoundDataDirectory() / configName;

	file_to_load = data_dir_client_toml; // Primary location
	if (!std::filesystem::exists(file_to_load)) {
		file_to_load = exec_dir_client_toml;
		if (!std::filesystem::exists(file_to_load)) {
			file_to_load = work_dir_client_toml;
			if (!std::filesystem::exists(file_to_load)) {
				file_to_load.clear();
			}
		}
	}

	if (file_to_load.empty() || !std::filesystem::exists(file_to_load)) {
		return;
	}

	loaded_file_path = file_to_load.string();

	toml::table config;
	try {
		config = toml::parse_file(file_to_load.string());
	} catch (const toml::parse_error& err) {
		spdlog::error("Parsing failed for {}: {}", configName, err.description());
		return;
	}

	if (config["clients"].as_array()) {
		auto& clients = *config["clients"].as_array();
		for (auto&& elem : clients) {
			if (!elem.is_table()) {
				continue;
			}
			toml::table& client = *elem.as_table();

			int version = client["version"].value_or(0);
			std::string name = client["name"].value_or("");

			// OTB Info
			int otbId = client["otbId"].value_or(0);
			int otbMajor = client["otbMajor"].value_or(0);

			OtbVersion otb;
			otb.name = name;
			otb.id = static_cast<OtbVersionID>(otbId);
			otb.format_version = static_cast<OtbFormatVersion>(otbMajor);

			// Data Directory
			std::string dataDirectory = client["dataDirectory"].value_or("");
			std::unique_ptr<ClientVersion> cv = std::make_unique<ClientVersion>(otb, name, dataDirectory);

			ClientVersion* cv_ptr = cv.get();
			cv_ptr->version = version;
			cv_ptr->description = client["description"].value_or("");
			cv_ptr->item_definition_mode = parseItemDefinitionMode(client["configType"].value_or("dat_otb")).value_or(ItemDefinitionMode::DatOtb);

			// OTBM Versions
			cv_ptr->preferred_map_version = MAP_OTBM_UNKNOWN;
			if (client["otbmVersions"].as_array()) {
				auto& otbmVers = *client["otbmVersions"].as_array();
				for (auto&& v : otbmVers) {
					int ver = v.value_or(-1);
					if (ver >= 1 && ver <= 4) {
						int enumVer = ver - 1;
						cv_ptr->map_versions_supported.push_back(static_cast<MapVersionID>(enumVer));
						if (cv_ptr->preferred_map_version == MAP_OTBM_UNKNOWN) {
							cv_ptr->preferred_map_version = static_cast<MapVersionID>(enumVer);
						}
					}
				}
			}

			// Data / Signatures - Robust parsing
			ClientData client_data;
			client_data.datSignature = 0;
			client_data.sprSignature = 0;

			auto parseHex = [](const toml::node_view<toml::node>& node) -> uint32_t {
				uint32_t val = 0;
				if (node.is_string()) {
					std::string s = node.as_string()->get();
					std::from_chars(s.data(), s.data() + s.size(), val, 16);
				} else if (node.is_integer()) {
					val = (uint32_t)node.as_integer()->get();
				}
				return val;
			};

			client_data.datSignature = parseHex(client["datSignature"]);
			client_data.sprSignature = parseHex(client["sprSignature"]);
			client_data.datFormat = getDatFormatForVersion(version);

			cv_ptr->metadata_file = client["metadataFile"].value_or("Tibia.dat");
			cv_ptr->sprites_file = client["spritesFile"].value_or("Tibia.spr");

			cv_ptr->is_transparent = client["transparency"].value_or(version >= 1010);
			cv_ptr->is_extended = client["extended"].value_or(version >= 860);
			cv_ptr->has_frame_durations = client["frameDurations"].value_or(version >= 1050);
			cv_ptr->has_frame_groups = client["frameGroups"].value_or(version >= 1057);

			if (!cv_ptr->data_versions.empty()) {
				cv_ptr->data_versions[0] = client_data;
			} else {
				cv_ptr->data_versions.push_back(client_data);
			}
			cv_ptr->visible = true;

			bool isDefault = client["default"].value_or(false);
			if (isDefault) {
				latest_version = cv_ptr;
			}

			client_versions.push_back(std::move(cv));
		}
	}
}

void ClientVersion::unloadVersions() {
	client_versions.clear();
	latest_version = nullptr;
}

void ClientVersion::addVersion(std::unique_ptr<ClientVersion> version) {
	client_versions.push_back(std::move(version));
}

void ClientVersion::removeVersion(const ClientVersionID& id) {
	std::erase_if(client_versions, [&](const auto& cv) {
		return cv->getName() == id;
	});
}

DatFormat ClientVersion::getDatFormatForVersion(int version) {
	// Using a map makes this more data-driven and easier to maintain.
	static const std::map<int, DatFormat, std::greater<int>> version_to_format = {
		{ 1057, DAT_FORMAT_1057 },
		{ 1050, DAT_FORMAT_1050 },
		{ 1010, DAT_FORMAT_1010 },
		{ 960, DAT_FORMAT_96 },
		{ 860, DAT_FORMAT_86 },
		{ 780, DAT_FORMAT_78 },
		{ 750, DAT_FORMAT_755 },
		{ 710, DAT_FORMAT_74 },
	};

	auto it = version_to_format.lower_bound(version);
	if (it != version_to_format.end()) {
		return it->second;
	}

	return DAT_FORMAT_UNKNOWN;
}

bool ClientVersion::isDefaultPath() const {
	std::filesystem::path clientPath = getClientPath();
	std::filesystem::path dataPath = getDataPath();

	std::error_code ec;
	return std::filesystem::equivalent(clientPath, dataPath, ec) || clientPath == dataPath;
}

bool ClientVersion::saveVersions() {
	// 1. Save technical database to data/clients.toml
	toml::table db_table;
	toml::array db_clients_array;

	// 2. Save user-specific config to config.toml (g_settings)
	toml::array config_clients_array;

	for (const auto& version : client_versions) {
		// Technical database object
		toml::table db_obj;
		db_obj.insert_or_assign("name", version->name);
		db_obj.insert_or_assign("version", (int)version->version);
		db_obj.insert_or_assign("otbId", (int)version->otb.id);
		db_obj.insert_or_assign("otbMajor", (int)version->otb.format_version);
		db_obj.insert_or_assign("description", version->description);
		db_obj.insert_or_assign("configType", toString(version->item_definition_mode));
		db_obj.insert_or_assign("metadataFile", version->metadata_file);
		db_obj.insert_or_assign("spritesFile", version->sprites_file);
		db_obj.insert_or_assign("transparency", version->is_transparent);
		db_obj.insert_or_assign("extended", version->is_extended);
		db_obj.insert_or_assign("frameDurations", version->has_frame_durations);
		db_obj.insert_or_assign("frameGroups", version->has_frame_groups);
		db_obj.insert_or_assign("dataDirectory", version->data_path);

		if (!version->data_versions.empty()) {
			db_obj.insert_or_assign("datSignature", std::format("{:X}", version->data_versions[0].datSignature));
			db_obj.insert_or_assign("sprSignature", std::format("{:X}", version->data_versions[0].sprSignature));
		}

		toml::array otbmVers;
		for (auto v : version->map_versions_supported) {
			otbmVers.push_back((int)v + 1);
		}
		db_obj.insert_or_assign("otbmVersions", std::move(otbmVers));
		// (Moved to config_obj below)
		db_clients_array.push_back(std::move(db_obj));

		// User config object (ONLY path)
		toml::table config_obj;
		config_obj.insert_or_assign("name", version->name);
		std::filesystem::path cp = version->getClientPath();
		if (!cp.empty()) {
			config_obj.insert_or_assign("clientPath", cp.string());
		} else {
			config_obj.insert_or_assign("clientPath", "");
		}
		config_obj.insert_or_assign("default", version.get() == latest_version);
		config_clients_array.push_back(std::move(config_obj));
	}

	// Save Database
	db_table.insert_or_assign("clients", std::move(db_clients_array));

	std::filesystem::path db_file;
	if (!loaded_file_path.empty()) {
		db_file = loaded_file_path;
	} else {
		db_file = FileSystem::GetDataDirectory() / "clients.toml";
	}

	std::error_code ec;
	std::filesystem::create_directories(db_file.parent_path(), ec);

	std::ofstream db_stream(db_file);
	if (!db_stream.is_open()) {
		spdlog::error("Failed to open clients.toml for writing: {}", db_file.string());
		return false;
	}
	db_stream << db_table;
	db_stream.close();

	// Save User Config
	g_settings.getTable().insert_or_assign("clients", std::move(config_clients_array));
	g_settings.save();
	return true;
}

// Client version class

ClientVersion::ClientVersion(OtbVersion otb, std::string versionName, std::string path) :
	otb(otb),
	version(0),
	name(versionName),
	visible(false),
	is_transparent(false),
	is_extended(false),
	has_frame_durations(false),
	has_frame_groups(false),
	preferred_map_version(MAP_OTBM_UNKNOWN),
	data_path(path) {
	////
	// Default transparency check (can be updated later or in load)
	data_versions.push_back(ClientData());
}

void ClientVersion::setVersion(uint32_t v) {
	version = v;
}

ClientVersion* ClientVersion::get(const ClientVersionID& id) {
	for (const auto& cv : client_versions) {
		if (cv->name == id) {
			return cv.get();
		}
	}
	return nullptr;
}

ClientVersion* ClientVersion::getBestMatch(OtbVersionID id) {
	// Try to find a default one first
	for (const auto& cv : client_versions) {
		if (cv->otb.id == id) {
			// We could check for a 'default' flag here if we wanted to be more precise
			return cv.get();
		}
	}
	return nullptr;
}

ClientVersion* ClientVersion::getByItemsVersion(uint32_t major, uint32_t minor) {
	for (const auto& client_version : client_versions) {
		ClientVersion* candidate = client_version.get();
		if (!candidate->isVisible()) {
			continue;
		}
		if (candidate->getOtbMajor() == major && candidate->getOtbId() == minor) {
			return candidate;
		}
	}

	for (const auto& client_version : client_versions) {
		ClientVersion* candidate = client_version.get();
		if (candidate->getOtbMajor() == major && candidate->getOtbId() == minor) {
			return candidate;
		}
	}

	return nullptr;
}

ClientVersionList ClientVersion::getAll() {
	ClientVersionList l;
	for (const auto& cv : client_versions) {
		l.push_back(cv.get());
	}
	return l;
}

ClientVersionList ClientVersion::getAllVisible() {
	ClientVersionList l;
	for (const auto& cv : client_versions) {
		if (cv->isVisible()) {
			l.push_back(cv.get());
		}
	}
	return l;
}

ClientVersionList ClientVersion::getConfiguredVisible() {
	ClientVersionList configured_versions;
	for (const auto& client_version : client_versions) {
		if (client_version->isVisible() && client_version->hasConfiguredClientPath()) {
			configured_versions.push_back(client_version.get());
		}
	}
	return configured_versions;
}

ClientVersionList ClientVersion::getAllForOTBMVersion(MapVersionID id) {
	ClientVersionList list;
	for (const auto& cv : client_versions) {
		if (cv->isVisible()) {
			if (std::ranges::find(cv->map_versions_supported, id) != cv->map_versions_supported.end()) {
				list.push_back(cv.get());
			}
		}
	}
	return list;
}

ClientVersion* ClientVersion::getLatestVersion() {
	return latest_version;
}

void ClientVersion::setLatestVersion(ClientVersion* version) {
	latest_version = version;
}

FileName ClientVersion::getDataPath() const {
	std::filesystem::path basePath = FileSystem::GetDataDirectory();
	std::error_code ec;
	if (!std::filesystem::is_directory(basePath, ec)) {
		basePath = FileSystem::GetFoundDataDirectory();
	}
	return basePath / data_path;
}

FileName ClientVersion::getLocalDataPath() const {
	FileName f = FileSystem::GetLocalDataDirectory() / data_path;
	std::error_code ec;
	std::filesystem::create_directories(f, ec);
	return f;
}

bool ClientVersion::hasValidPaths() {
	if (!std::filesystem::is_directory(client_path)) {
		return false;
	}

	// OTFI loading removed (deprecated)
	// Metadata and sprites paths are now set in loadVersionsFromTOML or defaulted.
	// We just verify they exist here.
	metadata_path = client_path / metadata_file;
	sprites_path = client_path / sprites_file;

	if (!std::filesystem::exists(metadata_path) || !std::filesystem::exists(sprites_path)) {
		// protobuf should stick to catalog-content.json and prodvided dat file
		if (isProtobuf()) {
            return false;
		}

		// Fallback to "Tibia.dat" / "Tibia.spr" if the configured files don't exist
		// This maintains some backward compatibility if the toml config is slightly off but files are standard
		metadata_path = client_path / (std::string(ASSETS_NAME) + ".dat");
		sprites_path = client_path / (std::string(ASSETS_NAME) + ".spr");
	}

	if (!std::filesystem::exists(metadata_path) || !std::filesystem::exists(sprites_path)) {
		return false;
	}

	// protobuf does not use signatures
	if (isProtobuf() || !g_settings.getInteger(Config::CHECK_SIGNATURES)) {
		return true;
	}

	// Peek the version of the files
	FileReadHandle dat_file(metadata_path.string());
	if (!dat_file.isOk()) {
		spdlog::error("Could not open metadata file: {}", metadata_path.string());
		return false;
	}

	uint32_t datSignature;
	dat_file.getU32(datSignature);
	dat_file.close();

	FileReadHandle spr_file(sprites_path.string());
	if (!spr_file.isOk()) {
		spdlog::error("Could not open sprites file: {}", sprites_path.string());
		return false;
	}

	uint32_t sprSignature;
	spr_file.getU32(sprSignature);
	spr_file.close();

	for (const auto& clientData : data_versions) {
		if (clientData.sprSignature == sprSignature && clientData.datSignature == datSignature) {
			return true;
		}
	}

	spdlog::error("Signatures are incorrect. Metadata signature: {:X}, Sprites signature: {:X}", datSignature, sprSignature);
	return false;
}

DatFormat ClientVersion::getDatFormatForSignature(uint32_t signature) const {
	for (const auto& data : data_versions) {
		if (data.datSignature == signature) {
			return data.datFormat;
		}
	}

	return DAT_FORMAT_UNKNOWN;
}

std::string ClientVersion::getName() const {
	return name;
}

ClientVersionID ClientVersion::getID() const {
	return name;
}

OtbVersionID ClientVersion::getProtocolID() const {
	return otb.id;
}

bool ClientVersion::isVisible() const {
	return visible;
}

void ClientVersion::setClientPath(const FileName& dir) {
	client_path = dir;
}

bool ClientVersion::hasConfiguredClientPath() const {
	return !client_path.empty();
}

MapVersionID ClientVersion::getPrefferedMapVersionID() const {
	return preferred_map_version;
}

OtbVersion ClientVersion::getOTBVersion() const {
	return otb;
}

ClientVersionList ClientVersion::getExtensionsSupported() const {
	return extension_versions;
}

ClientVersionList ClientVersion::getAllVersionsSupportedForClientVersion(ClientVersion* clientVersion) {
	ClientVersionList versionList;
	for (const auto& versionEntry : client_versions) {
		ClientVersion* version = versionEntry.get();
		for (ClientVersion* checkVersion : version->getExtensionsSupported()) {
			if (clientVersion == checkVersion) {
				versionList.push_back(version);
			}
		}
	}
	std::sort(versionList.begin(), versionList.end(), VersionComparisonPredicate);
	return versionList;
}
void ClientVersion::backup() {
	backup_data.version = version;
	backup_data.name = name;
	backup_data.description = description;
	backup_data.item_definition_mode = item_definition_mode;
	backup_data.metadata_file = metadata_file;
	backup_data.sprites_file = sprites_file;
	backup_data.is_transparent = is_transparent;
	backup_data.is_extended = is_extended;
	backup_data.has_frame_durations = has_frame_durations;
	backup_data.has_frame_groups = has_frame_groups;
	backup_data.client_path = client_path;
	backup_data.data_path = data_path;
	backup_data.preferred_map_version = preferred_map_version;
	backup_data.otb = otb;
	backup_data.data_versions = data_versions;
	backup_data.map_versions_supported = map_versions_supported;
	is_dirty = false;
}

void ClientVersion::restore() {
	version = backup_data.version;
	name = backup_data.name;
	description = backup_data.description;
	item_definition_mode = backup_data.item_definition_mode;
	metadata_file = backup_data.metadata_file;
	sprites_file = backup_data.sprites_file;
	is_transparent = backup_data.is_transparent;
	is_extended = backup_data.is_extended;
	has_frame_durations = backup_data.has_frame_durations;
	has_frame_groups = backup_data.has_frame_groups;
	client_path = backup_data.client_path;
	data_path = backup_data.data_path;
	preferred_map_version = backup_data.preferred_map_version;
	otb = backup_data.otb;
	data_versions = backup_data.data_versions;
	map_versions_supported = backup_data.map_versions_supported;
	is_dirty = false;
}

std::unique_ptr<ClientVersion> ClientVersion::clone() const {
	auto new_cv = std::make_unique<ClientVersion>(otb, name, data_path);
	new_cv->version = version;
	new_cv->visible = visible;
	new_cv->is_transparent = is_transparent;
	new_cv->is_extended = is_extended;
	new_cv->has_frame_durations = has_frame_durations;
	new_cv->has_frame_groups = has_frame_groups;
	new_cv->metadata_file = metadata_file;
	new_cv->sprites_file = sprites_file;
	new_cv->map_versions_supported = map_versions_supported;
	new_cv->preferred_map_version = preferred_map_version;
	new_cv->data_versions = data_versions;
	new_cv->client_path = client_path;
	new_cv->description = description;
	new_cv->item_definition_mode = item_definition_mode;
	return new_cv;
}

bool ClientVersion::isValid() const {
	if (name.empty()) {
		return false;
	}
	return true;
}
