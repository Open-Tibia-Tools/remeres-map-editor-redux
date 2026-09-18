#include "item_definitions/core/item_definition_store.h"

#include <cmath>
#include <stdexcept>

namespace {
	constexpr uint64_t flagMask(ItemFlag flag) {
		return uint64_t { 1 } << static_cast<uint8_t>(flag);
	}
}

ItemDefinitionStore g_item_definitions;

ItemDefinitionView::ItemDefinitionView(const ItemDefinitionStore* store, DefinitionId index) :
	store_(store), index_(index) {
}

bool ItemDefinitionView::isValid() const {
	return store_ != nullptr && index_ < store_->identity_.server_ids.size() && index_ < store_->visual_.client_ids.size() &&
		index_ < store_->editor_.data.size() && index_ < store_->passive_metadata_.json_blobs.size();
}

std::string_view ItemDefinitionView::passiveMetadataJson() const {
	static constexpr std::string_view empty;
	return isValid() ? std::string_view(store_->passive_metadata_.json_blobs[index_]) : empty;
}

ServerItemId ItemDefinitionView::serverId() const {
	return isValid() ? store_->identity_.server_ids[index_] : 0;
}

ClientItemId ItemDefinitionView::clientId() const {
	return isValid() ? store_->visual_.client_ids[index_] : 0;
}

ItemGroup_t ItemDefinitionView::group() const {
	return isValid() ? store_->identity_.groups[index_] : ITEM_GROUP_NONE;
}

ItemTypes_t ItemDefinitionView::type() const {
	return isValid() ? store_->identity_.types[index_] : ITEM_TYPE_NONE;
}

std::string_view ItemDefinitionView::name() const {
	static constexpr std::string_view empty;
	return isValid() ? std::string_view(store_->text_.names[index_]) : empty;
}

std::string_view ItemDefinitionView::editorSuffix() const {
	static constexpr std::string_view empty;
	return isValid() ? std::string_view(store_->text_.editor_suffixes[index_]) : empty;
}

std::string_view ItemDefinitionView::description() const {
	static constexpr std::string_view empty;
	return isValid() ? std::string_view(store_->text_.descriptions[index_]) : empty;
}

bool ItemDefinitionView::hasFlag(ItemFlag flag) const {
	return isValid() && store_->isFlagSet(index_, flag);
}

int64_t ItemDefinitionView::attribute(ItemAttributeKey key) const {
	return isValid() ? store_->attributeValue(index_, key) : 0;
}

const ItemEditorData& ItemDefinitionView::editorData() const {
	static const ItemEditorData empty;
	return isValid() ? store_->editor_.data[index_] : empty;
}

bool ItemDefinitionView::isGroundTile() const { return group() == ITEM_GROUP_GROUND; }
bool ItemDefinitionView::isSplash() const { return group() == ITEM_GROUP_SPLASH; }
bool ItemDefinitionView::isFluidContainer() const { return group() == ITEM_GROUP_FLUID; }
bool ItemDefinitionView::isDepot() const { return type() == ITEM_TYPE_DEPOT; }
bool ItemDefinitionView::isMailbox() const { return type() == ITEM_TYPE_MAILBOX; }
bool ItemDefinitionView::isTrashHolder() const { return type() == ITEM_TYPE_TRASHHOLDER; }
bool ItemDefinitionView::isContainer() const { return type() == ITEM_TYPE_CONTAINER; }
bool ItemDefinitionView::isDoor() const { return type() == ITEM_TYPE_DOOR; }
bool ItemDefinitionView::isMagicField() const { return type() == ITEM_TYPE_MAGICFIELD; }
bool ItemDefinitionView::isTeleport() const { return type() == ITEM_TYPE_TELEPORT; }
bool ItemDefinitionView::isBed() const { return type() == ITEM_TYPE_BED; }
bool ItemDefinitionView::isKey() const { return type() == ITEM_TYPE_KEY; }
bool ItemDefinitionView::isPodium() const { return type() == ITEM_TYPE_PODIUM; }
bool ItemDefinitionView::isClientCharged() const { return hasFlag(ItemFlag::ClientChargeable); }
bool ItemDefinitionView::isExtraCharged() const { return hasFlag(ItemFlag::ExtraChargeable); }
bool ItemDefinitionView::isTooltipable() const { return hasFlag(ItemFlag::Tooltipable); }
bool ItemDefinitionView::isMetaItem() const { return hasFlag(ItemFlag::MetaItem); }
bool ItemDefinitionView::isFloorChange() const { return hasFlag(ItemFlag::FloorChange); }

