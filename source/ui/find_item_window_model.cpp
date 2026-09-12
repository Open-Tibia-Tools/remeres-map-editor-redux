#include "ui/find_item_window_model.h"

#include "app/settings.h"
#include "brushes/creature/creature_brush.h"
#include "brushes/raw/raw_brush.h"
#include "game/creatures.h"
#include "item_definitions/core/item_definition_store.h"
#include "rendering/core/game_sprite.h"
#include "ui/gui.h"
#include "util/common.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <string_view>
#include <tuple>

namespace {
	struct CatalogMatch {
		size_t index = 0;
		int score = 0;
		size_t token_count = 0;
	};

	struct QueryToken {
		std::string value;
		bool numeric = false;
		bool prefix_wildcard = false;
		bool suffix_wildcard = false;
	};

	[[nodiscard]] std::string trimCopy(std::string_view value) {
		const auto first = value.find_first_not_of(" \t\r\n");
		if (first == std::string_view::npos) {
			return {};
		}
		const auto last = value.find_last_not_of(" \t\r\n");
		return std::string(value.substr(first, last - first + 1));
	}

	[[nodiscard]] std::vector<std::string> tokenizeLower(std::string_view value) {
		std::vector<std::string> tokens;
		std::string current;

		for (const unsigned char ch : value) {
			if (std::isalnum(ch) != 0) {
				current.push_back(static_cast<char>(ch));
				continue;
			}

			if (!current.empty()) {
				tokens.push_back(std::move(current));
				current.clear();
			}
		}

		if (!current.empty()) {
			tokens.push_back(std::move(current));
		}

		return tokens;
	}

	struct ParsedQuery {
		std::string lower_text;
		std::vector<QueryToken> tokens;
	};

	[[nodiscard]] std::string trimToken(std::string_view value) {
		size_t first = 0;
		size_t last = value.size();

		while (first < last) {
			const unsigned char ch = static_cast<unsigned char>(value[first]);
			if (std::isalnum(ch) != 0 || ch == '*') {
				break;
			}
			++first;
		}

		while (last > first) {
			const unsigned char ch = static_cast<unsigned char>(value[last - 1]);
			if (std::isalnum(ch) != 0 || ch == '*') {
				break;
			}
			--last;
		}

		return std::string(value.substr(first, last - first));
	}

	[[nodiscard]] std::vector<QueryToken> parseQueryTokens(std::string_view value) {
		std::vector<QueryToken> tokens;
		size_t cursor = 0;

		while (cursor < value.size()) {
			while (cursor < value.size() && std::isspace(static_cast<unsigned char>(value[cursor])) != 0) {
				++cursor;
			}

			if (cursor >= value.size()) {
				break;
			}

			size_t next = cursor;
			while (next < value.size() && std::isspace(static_cast<unsigned char>(value[next])) == 0) {
				++next;
			}

			std::string raw = trimToken(value.substr(cursor, next - cursor));
			cursor = next;
			if (raw.empty()) {
				continue;
			}

			QueryToken token;
			token.prefix_wildcard = raw.starts_with('*');
			token.suffix_wildcard = raw.ends_with('*');
			if (token.prefix_wildcard) {
				raw.erase(raw.begin());
			}
			if (token.suffix_wildcard && !raw.empty()) {
				raw.pop_back();
			}

			for (const unsigned char ch : raw) {
				if (std::isalnum(ch) != 0) {
					token.value.push_back(static_cast<char>(ch));
				}
			}

			if (token.value.empty()) {
				continue;
			}

			token.numeric = std::all_of(token.value.begin(), token.value.end(), [](unsigned char ch) {
				return std::isdigit(ch) != 0;
			});

			tokens.push_back(std::move(token));
		}

		return tokens;
	}

	[[nodiscard]] ParsedQuery parseFinderQuery(const AdvancedFinderQuery& query) {
		ParsedQuery parsed;
		parsed.lower_text = trimCopy(as_lower_str(query.text));
		parsed.tokens = parseQueryTokens(parsed.lower_text);
		return parsed;
	}

