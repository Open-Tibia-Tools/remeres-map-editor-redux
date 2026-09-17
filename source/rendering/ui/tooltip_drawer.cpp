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

#include "rendering/ui/tooltip_drawer.h"
#include "rendering/core/graphics.h"
#include "rendering/core/normal_image.h"
#include "rendering/core/text_renderer.h"
#include "ui/theme.h"
#include <nanovg.h>
#include <format>
#include "rendering/core/coordinate_mapper.h"
#include <wx/wx.h>
#include "item_definitions/core/item_definition_store.h"


TooltipDrawer::TooltipDrawer() {
}

TooltipDrawer::~TooltipDrawer() {
	clear();
	if (lastContext) {
		for (auto& pair : spriteCache) {
			if (pair.second > 0) {
				nvgDeleteImage(lastContext, pair.second);
			}
		}
		spriteCache.clear();
	}
}

void TooltipDrawer::clear() {
	active_count = 0;
}

TooltipData& TooltipDrawer::requestTooltipData() {
	if (active_count >= tooltips.size()) {
		tooltips.emplace_back();
	}
	TooltipData& data = tooltips[active_count];
	data.clear();
	return data;
}

void TooltipDrawer::commitTooltip() {
	active_count++;
}

void TooltipDrawer::addItemTooltip(const TooltipData& data) {
	if (!data.hasVisibleFields()) {
		return;
	}
	TooltipData& dest = requestTooltipData();
	dest = data;
	commitTooltip();
}

void TooltipDrawer::addItemTooltip(TooltipData&& data) {
	if (!data.hasVisibleFields()) {
		return;
	}
	TooltipData& dest = requestTooltipData();
	dest = std::move(data);
	commitTooltip();
}

void TooltipDrawer::addWaypointTooltip(Position pos, std::string_view name) {
	if (name.empty()) {
		return;
	}
	TooltipData& data = requestTooltipData();
	data.pos = pos;
	data.category = TooltipCategory::WAYPOINT;
	data.waypointName = name;
	commitTooltip();
}

void TooltipDrawer::getHeaderColor(TooltipCategory cat, uint8_t& r, uint8_t& g, uint8_t& b) const {
	wxColour color;
	switch (cat) {
		case TooltipCategory::WAYPOINT:
			color = Theme::Get(Theme::Role::TooltipBorderWaypoint);
			break;
		case TooltipCategory::DOOR:
			color = Theme::Get(Theme::Role::TooltipBorderDoor);
			break;
		case TooltipCategory::TELEPORT:
			color = Theme::Get(Theme::Role::TooltipBorderTeleport);
			break;
		case TooltipCategory::TEXT:
			color = Theme::Get(Theme::Role::TooltipBorderText);
			break;
		case TooltipCategory::ITEM:
		default:
			color = Theme::Get(Theme::Role::TooltipBorderItem);
			break;
	}
	r = color.Red();
	g = color.Green();
	b = color.Blue();
}

