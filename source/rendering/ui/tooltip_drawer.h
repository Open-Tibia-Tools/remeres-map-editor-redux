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

#ifndef RME_TOOLTIP_DRAWER_H_
#define RME_TOOLTIP_DRAWER_H_

#include "app/definitions.h"
#include "map/position.h"
#include "rendering/core/render_view.h"
#include "rendering/indicators/technical_item_registry.h"
#include <glm/vec3.hpp>
#include <vector>
#include <string>
#include <string_view>
#include <unordered_map>

class Item;
class Waypoint;
struct NVGcontext;

struct ContainerItem {
	uint16_t id = 0;
	uint8_t count = 0;
	uint8_t subtype = 0;
};

// Tooltip category determines header border accent and icon badge
enum class TooltipCategory {
	ITEM,      // Dark charcoal - generic item
	DOOR,      // Saddle Brown - door with door ID
	TELEPORT,  // Purple - teleporter
	TEXT,      // Gold - readable text (signs, books)
	WAYPOINT,  // Vivid Blue - landmark
	HOUSE,     // Emerald Green / Orange - house / guildhall
	ZONE       // Dynamic Zone RGB - zone area
};

// Structured tooltip data for card-based rendering
struct TooltipData {
	Position pos;
	TooltipCategory category = TooltipCategory::ITEM;

	// Header info
	uint16_t itemId = 0;
	uint16_t clientId = 0;
	std::string itemName;
	uint16_t count = 0;
	std::string subtitle;

	// Indicator badge (for houses, zones, waypoints, towns)
	rme::rendering::TileIndicatorType badgeType = rme::rendering::TileIndicatorType::None;

	// Optional fields (0 or empty = not shown)
	uint16_t actionId = 0;
	uint16_t uniqueId = 0;
	uint8_t doorId = 0;
	uint8_t depotId = 0;
	std::string townName;
	std::string text;
	std::string description;
	Position destination; // For teleports (check if valid via destination.x > 0)

	// Waypoint-specific
	std::string waypointName;

	// House-specific
	uint32_t houseId = 0;
	uint32_t houseRent = 0;
	size_t houseSize = 0;
	bool isGuildhall = false;

	// Zone-specific
	uint16_t zoneId = 0;
	glm::vec3 zoneRgb = glm::vec3(0.0f);
	size_t zoneTileCount = 0;
	size_t zoneAreaCount = 0;
	std::string zoneFlags;

	// Container contents
	std::vector<ContainerItem> containerItems;
	uint8_t containerCapacity = 0;

	TooltipData() = default;

	// Constructor for waypoint
	TooltipData(Position p, std::string_view wpName) :
		pos(p), category(TooltipCategory::WAYPOINT), itemName(wpName), waypointName(wpName),
		badgeType(rme::rendering::TileIndicatorType::Waypoint) { }

	// Constructor for item
	TooltipData(Position p, uint16_t id, std::string_view name) :
		pos(p), category(TooltipCategory::ITEM), itemId(id), itemName(name) { }

	// Determine category based on fields
	void updateCategory() {
		if (houseId > 0) {
			category = TooltipCategory::HOUSE;
		} else if (zoneId > 0) {
			category = TooltipCategory::ZONE;
		} else if (!waypointName.empty()) {
			category = TooltipCategory::WAYPOINT;
		} else if (destination.x > 0) {
			category = TooltipCategory::TELEPORT;
		} else if (doorId > 0) {
			category = TooltipCategory::DOOR;
		} else if (!text.empty()) {
			category = TooltipCategory::TEXT;
		} else {
			category = TooltipCategory::ITEM;
		}
	}

	// Check if this tooltip has any visible fields
	bool hasVisibleFields() const {
		return houseId > 0 || zoneId > 0 || !waypointName.empty() || actionId > 0 ||
		       uniqueId > 0 || doorId > 0 || depotId > 0 || !text.empty() ||
		       !description.empty() || destination.x > 0 || !containerItems.empty() || count > 1;
	}