void ItemDefinitionStore::clear() {
	identity_ = {};
	flags_ = {};
	attributes_ = {};
	text_ = {};
	visual_ = {};
	passive_metadata_ = {};
	editor_ = {};
	server_to_index_.clear();
	client_to_servers_.clear();
	empty_client_results_.clear();
	max_server_id_ = 0;
	MajorVersion = 0;
	MinorVersion = 0;
	BuildNumber = 0;
}

void ItemDefinitionStore::reserve(size_t count) {
	identity_.server_ids.reserve(count);
	identity_.groups.reserve(count);
	identity_.types.reserve(count);
	flags_.masks.reserve(count);
	attributes_.volumes.reserve(count);
	attributes_.max_text_lengths.reserve(count);
	attributes_.slot_positions.reserve(count);
	attributes_.weapon_types.reserve(count);
	attributes_.classifications.reserve(count);
	attributes_.border_base_ground_ids.reserve(count);
	attributes_.border_groups.reserve(count);
	attributes_.weights.reserve(count);
	attributes_.attacks.reserve(count);
	attributes_.defenses.reserve(count);
	attributes_.armors.reserve(count);
	attributes_.charges.reserve(count);
	attributes_.rotate_to.reserve(count);
	attributes_.way_speeds.reserve(count);
	attributes_.always_on_top_orders.reserve(count);
	attributes_.border_alignments.reserve(count);
	text_.names.reserve(count);
	text_.editor_suffixes.reserve(count);
	text_.descriptions.reserve(count);
	passive_metadata_.json_blobs.reserve(count);
	visual_.client_ids.reserve(count);
	editor_.data.reserve(count);
	server_to_index_.reserve(count + 100);
}

void ItemDefinitionStore::append(ResolvedItemDefinitionRow row) {
	const DefinitionId index = static_cast<DefinitionId>(identity_.server_ids.size());
	identity_.server_ids.push_back(row.server_id);
	identity_.groups.push_back(row.group);
	identity_.types.push_back(row.type);
	flags_.masks.push_back(row.flags);
	attributes_.volumes.push_back(row.volume);
	attributes_.max_text_lengths.push_back(row.max_text_len);
	attributes_.slot_positions.push_back(row.slot_position);
	attributes_.weapon_types.push_back(row.weapon_type);
	attributes_.classifications.push_back(row.classification);
	attributes_.border_base_ground_ids.push_back(row.border_base_ground_id);
	attributes_.border_groups.push_back(row.border_group);
	attributes_.weights.push_back(row.weight);
	attributes_.attacks.push_back(row.attack);
	attributes_.defenses.push_back(row.defense);
	attributes_.armors.push_back(row.armor);
	attributes_.charges.push_back(row.charges);
	attributes_.rotate_to.push_back(row.rotate_to);
	attributes_.way_speeds.push_back(row.way_speed);
	attributes_.always_on_top_orders.push_back(row.always_on_top_order);
	attributes_.border_alignments.push_back(row.border_alignment);
	text_.names.push_back(std::move(row.name));
	text_.editor_suffixes.push_back(std::move(row.editor_suffix));
	text_.descriptions.push_back(std::move(row.description));
	passive_metadata_.json_blobs.push_back(std::move(row.passive_metadata_json));
	visual_.client_ids.push_back(row.client_id);
	editor_.data.emplace_back();

	if (row.server_id >= server_to_index_.size()) {
		server_to_index_.resize(static_cast<size_t>(row.server_id) + 1, 0);
	}
	server_to_index_[row.server_id] = index + 1;
	if (row.client_id != 0) {
		client_to_servers_[row.client_id].push_back(row.server_id);
	}
	max_server_id_ = std::max(max_server_id_, row.server_id);
}