	[[nodiscard]] int scoreTextTokenMatch(const QueryToken& query_token, std::string_view name_token) {
		if (query_token.value == name_token) {
			return 0;
		}

		if (query_token.prefix_wildcard && query_token.suffix_wildcard && name_token.contains(query_token.value)) {
			return 30 + static_cast<int>(name_token.size() - query_token.value.size());
		}

		if (query_token.suffix_wildcard && name_token.starts_with(query_token.value)) {
			return 10 + static_cast<int>(name_token.size() - query_token.value.size());
		}

		if (query_token.prefix_wildcard && name_token.ends_with(query_token.value)) {
			return 20 + static_cast<int>(name_token.size() - query_token.value.size());
		}

		if (!query_token.prefix_wildcard && !query_token.suffix_wildcard && name_token.starts_with(query_token.value)) {
			return 10 + static_cast<int>(name_token.size() - query_token.value.size());
		}
		return -1;
	}

	[[nodiscard]] int scoreNumericTokenMatch(const QueryToken& query_token, std::string_view name_token) {
		if (query_token.value == name_token) {
			return 0;
		}

		if (query_token.prefix_wildcard && query_token.suffix_wildcard && name_token.contains(query_token.value)) {
			return 30 + static_cast<int>(name_token.size() - query_token.value.size());
		}

		if (query_token.suffix_wildcard && name_token.starts_with(query_token.value)) {
			return 10 + static_cast<int>(name_token.size() - query_token.value.size());
		}

		if (query_token.prefix_wildcard && name_token.ends_with(query_token.value)) {
			return 20 + static_cast<int>(name_token.size() - query_token.value.size());
		}

		return -1;
	}

	[[nodiscard]] int scoreTokenMatch(const QueryToken& query_token, std::string_view search_term) {
		return query_token.numeric ? scoreNumericTokenMatch(query_token, search_term) : scoreTextTokenMatch(query_token, search_term);
	}

	[[nodiscard]] int fuzzyMatchScore(const AdvancedFinderCatalogRow& row, const ParsedQuery& query) {
		if (query.tokens.empty()) {
			return query.lower_text.empty() ? 0 : -1;
		}

		if (row.lower_label == query.lower_text) {
			return 0;
		}

		int score = 0;
		for (const auto& query_token : query.tokens) {
			int best_score = std::numeric_limits<int>::max();
			for (const auto& search_term : row.search_terms) {
				const int token_score = scoreTokenMatch(query_token, search_term);
				if (token_score >= 0) {
					best_score = std::min(best_score, token_score);
				}
			}

			if (best_score == std::numeric_limits<int>::max()) {
				return -1;
			}
			score += best_score;
		}

		score += static_cast<int>(row.search_terms.size());
		return score;
	}

	[[nodiscard]] bool matchesMasks(const AdvancedFinderCatalogRow& row, const AdvancedFinderQuery& query) {
		if (query.type_mask != 0 && (row.type_mask & query.type_mask) == 0) {
			return false;
		}
		if ((row.property_mask & query.property_mask) != query.property_mask) {
			return false;
		}
		if ((row.interaction_mask & query.interaction_mask) != query.interaction_mask) {
			return false;
		}
		if ((row.visual_mask & query.visual_mask) != query.visual_mask) {
			return false;
		}
		return true;
	}

	[[nodiscard]] int matchScore(const AdvancedFinderCatalogRow& row, const AdvancedFinderQuery& query, const ParsedQuery& parsed_query) {
		if (!matchesMasks(row, query)) {
			return -1;
		}

		if (parsed_query.lower_text.empty()) {
			return 0;
		}

		return fuzzyMatchScore(row, parsed_query);
	}

