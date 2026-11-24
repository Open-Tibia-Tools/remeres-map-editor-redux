//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "palette/detected/detected_item_rules.h"

#include <algorithm>
#include <cctype>
#include <format>

bool IsUnregisteredBorder(const DetectedItemFacts& item) {
	return !item.meta && !item.ground && item.ground_border_piece && !item.registered_border && !item.brush_owned;
}

GroundBorderState ClassifyGroundBorder(const DetectedItemFacts& item) {
	if (item.meta || !item.ground || item.unpassable) {
		return GroundBorderState::NotAFloor;
	}
	if (!item.ground_brush_owned) {
		return GroundBorderState::NoGroundBrush;
	}
	return item.ground_brush_has_border ? GroundBorderState::Bordered : GroundBorderState::BrushWithoutBorder;
}

CustomItemKind ClassifyCustomItem(const DetectedItemFacts& item, std::optional<uint32_t> stock_item_count) {
	if (item.meta || !stock_item_count || item.client_id <= *stock_item_count) {
		return CustomItemKind::Stock;
	}
	if (item.ground) {
		return CustomItemKind::Ground;
	}
	if (item.ground_border_piece || item.registered_border) {
		return CustomItemKind::Border;
	}
	if (item.wall) {
		return CustomItemKind::Wall;
	}
	return CustomItemKind::Object;
}

MountainPiece ClassifyUnregisteredMountain(const DetectedItemFacts& item) {
	if (item.meta || item.brush_owned || item.registered_border || !item.unpassable || !item.block_missiles) {
		return MountainPiece::None;
	}
	if (item.ground) {
		return MountainPiece::Ground;
	}
	return item.ground_border_piece ? MountainPiece::Border : MountainPiece::None;
}

std::string DescribeBrokenMountainBrush(std::string_view brush_name, std::span<const MountainBrushItem> items) {
	std::string lower(brush_name);
	std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (lower.find("mountain") == std::string::npos) {
		return {};
	}
	if (items.empty()) {
		return "it has no ground items";
	}
	for (const MountainBrushItem& item : items) {
		if (!item.exists) {
			return std::format("item {} does not exist", item.server_id);
		}
		if (!item.ground) {
			return std::format("item {} is not a ground", item.server_id);
		}
	}
	return {};
}
