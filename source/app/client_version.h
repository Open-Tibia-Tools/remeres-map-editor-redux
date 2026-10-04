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

#ifndef RME_CLIENT_VERSION_H_
#define RME_CLIENT_VERSION_H_

#include <string>
#include <vector>
#include <memory>
#include <wx/string.h>
#include <wx/filename.h>

#include "app/main.h"
#include "app/settings.h"
#include "item_definitions/core/item_definition_types.h"

using ClientVersionID = std::string;
using OtbVersionID = int;

// Protocol versions (Legacy/OTB IDs)
enum ProtocolVersions : OtbVersionID {
	PROTOCOL_VERSION_NONE = 0,
	PROTOCOL_VERSION_ALL = -1,
	CLIENT_VERSION_750 = 1,
	CLIENT_VERSION_755 = 2,
	CLIENT_VERSION_760 = 3,
	CLIENT_VERSION_770 = 3,
	CLIENT_VERSION_780 = 4,
	CLIENT_VERSION_790 = 5,
	CLIENT_VERSION_792 = 6,
	CLIENT_VERSION_800 = 7,
	CLIENT_VERSION_810 = 8,
	CLIENT_VERSION_811 = 9,
	CLIENT_VERSION_820 = 10, // After this version, OTBM stores charges as an attribute
	CLIENT_VERSION_830 = 11,
	CLIENT_VERSION_840 = 12,
	CLIENT_VERSION_841 = 13,
	CLIENT_VERSION_842 = 14,
	CLIENT_VERSION_850 = 15,
	CLIENT_VERSION_854_BAD = 16, // Legacy
	CLIENT_VERSION_854 = 17,
	CLIENT_VERSION_855 = 18,
	CLIENT_VERSION_860_OLD = 19, // Legacy
	CLIENT_VERSION_860 = 20,
	CLIENT_VERSION_861 = 21,
	CLIENT_VERSION_862 = 22,
	CLIENT_VERSION_870 = 23,
	CLIENT_VERSION_871 = 24,
	CLIENT_VERSION_872 = 25,
	CLIENT_VERSION_873 = 26,
	CLIENT_VERSION_900 = 27,
	CLIENT_VERSION_910 = 28,
	CLIENT_VERSION_920 = 29,
	CLIENT_VERSION_940 = 30,
	CLIENT_VERSION_944_V1 = 31, // Legacy
	CLIENT_VERSION_944_V2 = 32, // Legacy
	CLIENT_VERSION_944_V3 = 33, // Legacy
	CLIENT_VERSION_944_V4 = 34,
	CLIENT_VERSION_946 = 35,
	CLIENT_VERSION_950 = 36,
	CLIENT_VERSION_952 = 37,
	CLIENT_VERSION_953 = 38,
	CLIENT_VERSION_954 = 39,
	CLIENT_VERSION_960 = 40,
	CLIENT_VERSION_961 = 41,
	CLIENT_VERSION_963 = 42,
	CLIENT_VERSION_970 = 43,
	CLIENT_VERSION_980 = 44,
	CLIENT_VERSION_981 = 45,
	CLIENT_VERSION_982 = 46,
	CLIENT_VERSION_983 = 47,
	CLIENT_VERSION_985 = 48,
	CLIENT_VERSION_986 = 49,
	CLIENT_VERSION_1010 = 50,
	CLIENT_VERSION_1020 = 51,
	CLIENT_VERSION_1021 = 52,
	CLIENT_VERSION_1030 = 53,
	CLIENT_VERSION_1031 = 54,
	CLIENT_VERSION_1041 = 55,
	CLIENT_VERSION_1077 = 56,
	CLIENT_VERSION_1098 = 57,
	CLIENT_VERSION_10100 = 58,
	CLIENT_VERSION_1271 = 59,
	CLIENT_VERSION_1281 = 60,
	CLIENT_VERSION_1285 = 61,
	CLIENT_VERSION_1286 = 62,
	CLIENT_VERSION_1287 = 63,
	CLIENT_VERSION_1290 = 64,
	CLIENT_VERSION_1310 = 65,
	CLIENT_VERSION_1320 = 66,

};