	[[nodiscard]] AdvancedFinderFilterMask buildTypeMask(const ItemDefinitionView& definition) {
		AdvancedFinderFilterMask mask = 0;
		const auto slot_position = static_cast<uint16_t>(definition.attribute(ItemAttributeKey::SlotPosition));
		const auto weapon_type = static_cast<uint8_t>(definition.attribute(ItemAttributeKey::WeaponType));

		if (definition.isDepot()) {
			mask |= advancedFinderBit(AdvancedFinderTypeFilter::Depot);
		}
		if (definition.isMailbox()) {
			mask |= advancedFinderBit(AdvancedFinderTypeFilter::Mailbox);
		}
		if (definition.isTrashHolder()) {
			mask |= advancedFinderBit(AdvancedFinderTypeFilter::TrashHolder);
		}
		if (definition.isContainer()) {
			mask |= advancedFinderBit(AdvancedFinderTypeFilter::Container);
		}
		if (definition.isDoor()) {
			mask |= advancedFinderBit(AdvancedFinderTypeFilter::Door);
		}
		if (definition.isMagicField()) {
			mask |= advancedFinderBit(AdvancedFinderTypeFilter::MagicField);
		}
		if (definition.isTeleport()) {
			mask |= advancedFinderBit(AdvancedFinderTypeFilter::Teleport);
		}
		if (definition.isBed()) {
			mask |= advancedFinderBit(AdvancedFinderTypeFilter::Bed);
		}
		if (definition.isKey()) {
			mask |= advancedFinderBit(AdvancedFinderTypeFilter::Key);
		}
		if (definition.isPodium()) {
			mask |= advancedFinderBit(AdvancedFinderTypeFilter::Podium);
		}
		if (weapon_type != WEAPON_NONE && weapon_type != WEAPON_AMMO) {
			mask |= advancedFinderBit(AdvancedFinderTypeFilter::Weapon);
		}
		if (weapon_type == WEAPON_AMMO) {
			mask |= advancedFinderBit(AdvancedFinderTypeFilter::Ammo);
		}
		if ((slot_position & SLOTP_HEAD) != 0 || (slot_position & SLOTP_NECKLACE) != 0 || (slot_position & SLOTP_ARMOR) != 0 ||
			(slot_position & SLOTP_LEGS) != 0 || (slot_position & SLOTP_FEET) != 0 || (slot_position & SLOTP_RING) != 0 ||
			((slot_position & SLOTP_AMMO) != 0 && weapon_type != WEAPON_AMMO)) {
			mask |= advancedFinderBit(AdvancedFinderTypeFilter::Armor);
		}
		if (definition.group() == ITEM_GROUP_RUNE) {
			mask |= advancedFinderBit(AdvancedFinderTypeFilter::Rune);
		}

		return mask;
	}

	[[nodiscard]] AdvancedFinderFilterMask buildPropertyMask(const ItemDefinitionView& definition) {
		AdvancedFinderFilterMask mask = 0;
		if (definition.hasFlag(ItemFlag::Unpassable)) {
			mask |= advancedFinderBit(AdvancedFinderPropertyFilter::Unpassable);
		}
		if (!definition.hasFlag(ItemFlag::Moveable)) {
			mask |= advancedFinderBit(AdvancedFinderPropertyFilter::Unmovable);
		}
		if (definition.hasFlag(ItemFlag::BlockMissiles)) {
			mask |= advancedFinderBit(AdvancedFinderPropertyFilter::BlockMissiles);
		}
		if (definition.hasFlag(ItemFlag::BlockPathfinder)) {
			mask |= advancedFinderBit(AdvancedFinderPropertyFilter::BlockPathfinder);
		}
		if (definition.hasFlag(ItemFlag::HasElevation)) {
			mask |= advancedFinderBit(AdvancedFinderPropertyFilter::HasElevation);
		}
		if (definition.isFloorChange()) {
			mask |= advancedFinderBit(AdvancedFinderPropertyFilter::FloorChange);
		}
		if (definition.hasFlag(ItemFlag::FullTile)) {
			mask |= advancedFinderBit(AdvancedFinderPropertyFilter::FullTile);
		}
		return mask;
	}