int TooltipDrawer::getSpriteImage(NVGcontext* vg, uint16_t itemId) {
	if (itemId == 0) {
		return 0;
	}

	// Detect context change and clear cache
	if (vg != lastContext) {
		// If we had a previous context, we'd ideally delete images from it,
		// but if the context pointer changed, the old one might be invalid.
		// However, adhering to the request to clear cache with nvgDeleteImage:
		if (lastContext) {
			for (auto& pair : spriteCache) {
				if (pair.second > 0) {
					nvgDeleteImage(lastContext, pair.second);
				}
			}
		}
		spriteCache.clear();
		lastContext = vg;
	}

	// Resolve Item ID
	const auto definition = g_item_definitions.get(itemId);
	GameSprite* gameSprite = definition ? g_graphics.getGameSprite(definition.clientId()) : nullptr;
	if (!gameSprite) {
		return 0;
	}

	// We use the item ID as the cache key since it's unique and stable
	auto itCache = spriteCache.find(itemId);
	if (itCache != spriteCache.end()) {
		return itCache->second;
	}

	if (gameSprite && !gameSprite->spriteList.empty()) {
		// Use the first frame/part of the sprite
		NormalImage* img = gameSprite->spriteList[0];
		if (img) {
			const auto dimensions = img->getDimensions();
			std::unique_ptr<uint8_t[]> rgba;

			// For legacy sprites (no transparency), use getRGBData + Magenta Masking
			// This matches how WxWidgets/SpriteIconGenerator renders icons
			if (!g_graphics.hasTransparency()) {
				std::unique_ptr<uint8_t[]> rgb = img->getRGBData();
				if (rgb) {
					rgba = std::make_unique<uint8_t[]>(dimensions.pixelCount() * 4);
					for (size_t i = 0; i < dimensions.pixelCount(); ++i) {
						uint8_t r = rgb[i * 3 + 0];
						uint8_t g = rgb[i * 3 + 1];
						uint8_t b = rgb[i * 3 + 2];

						// Magic Pink (Magenta) is transparent for legacy sprites
						if (r == 0xFF && g == 0x00 && b == 0xFF) {
							rgba[i * 4 + 0] = 0;
							rgba[i * 4 + 1] = 0;
							rgba[i * 4 + 2] = 0;
							rgba[i * 4 + 3] = 0;
						} else {
							rgba[i * 4 + 0] = r;
							rgba[i * 4 + 1] = g;
							rgba[i * 4 + 2] = b;
							rgba[i * 4 + 3] = 255;
						}
					}
				} else {
					// getRGBData failed, should ideally not happen if sprite exists logic is correct
				}
			}

			// Fallback/Standard path for alpha sprites or if RGB failed
			if (!rgba) {
				rgba = img->getRGBAData();
				if (!rgba) {
					// getRGBAData failed
				}
			}

			if (rgba) {
				int image = nvgCreateImageRGBA(vg, dimensions.width, dimensions.height, 0, rgba.get());
				if (image == 0) {
					// nvgCreateImageRGBA failed
				} else {
					// Success
					// Check if we are overwriting an existing valid image (shouldn't happen given find() above, but safest)
					if (spriteCache.contains(itemId) && spriteCache[itemId] > 0) {
						nvgDeleteImage(vg, spriteCache[itemId]);
					}
					spriteCache[itemId] = image;
					return image;
				}
			}
		}
	} else {
		// GameSprite missing or empty list
	}

	return 0;
}

void TooltipDrawer::prepareFields(const TooltipData& tooltip) {
	// Build content lines with word wrapping support
	scratch_fields_count = 0;
	storage.clear();
	if (storage.capacity() < 4096) {
		storage.reserve(4096);
	}

	auto addField = [&](std::string_view label, std::string_view value, Theme::Role colorRole) {
		if (scratch_fields_count >= scratch_fields.size()) {
			scratch_fields.emplace_back();
		}
		FieldLine& field = scratch_fields[scratch_fields_count++];
		field.label = label;
		field.value = value;
		wxColour c = Theme::Get(colorRole);
		field.r = c.Red();
		field.g = c.Green();
		field.b = c.Blue();
		field.wrappedLines.clear();
	};

	if (tooltip.category == TooltipCategory::WAYPOINT) {
		addField("Waypoint", tooltip.waypointName, Theme::Role::TooltipWaypoint);
	} else {
		if (tooltip.actionId > 0) {
			size_t start = storage.size();
			std::format_to(std::back_inserter(storage), "{}", tooltip.actionId);
			addField("Action ID", std::string_view(storage.data() + start, storage.size() - start), Theme::Role::TooltipActionId);
		}
		if (tooltip.uniqueId > 0) {
			size_t start = storage.size();
			std::format_to(std::back_inserter(storage), "{}", tooltip.uniqueId);
			addField("Unique ID", std::string_view(storage.data() + start, storage.size() - start), Theme::Role::TooltipUniqueId);
		}
		if (tooltip.doorId > 0) {
			size_t start = storage.size();
			std::format_to(std::back_inserter(storage), "{}", tooltip.doorId);
			addField("Door ID", std::string_view(storage.data() + start, storage.size() - start), Theme::Role::TooltipDoorId);
		}
		if (tooltip.destination.x > 0) {
			size_t start = storage.size();
			std::format_to(std::back_inserter(storage), "{}, {}, {}", tooltip.destination.x, tooltip.destination.y, tooltip.destination.z);
			addField("Destination", std::string_view(storage.data() + start, storage.size() - start), Theme::Role::TooltipTeleport);
		}
		if (!tooltip.description.empty()) {
			addField("Description", tooltip.description, Theme::Role::TooltipBodyText);
		}
		if (!tooltip.text.empty()) {
			size_t start = storage.size();
			std::format_to(std::back_inserter(storage), "\"{}\"", tooltip.text);
			addField("Text", std::string_view(storage.data() + start, storage.size() - start), Theme::Role::TooltipTextValue);
		}
	}
}