#define CLIENT_VERSION_NONE ""
#define OTB_VERSION_NONE 0

// OTBM versions
enum MapVersionID {
	MAP_OTBM_UNKNOWN = -1,
	MAP_OTBM_1 = 0,
	MAP_OTBM_2 = 1,
	MAP_OTBM_3 = 2,
	MAP_OTBM_4 = 3,
};

inline MapVersionID normalizeOtbmVersion(uint32_t raw_version) noexcept {
	if (raw_version == 4) {
		return MAP_OTBM_4;
	}
	return static_cast<MapVersionID>(raw_version);
}

// The composed version of a otbm file (otbm version, client version)
struct MapVersion {
	MapVersion() :
		otbm(MAP_OTBM_1), client(0) { }
	MapVersion(MapVersionID m, OtbVersionID c) :
		otbm(m), client(c) { }
	MapVersionID otbm;
	OtbVersionID client;
	uint32_t items_major = 0; // OTB format version stored in the header; disambiguates clients sharing an OTB id
};

enum OtbFormatVersion : uint32_t {
	OTB_VERSION_1 = 1,
	OTB_VERSION_2 = 2,
	OTB_VERSION_3 = 3,
};

// Represents an OTB version
struct OtbVersion {
	// '8.60', '7.40' etc.
	std::string name;
	// What file format the OTB is in (version 1..3)
	OtbFormatVersion format_version;
	// The minor version ID of the OTB (maps to CLIENT_VERSION in OTServ)
	OtbVersionID id;
};

// Formats for the metadata file
enum DatFormat {
	DAT_FORMAT_UNKNOWN,
	DAT_FORMAT_74,
	DAT_FORMAT_755,
	DAT_FORMAT_78,
	DAT_FORMAT_86,
	DAT_FORMAT_96,
	DAT_FORMAT_1010,
	DAT_FORMAT_1050,
	DAT_FORMAT_1057
};

enum DatFlags : uint8_t {
	DatFlagGround = 0,
	DatFlagGroundBorder = 1,
	DatFlagOnBottom = 2,
	DatFlagOnTop = 3,
	DatFlagContainer = 4,
	DatFlagStackable = 5,
	DatFlagForceUse = 6,
	DatFlagMultiUse = 7,
	DatFlagWritable = 8,
	DatFlagWritableOnce = 9,
	DatFlagFluidContainer = 10,
	DatFlagSplash = 11,
	DatFlagNotWalkable = 12,
	DatFlagNotMoveable = 13,
	DatFlagBlockProjectile = 14,
	DatFlagNotPathable = 15,
	DatFlagPickupable = 16,
	DatFlagHangable = 17,
	DatFlagHookSouth = 18,
	DatFlagHookEast = 19,
	DatFlagRotateable = 20,
	DatFlagLight = 21,
	DatFlagDontHide = 22,
	DatFlagTranslucent = 23,
	DatFlagDisplacement = 24,
	DatFlagElevation = 25,
	DatFlagLyingCorpse = 26,
	DatFlagAnimateAlways = 27,
	DatFlagMinimapColor = 28,
	DatFlagLensHelp = 29,
	DatFlagFullGround = 30,
	DatFlagLook = 31,
	DatFlagCloth = 32,
	DatFlagMarket = 33,
	DatFlagUsable = 34,
	DatFlagWrappable = 35,
	DatFlagUnwrappable = 36,
	DatFlagTopEffect = 37,
	DatFlagWings = 38,
	DatFlagDefault = 40,

	DatFlagFloorChange = 252,
	DatFlagNoMoveAnimation = 253, // 10.10: real value is 16, but we need to do this for backwards compatibility
	DatFlagChargeable = 254,
	DatFlagLast = 255
};

// Represents a client file version
struct ClientData {
	DatFormat datFormat;
	uint32_t datSignature;
	uint32_t sprSignature;
};

// typedef the client version
class ClientVersion;
using ClientVersionList = std::vector<ClientVersion*>;