	[[nodiscard]] AdvancedFinderFilterMask buildInteractionMask(const ItemDefinitionView& definition) {
		AdvancedFinderFilterMask mask = 0;
		if (definition.hasFlag(ItemFlag::CanReadText)) {
			mask |= advancedFinderBit(AdvancedFinderInteractionFilter::Readable);
		}
		if (definition.hasFlag(ItemFlag::CanWriteText)) {
			mask |= advancedFinderBit(AdvancedFinderInteractionFilter::Writeable);
		}
		if (definition.hasFlag(ItemFlag::Pickupable)) {
			mask |= advancedFinderBit(AdvancedFinderInteractionFilter::Pickupable);
		}
		if (definition.hasFlag(ItemFlag::ForceUse) || definition.hasFlag(ItemFlag::MultiUse)) {
			mask |= advancedFinderBit(AdvancedFinderInteractionFilter::ForceUse);
		}
		if (definition.hasFlag(ItemFlag::AllowDistRead)) {
			mask |= advancedFinderBit(AdvancedFinderInteractionFilter::DistRead);
		}
		if (definition.hasFlag(ItemFlag::Rotatable)) {
			mask |= advancedFinderBit(AdvancedFinderInteractionFilter::Rotatable);
		}
		if (definition.hasFlag(ItemFlag::IsHangable)) {
			mask |= advancedFinderBit(AdvancedFinderInteractionFilter::Hangable);
		}
		return mask;
	}

	[[nodiscard]] AdvancedFinderFilterMask buildVisualMask(const ItemDefinitionView& definition) {
		AdvancedFinderFilterMask mask = 0;
		const auto* sprite = dynamic_cast<GameSprite*>(g_gui.gfx.getSprite(definition.clientId()));

		if (sprite && sprite->hasLight()) {
			mask |= advancedFinderBit(AdvancedFinderVisualFilter::HasLight);
		}
		if (sprite && (sprite->animator != nullptr || sprite->frames > 1)) {
			mask |= advancedFinderBit(AdvancedFinderVisualFilter::Animation);
		}
		if (definition.attribute(ItemAttributeKey::AlwaysOnTopOrder) > 0) {
			mask |= advancedFinderBit(AdvancedFinderVisualFilter::AlwaysTop);
		}
		if (definition.hasFlag(ItemFlag::IgnoreLook)) {
			mask |= advancedFinderBit(AdvancedFinderVisualFilter::IgnoreLook);
		}
		if (definition.attribute(ItemAttributeKey::Charges) > 0 || definition.hasFlag(ItemFlag::ClientChargeable) || definition.hasFlag(ItemFlag::ExtraChargeable)) {
			mask |= advancedFinderBit(AdvancedFinderVisualFilter::HasCharges);
		}
		if (definition.hasFlag(ItemFlag::ClientChargeable)) {
			mask |= advancedFinderBit(AdvancedFinderVisualFilter::ClientCharges);
		}
		if (definition.hasFlag(ItemFlag::Decays)) {
			mask |= advancedFinderBit(AdvancedFinderVisualFilter::Decays);
		}
		if (definition.attribute(ItemAttributeKey::WaySpeed) != 100) {
			mask |= advancedFinderBit(AdvancedFinderVisualFilter::HasSpeed);
		}
		return mask;
	}
}