bool ItemDefinitionStore::exists(ServerItemId server_id) const {
	if (server_id >= server_to_index_.size()) {
		return false;
	}
	return server_to_index_[server_id] != 0;
}

ItemDefinitionView ItemDefinitionStore::get(ServerItemId server_id) const {
	if (server_id >= server_to_index_.size()) {
		return {};
	}
	const DefinitionId stored_index = server_to_index_[server_id];
	if (stored_index == 0) {
		return {};
	}
	return ItemDefinitionView(this, stored_index - 1);
}

std::optional<ServerItemId> ItemDefinitionStore::findByClientId(ClientItemId client_id) const {
	const auto it = client_to_servers_.find(client_id);
	if (it == client_to_servers_.end() || it->second.empty()) {
		return std::nullopt;
	}
	return it->second.front();
}

std::span<const ServerItemId> ItemDefinitionStore::findAllByClientId(ClientItemId client_id) const {
	const auto it = client_to_servers_.find(client_id);
	if (it == client_to_servers_.end()) {
		return empty_client_results_;
	}
	return it->second;
}

void ItemDefinitionStore::setEditorData(ServerItemId server_id, const ItemEditorData& editor_data) {
	editor_.data[indexOf(server_id)] = editor_data;
}

ItemEditorData& ItemDefinitionStore::mutableEditorData(ServerItemId server_id) {
	return editor_.data[indexOf(server_id)];
}

const ItemEditorData& ItemDefinitionStore::editorData(ServerItemId server_id) const {
	return editor_.data[indexOf(server_id)];
}

void ItemDefinitionStore::setMetaItem(ServerItemId server_id, bool value) {
	setFlagAtIndex(indexOf(server_id), ItemFlag::MetaItem, value);
}

void ItemDefinitionStore::ensureMetaItem(ServerItemId server_id) {
	if (exists(server_id)) {
		setMetaItem(server_id, true);
		return;
	}

	ResolvedItemDefinitionRow row;
	row.server_id = server_id;
	row.flags = flagMask(ItemFlag::MetaItem);
	append(std::move(row));
}

void ItemDefinitionStore::setFlag(ServerItemId server_id, ItemFlag flag, bool value) {
	setFlagAtIndex(indexOf(server_id), flag, value);
}

void ItemDefinitionStore::setAttribute(ServerItemId server_id, ItemAttributeKey key, int64_t value) {
	setAttributeAtIndex(indexOf(server_id), key, value);
}

void ItemDefinitionStore::setGroup(ServerItemId server_id, ItemGroup_t group) {
	identity_.groups[indexOf(server_id)] = group;
}

void ItemDefinitionStore::setType(ServerItemId server_id, ItemTypes_t type) {
	identity_.types[indexOf(server_id)] = type;
}

void ItemDefinitionStore::setName(ServerItemId server_id, std::string value) {
	text_.names[indexOf(server_id)] = std::move(value);
}

void ItemDefinitionStore::setEditorSuffix(ServerItemId server_id, std::string value) {
	text_.editor_suffixes[indexOf(server_id)] = std::move(value);
}

void ItemDefinitionStore::setDescription(ServerItemId server_id, std::string value) {
	text_.descriptions[indexOf(server_id)] = std::move(value);
}

DefinitionId ItemDefinitionStore::indexOf(ServerItemId server_id) const {
	if (server_id >= server_to_index_.size()) {
		throw std::out_of_range("Unknown item definition id");
	}
	const DefinitionId stored_index = server_to_index_[server_id];
	if (stored_index == 0) {
		throw std::out_of_range("Unknown item definition id");
	}
	return stored_index - 1;
}

bool ItemDefinitionStore::isFlagSet(DefinitionId index, ItemFlag flag) const {
	return (flags_.masks[index] & flagMask(flag)) != 0;
}