TooltipDrawer::LayoutMetrics TooltipDrawer::calculateLayout(NVGcontext* vg, const TooltipData& tooltip, float maxWidth, float minWidth, float padding, float fontSize) {
	LayoutMetrics lm = {};

	// Set up font for measurements
	nvgFontSize(vg, fontSize);
	nvgFontFace(vg, "sans");

	// Cache fixed label text widths
	static thread_local std::unordered_map<std::string_view, float> s_label_width_cache;
	float maxLabelWidth = 0.0f;
	for (size_t i = 0; i < scratch_fields_count; ++i) {
		const auto& field = scratch_fields[i];
		auto it = s_label_width_cache.find(field.label);
		float lw = 0.0f;
		if (it != s_label_width_cache.end()) {
			lw = it->second;
		} else {
			float labelBounds[4];
			nvgTextBounds(vg, 0, 0, field.label.data(), field.label.data() + field.label.size(), labelBounds);
			lw = labelBounds[2] - labelBounds[0];
			s_label_width_cache[field.label] = lw;
		}
		if (lw > maxLabelWidth) {
			maxLabelWidth = lw;
		}
	}

	lm.valueStartX = maxLabelWidth + 12.0f; // Gap between label and value
	float maxValueWidth = maxWidth - lm.valueStartX - padding * 2;

	// Word wrap values that are too long
	int totalLines = 0;
	float actualMaxWidth = minWidth;

	for (size_t i = 0; i < scratch_fields_count; ++i) {
		auto& field = scratch_fields[i];
		const char* start = field.value.data();
		const char* end = start + field.value.length();

		// Check if value fits on one line
		float valueBounds[4];
		nvgTextBounds(vg, 0, 0, start, end, valueBounds);
		float valueWidth = valueBounds[2] - valueBounds[0];

		if (valueWidth <= maxValueWidth) {
			// Single line
			field.wrappedLines.push_back(field.value);
			totalLines++;
			float lineWidth = lm.valueStartX + valueWidth + padding * 2;
			if (lineWidth > actualMaxWidth) {
				actualMaxWidth = lineWidth;
			}
		} else {
			// Need to wrap - use NanoVG text breaking
			NVGtextRow rows[16];
			int nRows = nvgTextBreakLines(vg, start, end, maxValueWidth, rows, 16);

			for (int j = 0; j < nRows; j++) {
				std::string_view line(rows[j].start, rows[j].end - rows[j].start);
				field.wrappedLines.push_back(line);
				totalLines++;

				float lineWidth = lm.valueStartX + rows[j].width + padding * 2;
				if (lineWidth > actualMaxWidth) {
					actualMaxWidth = lineWidth;
				}
			}

			if (nRows == 0) {
				// Fallback if breaking failed
				field.wrappedLines.push_back(field.value);
				totalLines++;
			}
		}
	}

	// Calculate container grid dimensions
	lm.containerCols = 0;
	lm.containerRows = 0;
	lm.gridSlotSize = 34.0f; // 32px + padding
	lm.containerHeight = 0.0f;

	lm.numContainerItems = static_cast<int>(tooltip.containerItems.size());
	int capacity = static_cast<int>(tooltip.containerCapacity);
	lm.emptyContainerSlots = std::max(0, capacity - lm.numContainerItems);
	lm.totalContainerSlots = lm.numContainerItems;

	if (lm.emptyContainerSlots > 0) {
		lm.totalContainerSlots++; // Add one slot for the summary
	}

	// Apply a hard cap for visual safety
	if (lm.totalContainerSlots > 33) {
		lm.totalContainerSlots = 33;
	}

	if (capacity > 0 || lm.numContainerItems > 0) {
		// Heuristic: try to keep it somewhat square but matching width
		lm.containerCols = std::min(4, lm.totalContainerSlots);
		if (lm.totalContainerSlots > 4) {
			lm.containerCols = 5;
		}
		if (lm.totalContainerSlots > 10) {
			lm.containerCols = 6;
		}
		if (lm.totalContainerSlots > 15) {
			lm.containerCols = 8;
		}

		if (lm.containerCols == 0 && lm.totalContainerSlots > 0) {
			lm.containerCols = 1;
		}

		if (lm.containerCols > 0) {
			lm.containerRows = (lm.totalContainerSlots + lm.containerCols - 1) / lm.containerCols;
			lm.containerHeight = lm.containerRows * lm.gridSlotSize + 4.0f; // + top margin
		}
	}

	// Calculate final box dimensions
	lm.width = std::min(maxWidth + padding * 2, std::max(minWidth, actualMaxWidth));

	bool hasContainer = lm.totalContainerSlots > 0;
	if (hasContainer) {
		float gridWidth = lm.containerCols * lm.gridSlotSize;
		lm.width = std::max(lm.width, gridWidth + padding * 2);
	}

	float lineHeight = fontSize * 1.4f;
	lm.height = totalLines * lineHeight + padding * 2;
	if (hasContainer) {
		lm.height += lm.containerHeight + 4.0f;
	}

	return lm;
}