AdvancedFinderPersistedState LoadAdvancedFinderPersistedState() {
	AdvancedFinderPersistedState state;

	state.query.type_mask = g_settings.getUnsignedInteger(Config::ADVANCED_ITEM_FINDER_TYPE_FILTERS);
	state.query.property_mask = g_settings.getUnsignedInteger(Config::ADVANCED_ITEM_FINDER_PROPERTY_FILTERS);
	state.query.interaction_mask = g_settings.getUnsignedInteger(Config::ADVANCED_ITEM_FINDER_INTERACTION_FILTERS);
	state.query.visual_mask = g_settings.getUnsignedInteger(Config::ADVANCED_ITEM_FINDER_VISUAL_FILTERS);
	state.query.text = g_settings.getString(Config::ADVANCED_ITEM_FINDER_QUERY_TEXT);

	state.selection.kind = static_cast<AdvancedFinderCatalogKind>(std::clamp(g_settings.getInteger(Config::ADVANCED_ITEM_FINDER_SELECTED_KIND), 0, 1));
	state.selection.server_id = static_cast<ServerItemId>(std::clamp<int>(g_settings.getInteger(Config::ADVANCED_ITEM_FINDER_SELECTED_SERVER_ID), 0, std::numeric_limits<uint16_t>::max()));
	state.selection.creature_name = as_lower_str(g_settings.getString(Config::ADVANCED_ITEM_FINDER_SELECTED_CREATURE));
	const auto position_x = g_settings.getInteger(Config::ADVANCED_ITEM_FINDER_WINDOW_X);
	const auto position_y = g_settings.getInteger(Config::ADVANCED_ITEM_FINDER_WINDOW_Y);
	if (position_x >= 0 && position_y >= 0) {
		state.position = wxPoint(position_x, position_y);
	}

	state.size = wxSize(
		std::max(900, g_settings.getInteger(Config::ADVANCED_ITEM_FINDER_WINDOW_WIDTH)),
		std::max(680, g_settings.getInteger(Config::ADVANCED_ITEM_FINDER_WINDOW_HEIGHT))
	);

	return state;
}

void SaveAdvancedFinderPersistedState(const AdvancedFinderPersistedState& state) {
	g_settings.setUnsignedInteger(Config::ADVANCED_ITEM_FINDER_TYPE_FILTERS, state.query.type_mask);
	g_settings.setUnsignedInteger(Config::ADVANCED_ITEM_FINDER_PROPERTY_FILTERS, state.query.property_mask);
	g_settings.setUnsignedInteger(Config::ADVANCED_ITEM_FINDER_INTERACTION_FILTERS, state.query.interaction_mask);
	g_settings.setUnsignedInteger(Config::ADVANCED_ITEM_FINDER_VISUAL_FILTERS, state.query.visual_mask);
	g_settings.setString(Config::ADVANCED_ITEM_FINDER_QUERY_TEXT, state.query.text);
	g_settings.setInteger(Config::ADVANCED_ITEM_FINDER_SELECTED_KIND, static_cast<int>(state.selection.kind));
	g_settings.setInteger(Config::ADVANCED_ITEM_FINDER_SELECTED_SERVER_ID, state.selection.server_id);
	g_settings.setString(Config::ADVANCED_ITEM_FINDER_SELECTED_CREATURE, state.selection.creature_name);
	if (state.position != wxDefaultPosition && state.position.x >= 0 && state.position.y >= 0) {
		g_settings.setInteger(Config::ADVANCED_ITEM_FINDER_WINDOW_X, state.position.x);
		g_settings.setInteger(Config::ADVANCED_ITEM_FINDER_WINDOW_Y, state.position.y);
	}

	if (state.size.IsFullySpecified()) {
		g_settings.setInteger(Config::ADVANCED_ITEM_FINDER_WINDOW_WIDTH, state.size.GetWidth());
		g_settings.setInteger(Config::ADVANCED_ITEM_FINDER_WINDOW_HEIGHT, state.size.GetHeight());
	}
}

