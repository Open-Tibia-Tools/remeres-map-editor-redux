#ifndef RME_ITEM_DEFINITION_STORE_H_
#define RME_ITEM_DEFINITION_STORE_H_

#include "item_definitions/core/item_definition_fragments.h"

#include <array>
#include <limits>
#include <span>
#include <unordered_map>

class ItemDefinitionStore;

struct ItemIdentityTable {
	std::vector<ServerItemId> server_ids;
	std::vector<ItemGroup_t> groups;
	std::vector<ItemTypes_t> types;
};

struct ItemFlagTable {
	std::vector<uint64_t> masks;
};

struct ItemAttributeTable {
	std::vector<uint16_t> volumes;
	std::vector<uint16_t> max_text_lengths;
	std::vector<uint16_t> slot_positions;
	std::vector<uint8_t> weapon_types;
	std::vector<uint8_t> classifications;
	std::vector<uint16_t> border_base_ground_ids;
	std::vector<uint32_t> border_groups;
	std::vector<float> weights;
	std::vector<int> attacks;
	std::vector<int> defenses;
	std::vector<int> armors;
	std::vector<uint32_t> charges;
	std::vector<uint16_t> rotate_to;
	std::vector<uint16_t> way_speeds;
	std::vector<int> always_on_top_orders;
	std::vector<BorderType> border_alignments;
};

struct ItemTextTable {
	std::vector<std::string> names;
	std::vector<std::string> editor_suffixes;
	std::vector<std::string> descriptions;
};

struct ItemPassiveMetadataTable {
	std::vector<std::string> json_blobs;
};

struct ItemVisualTable {
	std::vector<ClientItemId> client_ids;
};

struct ItemEditorTable {
	std::vector<ItemEditorData> data;
};

class ItemDefinitionView {
public:
	ItemDefinitionView() = default;
	ItemDefinitionView(const ItemDefinitionStore* store, DefinitionId index);

	explicit operator bool() const {
		return isValid();
	}

	ServerItemId serverId() const;
	ClientItemId clientId() const;
	ItemGroup_t group() const;
	ItemTypes_t type() const;
	std::string_view name() const;
	std::string_view editorSuffix() const;
	std::string_view description() const;
	std::string_view passiveMetadataJson() const;
	bool hasFlag(ItemFlag flag) const;
	int64_t attribute(ItemAttributeKey key) const;
	const ItemEditorData& editorData() const;

	bool isGroundTile() const;
	bool isSplash() const;
	bool isFluidContainer() const;
	bool isDepot() const;
	bool isMailbox() const;
	bool isTrashHolder() const;
	bool isContainer() const;
	bool isDoor() const;
	bool isMagicField() const;
	bool isTeleport() const;
	bool isBed() const;
	bool isKey() const;
	bool isPodium() const;
	bool isClientCharged() const;
	bool isExtraCharged() const;
	bool isTooltipable() const;
	bool isMetaItem() const;
	bool isFloorChange() const;

private:
	bool isValid() const;

	const ItemDefinitionStore* store_ = nullptr;
	DefinitionId index_ = 0;
};

class ItemDefinitionStore {
public:
	void clear();
	void reserve(size_t count);
	void append(ResolvedItemDefinitionRow row);

	bool exists(ServerItemId server_id) const;
	bool typeExists(ServerItemId server_id) const {
		return exists(server_id);
	}
	ItemDefinitionView get(ServerItemId server_id) const;
	std::optional<ServerItemId> findByClientId(ClientItemId client_id) const;
	std::span<const ServerItemId> findAllByClientId(ClientItemId client_id) const;
	std::span<const ServerItemId> allIds() const {
		return identity_.server_ids;
	}
	ServerItemId maxServerId() const {
		return max_server_id_;
	}
	ServerItemId getMaxID() const {
		return maxServerId();
	}

	void setEditorData(ServerItemId server_id, const ItemEditorData& editor_data);
	ItemEditorData& mutableEditorData(ServerItemId server_id);
	const ItemEditorData& editorData(ServerItemId server_id) const;
	void setMetaItem(ServerItemId server_id, bool value);
	void ensureMetaItem(ServerItemId server_id);
	void setFlag(ServerItemId server_id, ItemFlag flag, bool value);
	void setAttribute(ServerItemId server_id, ItemAttributeKey key, int64_t value);
	void setGroup(ServerItemId server_id, ItemGroup_t group);
	void setType(ServerItemId server_id, ItemTypes_t type);
	void setName(ServerItemId server_id, std::string value);
	void setEditorSuffix(ServerItemId server_id, std::string value);
	void setDescription(ServerItemId server_id, std::string value);

	uint32_t MajorVersion = 0;
	uint32_t MinorVersion = 0;
	uint32_t BuildNumber = 0;

private:
	friend class ItemDefinitionView;

	DefinitionId indexOf(ServerItemId server_id) const;
	bool isFlagSet(DefinitionId index, ItemFlag flag) const;
	void setFlagAtIndex(DefinitionId index, ItemFlag flag, bool value);
	int64_t attributeValue(DefinitionId index, ItemAttributeKey key) const;
	void setAttributeAtIndex(DefinitionId index, ItemAttributeKey key, int64_t value);

	ItemIdentityTable identity_;
	ItemFlagTable flags_;
	ItemAttributeTable attributes_;
	ItemTextTable text_;
	ItemPassiveMetadataTable passive_metadata_;
	ItemVisualTable visual_;
	ItemEditorTable editor_;

	std::vector<DefinitionId> server_to_index_;
	std::unordered_map<ClientItemId, std::vector<ServerItemId>> client_to_servers_;
	mutable std::vector<ServerItemId> empty_client_results_;
	ServerItemId max_server_id_ = 0;
};

extern ItemDefinitionStore g_item_definitions;

#endif