void TooltipDrawer::drawBackground(NVGcontext* vg, float x, float y, float width, float height, float cornerRadius, const TooltipData& tooltip) {
	// GPU Box Gradient soft drop shadow (replaces 4-pass CPU tessellation loop)
	NVGpaint shadowPaint = nvgBoxGradient(vg, x, y + 2.0f, width, height, cornerRadius * 2.0f, 10.0f, nvgRGBA(0, 0, 0, 120), nvgRGBA(0, 0, 0, 0));
	nvgBeginPath(vg);
	nvgRect(vg, x - 10.0f, y - 8.0f, width + 20.0f, height + 20.0f);
	nvgFillPaint(vg, shadowPaint);
	nvgFill(vg);

	// Get border color based on category
	uint8_t borderR, borderG, borderB;
	getHeaderColor(tooltip.category, borderR, borderG, borderB);

	// Main background and full colored border - single path fill & stroke
	wxColour bgCol = Theme::Get(Theme::Role::TooltipBg);
	nvgBeginPath(vg);
	nvgRoundedRect(vg, x, y, width, height, cornerRadius);
	nvgFillColor(vg, nvgRGBA(bgCol.Red(), bgCol.Green(), bgCol.Blue(), 250));
	nvgFill(vg);
	nvgStrokeColor(vg, nvgRGBA(borderR, borderG, borderB, 255));
	nvgStrokeWidth(vg, 1.0f);
	nvgStroke(vg);
}

void TooltipDrawer::drawFields(NVGcontext* vg, float x, float y, float valueStartX, float lineHeight, float padding, float fontSize) {

	float contentX = x + padding;
	float cursorY = y + padding;

	nvgFontSize(vg, fontSize);
	nvgFontFace(vg, "sans");
	nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

	for (size_t i = 0; i < scratch_fields_count; ++i) {
		const auto& field = scratch_fields[i];
		bool firstLine = true;
		for (const auto& line : field.wrappedLines) {
			if (firstLine) {
				// Draw label - use theme tooltip label color
				wxColour labelCol = Theme::Get(Theme::Role::TooltipLabel);
				nvgFillColor(vg, nvgRGBA(labelCol.Red(), labelCol.Green(), labelCol.Blue(), 200));
				nvgText(vg, contentX, cursorY, field.label.data(), field.label.data() + field.label.size());
				firstLine = false;
			}

			// Draw value line in theme-defined semantic color
			nvgFillColor(vg, nvgRGBA(field.r, field.g, field.b, 255));
			nvgText(vg, contentX + valueStartX, cursorY, line.data(), line.data() + line.size());

			cursorY += lineHeight;
		}
	}
}