std::vector<AdvancedFinderCatalogRow> BuildAdvancedFinderCatalog(bool include_creatures) {
	std::vector<AdvancedFinderCatalogRow> catalog;
	catalog.reserve(g_item_definitions.allIds().size() + (include_creatures ? 256 : 0));

	for (const ServerItemId id : g_item_definitions.allIds()) {
		const auto definition = g_item_definitions.get(id);
		if (!definition) {
			continue;
		}

		RAWBrush* raw_brush = definition.editorData().raw_brush;
		if (raw_brush == nullptr) {
			continue;
		}

		AdvancedFinderCatalogRow row;
		row.kind = AdvancedFinderCatalogKind::Item;
		row.brush = raw_brush;
		row.raw_brush = raw_brush;
		row.server_id = id;
		row.client_id = definition.clientId();
		row.label = std::string(definition.name());
		if (row.label.empty()) {
			row.label = raw_brush->getName();
		}
		row.lower_label = as_lower_str(row.label);
		row.name_tokens = tokenizeLower(row.lower_label);
		row.search_terms = row.name_tokens;
		if (row.server_id != 0) {
			row.search_terms.push_back(std::to_string(row.server_id));
		}
		if (row.client_id != 0) {
			row.search_terms.push_back(std::to_string(row.client_id));
		}
		row.type_mask = buildTypeMask(definition);
		row.property_mask = buildPropertyMask(definition);
		row.interaction_mask = buildInteractionMask(definition);
		row.visual_mask = buildVisualMask(definition);
		catalog.push_back(std::move(row));
	}

	if (include_creatures) {
		for (auto it = g_creatures.begin(); it != g_creatures.end(); ++it) {
			CreatureType* creature_type = it->second;
			if (creature_type == nullptr || creature_type->brush == nullptr) {
				continue;
			}

			AdvancedFinderCatalogRow row;
			row.kind = AdvancedFinderCatalogKind::Creature;
			row.brush = creature_type->brush;
			row.creature_brush = creature_type->brush;
			row.label = creature_type->name;
			row.lower_label = as_lower_str(row.label);
			row.name_tokens = tokenizeLower(row.lower_label);
			row.search_terms = row.name_tokens;
			row.search_terms.push_back("creature");
			row.type_mask = advancedFinderBit(AdvancedFinderTypeFilter::Creature);
			catalog.push_back(std::move(row));
		}
	}

	return catalog;
}

std::vector<size_t> FilterAdvancedFinderCatalog(const std::vector<AdvancedFinderCatalogRow>& catalog, const AdvancedFinderQuery& query) {
	const auto parsed_query = parseFinderQuery(query);
	std::vector<CatalogMatch> matches;
	matches.reserve(catalog.size());

	for (size_t index = 0; index < catalog.size(); ++index) {
		const int score = matchScore(catalog[index], query, parsed_query);
		if (score >= 0) {
			matches.push_back(CatalogMatch { index, score, catalog[index].name_tokens.size() });
		}
	}

	std::ranges::sort(matches, [&](const CatalogMatch& lhs, const CatalogMatch& rhs) {
		const auto& lhs_row = catalog[lhs.index];
		const auto& rhs_row = catalog[rhs.index];

		if (parsed_query.lower_text.empty()) {
			const auto lhs_cid = lhs_row.isCreature() ? std::numeric_limits<uint32_t>::max() : static_cast<uint32_t>(lhs_row.client_id);
			const auto rhs_cid = rhs_row.isCreature() ? std::numeric_limits<uint32_t>::max() : static_cast<uint32_t>(rhs_row.client_id);
			return std::tie(lhs_cid, lhs_row.server_id, lhs_row.lower_label) <
				std::tie(rhs_cid, rhs_row.server_id, rhs_row.lower_label);
		}

		return std::tie(lhs.score, lhs.token_count, lhs_row.lower_label, lhs_row.server_id, lhs_row.client_id) <
			std::tie(rhs.score, rhs.token_count, rhs_row.lower_label, rhs_row.server_id, rhs_row.client_id);
	});

	std::vector<size_t> filtered_indices;
	filtered_indices.reserve(matches.size());
	for (const auto& match : matches) {
		filtered_indices.push_back(match.index);
	}
	return filtered_indices;
}

AdvancedFinderSelectionKey MakeAdvancedFinderSelectionKey(const AdvancedFinderCatalogRow& row) {
	AdvancedFinderSelectionKey key;
	key.kind = row.kind;
	key.server_id = row.server_id;
	if (row.isCreature()) {
		key.creature_name = row.lower_label;
	}
	return key;
}

bool AdvancedFinderSelectionMatches(const AdvancedFinderCatalogRow& row, const AdvancedFinderSelectionKey& selection) {
	if (row.kind != selection.kind) {
		return false;
	}

	if (row.isItem()) {
		return row.server_id != 0 && row.server_id == selection.server_id;
	}

	return row.lower_label == selection.creature_name;
}