class ClientVersion : boost::noncopyable {
public:
	ClientVersion(OtbVersion otb, std::string versionName, wxString path);
	~ClientVersion() = default;

	static void loadVersions();
	static void unloadVersions();
	static bool saveVersions();

	static void addVersion(std::unique_ptr<ClientVersion> version);
	static void removeVersion(const ClientVersionID& id);

	static ClientVersion* get(const ClientVersionID& id);
	static ClientVersion* getBestMatch(OtbVersionID id);
	static ClientVersion* getByItemsVersion(uint32_t major, uint32_t minor);
	static ClientVersionList getVisible(std::string from, std::string to);
	static ClientVersionList getAll();
	static ClientVersionList getAllVisible();
	static ClientVersionList getConfiguredVisible();
	static ClientVersionList getAllForOTBMVersion(MapVersionID map_version);
	static ClientVersionList getAllVersionsSupportedForClientVersion(ClientVersion* v);
	static ClientVersion* getLatestVersion();
	static void setLatestVersion(ClientVersion* version);

	std::unique_ptr<ClientVersion> clone() const;
	bool isValid() const;

	bool operator==(const ClientVersion& o) const {
		return name == o.name;
	}

	bool hasValidPaths();
	bool loadValidPaths();
	bool isDefaultPath() const;
	void setClientPath(const FileName& dir);
	bool hasConfiguredClientPath() const;

	bool isVisible() const;
	std::string getName() const;

	ClientVersionID getID() const;
	OtbVersionID getProtocolID() const;
	MapVersionID getPrefferedMapVersionID() const;
	OtbVersion getOTBVersion() const;
	DatFormat getDatFormatForSignature(uint32_t signature) const;
	static DatFormat getDatFormatForVersion(int version);
	ClientVersionList getExtensionsSupported() const;

	bool isProtobuf() const {
        return item_definition_mode == ItemDefinitionMode::ProtobufOnly || item_definition_mode == ItemDefinitionMode::ProtobufOtb;
	}

	void markDirty() {
		is_dirty = true;
	}
	void clearDirty() {
		is_dirty = false;
	}
	bool isDirty() const {
		return is_dirty;
	}

	void backup();
	void restore();

	bool isTransparent() const {
		return is_transparent;
	}
	void setTransparent(bool v) {
		is_transparent = v;
	}

	bool isExtended() const {
		return is_extended;
	}
	void setExtended(bool v) {
		is_extended = v;
	}

	bool hasFrameDurations() const {
		return has_frame_durations;
	}
	void setFrameDurations(bool v) {
		has_frame_durations = v;
	}

	bool hasFrameGroups() const {
		return has_frame_groups;
	}
	void setFrameGroups(bool v) {
		has_frame_groups = v;
	}

	std::string getMetadataFile() const {
		return metadata_file;
	}
	void setMetadataFile(const std::string& v) {
		metadata_file = v;
	}

	std::string getSpritesFile() const {
		return sprites_file;
	}
	void setSpritesFile(const std::string& v) {
		sprites_file = v;
	}

	uint32_t getVersion() const {
		return version;
	}
	void setVersion(uint32_t v);

	void setName(const std::string& v) {
		name = v;
	}

	uint32_t getOtbId() const {
		return otb.id;
	}
	void setOtbId(uint32_t v) {
		otb.id = static_cast<OtbVersionID>(v);
	}

	uint32_t getOtbMajor() const {
		return otb.format_version;
	}
	void setOtbMajor(uint32_t v) {
		otb.format_version = static_cast<OtbFormatVersion>(v);
	}

	std::vector<MapVersionID>& getMapVersionsSupported() {
		return map_versions_supported;
	}
	const std::vector<MapVersionID>& getMapVersionsSupported() const {
		return map_versions_supported;
	}
	void setMapVersionsSupported(const std::vector<MapVersionID>& v) {
		map_versions_supported = v;
	}

	std::string getDataDirectory() const {
		return data_path.ToStdString();
	}
	void setDataDirectory(const std::string& v) {
		data_path = wxstr(v);
	}