	void clear() {
		pos = Position();
		category = TooltipCategory::ITEM;
		itemId = 0;
		clientId = 0;
		itemName.clear();
		count = 0;
		subtitle.clear();
		badgeType = rme::rendering::TileIndicatorType::None;

		actionId = 0;
		uniqueId = 0;
		doorId = 0;
		depotId = 0;
		townName.clear();
		text.clear();
		description.clear();
		destination = Position();
		waypointName.clear();

		houseId = 0;
		houseRent = 0;
		houseSize = 0;
		isGuildhall = false;

		zoneId = 0;
		zoneRgb = glm::vec3(0.0f);
		zoneTileCount = 0;
		zoneAreaCount = 0;
		zoneFlags.clear();

		containerItems.clear();
		containerCapacity = 0;
	}
};

class TooltipDrawer {
public:
	TooltipDrawer();
	~TooltipDrawer();

	// Add a structured tooltip for an item
	void addItemTooltip(const TooltipData& data);
	void addItemTooltip(TooltipData&& data);

	// Request a tooltip object from the pool. Call commitTooltip() to finalize.
	TooltipData& requestTooltipData();
	void commitTooltip();

	// Add a waypoint tooltip
	void addWaypointTooltip(Position pos, std::string_view name);

	// Draw all tooltips
	void draw(NVGcontext* vg, const RenderView& view);

	// Clear all tooltips
	void clear();

	bool empty() const noexcept {
		return active_count == 0;
	}

protected:
	struct FieldLine {
		std::string_view label;
		std::string_view value;
		uint8_t r = 255;
		uint8_t g = 255;
		uint8_t b = 255;
		std::vector<std::string_view> wrappedLines; // For multi-line values
	};
	std::vector<FieldLine> scratch_fields;
	size_t scratch_fields_count = 0;
	std::vector<std::string_view> scratch_text_lines; // For sunken text box
	std::string storage; // Scratch buffer for text generation
	std::string sanitized_text; // Scratch buffer for CRLF stripped text

	std::vector<TooltipData> tooltips;
	size_t active_count = 0;

	std::unordered_map<uint32_t, int> spriteCache; // itemId -> nvg image handle
	NVGcontext* lastContext = nullptr;

	// Helper to get or load sprite image
	int getSpriteImage(NVGcontext* vg, uint16_t itemId);

	// Helper to get header border color based on category and tooltip data
	void getHeaderColor(const TooltipData& tooltip, uint8_t& r, uint8_t& g, uint8_t& b) const;

	// Layout metrics
	struct LayoutMetrics {
		float width = 0.0f;
		float height = 0.0f;
		float headerHeight = 0.0f;
		float valueStartX = 0.0f;
		float fieldsHeight = 0.0f;
		float textBoxHeight = 0.0f;
		float gridSlotSize = 34.0f;
		int containerCols = 0;
		int containerRows = 0;
		float containerHeight = 0.0f;
		int totalContainerSlots = 0;
		int emptyContainerSlots = 0;
		int numContainerItems = 0;
	};

	void prepareFields(const TooltipData& tooltip);
	LayoutMetrics calculateLayout(NVGcontext* vg, const TooltipData& tooltip, float maxWidth, float minWidth, float padding, float fontSize);

	// Classic Tibia window drawing helpers
	void drawWindowFrame(NVGcontext* vg, float x, float y, float width, float height, const TooltipData& tooltip);
	void drawSunkenSlot(NVGcontext* vg, float x, float y, float size, const TooltipData& tooltip);
	void drawSunkenDivider(NVGcontext* vg, float x, float y, float width);
	void drawHeader(NVGcontext* vg, float x, float y, float width, const TooltipData& tooltip, const LayoutMetrics& layout, float padding);
	void drawFields(NVGcontext* vg, float x, float y, float valueStartX, float lineHeight, float padding, float fontSize);
	void drawSunkenTextBox(NVGcontext* vg, float x, float y, float width, float height, float lineHeight, float padding, float fontSize);
	void drawContainerGrid(NVGcontext* vg, float x, float y, const TooltipData& tooltip, const LayoutMetrics& layout, float padding);
};

#endif // RME_TOOLTIP_DRAWER_H_