std::vector<Brush*> FilterBrushesWithAdvancedFinder(const std::vector<Brush*>& brushes, const std::string& queryText) {
	AdvancedFinderQuery query;
	query.text = queryText;
	const auto parsed_query = parseFinderQuery(query);

	if (parsed_query.tokens.empty()) {
		return brushes;
	}

	struct BrushMatch {
		Brush* brush = nullptr;
		int score = 0;
		size_t token_count = 0;
		std::string lower_label;
		ServerItemId server_id = 0;
		ClientItemId client_id = 0;
	};

	std::vector<BrushMatch> matches;
	matches.reserve(brushes.size());

	for (Brush* brush : brushes) {
		if (!brush) {
			continue;
		}

		AdvancedFinderCatalogRow row;
		row.brush = brush;

		if (const auto* raw = dynamic_cast<const RAWBrush*>(brush)) {
			row.kind = AdvancedFinderCatalogKind::Item;
			row.raw_brush = const_cast<RAWBrush*>(raw);
			row.server_id = raw->getItemID();
			const auto def = g_item_definitions.get(row.server_id);
			if (def) {
				row.client_id = def.clientId();
				row.label = std::string(def.name());
			}

			if (row.label.empty()) {
				row.label = raw->getName();
			}
		} else if (const auto* cb = dynamic_cast<const CreatureBrush*>(brush)) {
			row.kind = AdvancedFinderCatalogKind::Creature;
			row.creature_brush = const_cast<CreatureBrush*>(cb);
			if (cb->getType()) {
				row.label = cb->getType()->name;
				if (cb->getType()->outfit.lookType != 0) {
					row.server_id = static_cast<ServerItemId>(cb->getType()->outfit.lookType);
				}
			} else {
				row.label = cb->getName();
			}
		} else {
			row.kind = AdvancedFinderCatalogKind::Item;
			row.label = brush->getName();
			row.server_id = static_cast<ServerItemId>(brush->getID());
			if (brush->getLookID() != 0) {
				row.client_id = static_cast<ClientItemId>(brush->getLookID());
			}
		}

		row.lower_label = as_lower_str(row.label);
		row.name_tokens = tokenizeLower(row.lower_label);
		row.search_terms = row.name_tokens;
		if (row.isCreature()) {
			row.search_terms.push_back("creature");
		}
		if (row.server_id != 0) {
			row.search_terms.push_back(std::to_string(row.server_id));
		}
		if (row.client_id != 0 && row.client_id != row.server_id) {
			row.search_terms.push_back(std::to_string(row.client_id));
		}

		// Also index any additional tokens from the brush full display name (e.g. RAWBrush suffixes like Hook South)
		const std::string brushFullName = as_lower_str(brush->getName());
		if (brushFullName != row.lower_label) {
			const auto extra_tokens = tokenizeLower(brushFullName);
			for (const auto& token : extra_tokens) {
				if (std::ranges::find(row.search_terms, token) == row.search_terms.end()) {
					row.search_terms.push_back(token);
				}
			}
		}

		const int score = fuzzyMatchScore(row, parsed_query);
		if (score >= 0) {
			matches.push_back(BrushMatch {
				.brush = brush,
				.score = score,
				.token_count = row.name_tokens.size(),
				.lower_label = std::move(row.lower_label),
				.server_id = row.server_id,
				.client_id = row.client_id
			});
		}
	}

	std::ranges::sort(matches, [](const BrushMatch& lhs, const BrushMatch& rhs) {
		return std::tie(lhs.score, lhs.token_count, lhs.lower_label, lhs.server_id, lhs.client_id) <
		       std::tie(rhs.score, rhs.token_count, rhs.lower_label, rhs.server_id, rhs.client_id);
	});

	std::vector<Brush*> result;
	result.reserve(matches.size());
	for (const auto& match : matches) {
		result.push_back(match.brush);
	}
	return result;
}