void TooltipDrawer::drawContainerGrid(NVGcontext* vg, float x, float y, const TooltipData& tooltip, const LayoutMetrics& layout) {

	if (layout.totalContainerSlots <= 0) {
		return;
	}

	// Calculate cursorY after text fields
	float fontSize = 11.0f;
	float lineHeight = fontSize * 1.4f;
	float textBlockHeight = 0.0f;
	for (size_t i = 0; i < scratch_fields_count; ++i) {
		const auto& field = scratch_fields[i];
		textBlockHeight += field.wrappedLines.size() * lineHeight;
	}

	float startY = y + 10.0f + textBlockHeight + 8.0f; // Padding + Text + Spacer
	float startX = x + 10.0f; // Padding

	for (int idx = 0; idx < layout.totalContainerSlots; ++idx) {
		int col = idx % layout.containerCols;
		int row = idx / layout.containerCols;

		float itemX = startX + col * layout.gridSlotSize;
		float itemY = startY + row * layout.gridSlotSize;

		// Draw slot background with rounded rect and subtle border
		nvgBeginPath(vg);
		nvgRoundedRect(vg, itemX, itemY, 32.0f, 32.0f, 2.0f);
		wxColour baseCol = Theme::Get(Theme::Role::CardBase);
		wxColour borderCol = Theme::Get(Theme::Role::Border);

		nvgFillColor(vg, nvgRGBA(baseCol.Red(), baseCol.Green(), baseCol.Blue(), 100)); // Dark slot placeholder
		nvgFill(vg);
		nvgStrokeColor(vg, nvgRGBA(borderCol.Red(), borderCol.Green(), borderCol.Blue(), 100)); // Light border
		nvgStrokeWidth(vg, 1.0f);
		nvgStroke(vg);

		// Check if this is the summary info slot (last slot if we have empty spaces)
		bool isSummarySlot = (layout.emptyContainerSlots > 0 && idx == layout.totalContainerSlots - 1);

		if (isSummarySlot) {
			// Draw empty slots count: "+N"
			std::string summary = "+" + std::to_string(layout.emptyContainerSlots);

			nvgFontSize(vg, 12.0f);
			nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

			// Text shadow
			nvgFillColor(vg, nvgRGBA(0, 0, 0, 255));
			nvgText(vg, itemX + 17, itemY + 17, summary.c_str(), nullptr);

			// Text
			wxColour countCol = Theme::Get(Theme::Role::TooltipCountText);
			nvgFillColor(vg, nvgRGBA(countCol.Red(), countCol.Green(), countCol.Blue(), 255));
			nvgText(vg, itemX + 16, itemY + 16, summary.c_str(), nullptr);

		} else if (idx < layout.numContainerItems) {
			// Draw Actual Item
			const auto& item = tooltip.containerItems[idx];

			// Draw item sprite
			int img = getSpriteImage(vg, item.id);
			if (img > 0) {
				nvgBeginPath(vg);
				nvgRect(vg, itemX, itemY, 32, 32);
				nvgFillPaint(vg, nvgImagePattern(vg, itemX, itemY, 32, 32, 0, img, 1.0f));
				nvgFill(vg);
			}

			// Draw Count
			if (item.count > 1) {
				std::string countStr = std::to_string(item.count);
				nvgFontSize(vg, 10.0f);
				nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM);

				// Text shadow
				nvgFillColor(vg, nvgRGBA(0, 0, 0, 255));
				nvgText(vg, itemX + 33, itemY + 33, countStr.c_str(), nullptr);

				// Text
				wxColour countCol = Theme::Get(Theme::Role::TooltipCountText);
				nvgFillColor(vg, nvgRGBA(countCol.Red(), countCol.Green(), countCol.Blue(), 255));
				nvgText(vg, itemX + 32, itemY + 32, countStr.c_str(), nullptr);
			}
		}
	}
}

void TooltipDrawer::draw(NVGcontext* vg, const RenderView& view) {
	if (!vg || active_count == 0) {
		return;
	}

	const float zoom = view.zoom < 0.01f ? 1.0f : view.zoom;
	const float inv_zoom = 1.0f / zoom;
	const float tile_size_screen = 32.0f * inv_zoom;

	// Zoom LOD guard: do not draw micro tooltips when zoomed beyond 10% zoom (matching editor LOD policy)
	if (view.zoom > 10.0f) {
		return;
	}

	const float screen_max_x = static_cast<float>(view.screensize_x) + 150.0f;
	const float screen_max_y = static_cast<float>(view.screensize_y) + 300.0f;

	for (size_t i = 0; i < active_count; ++i) {
		const auto& tooltip = tooltips[i];

		int unscaled_x, unscaled_y;
		if (!view.IsTileVisible(tooltip.pos.x, tooltip.pos.y, tooltip.pos.z, unscaled_x, unscaled_y)) {
			continue;
		}

		float screen_x = static_cast<float>(unscaled_x) * inv_zoom;
		float screen_y = static_cast<float>(unscaled_y) * inv_zoom;

		// Center on tile
		screen_x += tile_size_screen * 0.5f;
		screen_y += tile_size_screen * 0.5f;

		// Strict frustum culling before measuring/layout
		if (screen_x < -150.0f || screen_x > screen_max_x || screen_y < -50.0f || screen_y > screen_max_y) {
			continue;
		}

		// Constants
		constexpr float fontSize = 11.0f;
		constexpr float padding = 10.0f;
		constexpr float minWidth = 120.0f;
		constexpr float maxWidth = 220.0f;

		// 1. Prepare Content
		prepareFields(tooltip);

		// Skip if nothing to show
		if (scratch_fields_count == 0 && tooltip.containerItems.empty()) {
			continue;
		}

		// 2. Calculate Layout
		LayoutMetrics layout = calculateLayout(vg, tooltip, maxWidth, minWidth, padding, fontSize);

		// Position tooltip above tile
		float tooltipX = screen_x - (layout.width * 0.5f);
		float tooltipY = screen_y - layout.height - 12.0f;

		// 3. Draw Background & Shadow
		drawBackground(vg, tooltipX, tooltipY, layout.width, layout.height, 4.0f, tooltip);

		// 4. Draw Text Fields
		drawFields(vg, tooltipX, tooltipY, layout.valueStartX, fontSize * 1.4f, padding, fontSize);

		// 5. Draw Container Grid
		drawContainerGrid(vg, tooltipX, tooltipY, tooltip, layout);
	}
}