void ItemDefinitionStore::setFlagAtIndex(DefinitionId index, ItemFlag flag, bool value) {
	if (value) {
		flags_.masks[index] |= flagMask(flag);
	} else {
		flags_.masks[index] &= ~flagMask(flag);
	}
}

int64_t ItemDefinitionStore::attributeValue(DefinitionId index, ItemAttributeKey key) const {
	switch (key) {
		case ItemAttributeKey::Volume: return attributes_.volumes[index];
		case ItemAttributeKey::MaxTextLen: return attributes_.max_text_lengths[index];
		case ItemAttributeKey::SlotPosition: return attributes_.slot_positions[index];
		case ItemAttributeKey::WeaponType: return attributes_.weapon_types[index];
		case ItemAttributeKey::Classification: return attributes_.classifications[index];
		case ItemAttributeKey::BorderBaseGroundId: return attributes_.border_base_ground_ids[index];
		case ItemAttributeKey::BorderGroup: return attributes_.border_groups[index];
		case ItemAttributeKey::Weight: return static_cast<int64_t>(std::llround(attributes_.weights[index] * 1000.0f));
		case ItemAttributeKey::Attack: return attributes_.attacks[index];
		case ItemAttributeKey::Defense: return attributes_.defenses[index];
		case ItemAttributeKey::Armor: return attributes_.armors[index];
		case ItemAttributeKey::Charges: return attributes_.charges[index];
		case ItemAttributeKey::RotateTo: return attributes_.rotate_to[index];
		case ItemAttributeKey::WaySpeed: return attributes_.way_speeds[index];
		case ItemAttributeKey::AlwaysOnTopOrder: return attributes_.always_on_top_orders[index];
		case ItemAttributeKey::BorderAlignment: return static_cast<int64_t>(attributes_.border_alignments[index]);
	}
	return 0;
}

void ItemDefinitionStore::setAttributeAtIndex(DefinitionId index, ItemAttributeKey key, int64_t value) {
	switch (key) {
		case ItemAttributeKey::Volume: attributes_.volumes[index] = static_cast<uint16_t>(value); break;
		case ItemAttributeKey::MaxTextLen: attributes_.max_text_lengths[index] = static_cast<uint16_t>(value); break;
		case ItemAttributeKey::SlotPosition: attributes_.slot_positions[index] = static_cast<uint16_t>(value); break;
		case ItemAttributeKey::WeaponType: attributes_.weapon_types[index] = static_cast<uint8_t>(value); break;
		case ItemAttributeKey::Classification: attributes_.classifications[index] = static_cast<uint8_t>(value); break;
		case ItemAttributeKey::BorderBaseGroundId: attributes_.border_base_ground_ids[index] = static_cast<uint16_t>(value); break;
		case ItemAttributeKey::BorderGroup: attributes_.border_groups[index] = static_cast<uint32_t>(value); break;
		case ItemAttributeKey::Weight: attributes_.weights[index] = static_cast<float>(value) / 1000.0f; break;
		case ItemAttributeKey::Attack: attributes_.attacks[index] = static_cast<int>(value); break;
		case ItemAttributeKey::Defense: attributes_.defenses[index] = static_cast<int>(value); break;
		case ItemAttributeKey::Armor: attributes_.armors[index] = static_cast<int>(value); break;
		case ItemAttributeKey::Charges: attributes_.charges[index] = static_cast<uint32_t>(value); break;
		case ItemAttributeKey::RotateTo: attributes_.rotate_to[index] = static_cast<uint16_t>(value); break;
		case ItemAttributeKey::WaySpeed: attributes_.way_speeds[index] = static_cast<uint16_t>(value); break;
		case ItemAttributeKey::AlwaysOnTopOrder: attributes_.always_on_top_orders[index] = static_cast<int>(value); break;
		case ItemAttributeKey::BorderAlignment: attributes_.border_alignments[index] = static_cast<BorderType>(value); break;
	}
}