	uint32_t getDatSignature() const {
		return data_versions.empty() ? 0 : data_versions[0].datSignature;
	}
	DatFormat getDatFormat() const {
		return data_versions.empty() ? DAT_FORMAT_UNKNOWN : data_versions[0].datFormat;
	}
	void setDatFormat(DatFormat format) {
		if (data_versions.empty()) {
			data_versions.push_back(ClientData { .datFormat = format, .datSignature = 0, .sprSignature = 0 });
			return;
		}
		data_versions[0].datFormat = format;
	}
	void setDatSignature(uint32_t v) {
		if (data_versions.empty()) {
			data_versions.push_back(ClientData {
				.datFormat = DAT_FORMAT_UNKNOWN,
				.datSignature = v,
				.sprSignature = 0,
			});
			return;
		}
		data_versions[0].datSignature = v;
	}

	uint32_t getSprSignature() const {
		return data_versions.empty() ? 0 : data_versions[0].sprSignature;
	}
	void setSprSignature(uint32_t v) {
		if (data_versions.empty()) {
			data_versions.push_back(ClientData {
				.datFormat = DAT_FORMAT_UNKNOWN,
				.datSignature = 0,
				.sprSignature = v,
			});
			return;
		}
		data_versions[0].sprSignature = v;
	}
	void setClientData(uint32_t dat_signature, uint32_t spr_signature, DatFormat format) {
		if (data_versions.empty()) {
			data_versions.push_back(ClientData {
				.datFormat = format,
				.datSignature = dat_signature,
				.sprSignature = spr_signature,
			});
			return;
		}
		data_versions[0] = ClientData {
			.datFormat = format,
			.datSignature = dat_signature,
			.sprSignature = spr_signature,
		};
	}

	std::string getDescription() const {
		return description;
	}
	void setDescription(const std::string& v) {
		description = v;
	}

	std::string getConfigType() const {
		return toString(item_definition_mode);
	}
	void setConfigType(const std::string& v) {
		item_definition_mode = parseItemDefinitionMode(v).value_or(ItemDefinitionMode::DatOtb);
	}
	ItemDefinitionMode getItemDefinitionMode() const {
		return item_definition_mode;
	}

	FileName getDataPath() const;
	FileName getLocalDataPath() const;
	FileName getClientPath() const {
		return client_path;
	}
	wxFileName getMetadataPath() const {
		return metadata_path;
	}
	wxFileName getSpritesPath() const {
		return sprites_path;
	}

private:
	bool is_dirty = false;

	struct BackupData {
		uint32_t version;
		std::string name;
		std::string description;
		ItemDefinitionMode item_definition_mode;
		std::string metadata_file;
		std::string sprites_file;
		bool is_transparent;
		bool is_extended;
		bool has_frame_durations;
		bool has_frame_groups;
		FileName client_path;
		wxString data_path;
		MapVersionID preferred_map_version;
		OtbVersion otb;
		std::vector<ClientData> data_versions;
		std::vector<MapVersionID> map_versions_supported;
	} backup_data;

private:
	OtbVersion otb;

	uint32_t version;
	std::string name;
	bool visible;
	bool is_transparent;
	bool is_extended;
	bool has_frame_durations;
	bool has_frame_groups;

	std::string metadata_file;
	std::string sprites_file;

	std::vector<MapVersionID> map_versions_supported;
	MapVersionID preferred_map_version;
	std::vector<ClientData> data_versions;
	std::vector<ClientVersion*> extension_versions;

	wxString data_path;
	FileName client_path;
	wxFileName metadata_path;
	wxFileName sprites_path;
	std::string description;
	ItemDefinitionMode item_definition_mode = ItemDefinitionMode::DatOtb;

private:
	static void loadVersionsFromTOML(const std::string& configPath);

	// All versions
	using VersionList = std::vector<std::unique_ptr<ClientVersion>>;
	static VersionList client_versions;
	static ClientVersion* latest_version;
	static std::string loaded_file_path;
};

inline bool VersionComparisonPredicate(ClientVersion* a, ClientVersion* b) {
	if (a->getProtocolID() < b->getProtocolID()) {
		return true;
	}
	return false;
}

#endif
