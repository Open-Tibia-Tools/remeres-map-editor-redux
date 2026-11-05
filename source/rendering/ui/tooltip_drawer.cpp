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
#include "rendering/indicators/indicator_drawing_utils.h"
#include "ui/theme.h"
#include <nanovg.h>
#include <format>
#include <charconv>
#include "rendering/core/coordinate_mapper.h"
#include <wx/wx.h>
#include "item_definitions/core/item_definition_store.h"
#include "util/nvg_utils.h"
#include <algorithm>

namespace {
	inline NVGcolor ThemeNvColor(Theme::Role role, uint8_t alpha = 255) {
		const wxColour c = Theme::Get(role);
		return nvgRGBA(c.Red(), c.Green(), c.Blue(), alpha);
	}

	void AppendNumberWithCommas(uint64_t value, std::string& out) {
		char buf[32];
		auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), value);
		if (ec != std::errc()) {
			return;
		}
		const int len = static_cast<int>(ptr - buf);
		int first_group = len % 3;
		if (first_group == 0) {
			first_group = 3;
		}
		int i = 0;
		for (; i < first_group; ++i) {
			out.push_back(buf[i]);
		}
		for (; i < len; i += 3) {
			out.push_back(',');
			out.push_back(buf[i]);
			out.push_back(buf[i + 1]);
			out.push_back(buf[i + 2]);
		}
	}
} // namespace

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
	data.itemName = name;
	data.badgeType = rme::rendering::TileIndicatorType::Waypoint;
	commitTooltip();
}

NVGcolor TooltipDrawer::getHeaderColor(const TooltipData& tooltip) const {
	Theme::Role role = Theme::Role::TooltipBorderItem;
	switch (tooltip.category) {
		case TooltipCategory::WAYPOINT:
			role = Theme::Role::TooltipBorderWaypoint;
			break;
		case TooltipCategory::DOOR:
			role = Theme::Role::TooltipBorderDoor;
			break;
		case TooltipCategory::TELEPORT:
			role = Theme::Role::TooltipBorderTeleport;
			break;
		case TooltipCategory::TEXT:
			role = Theme::Role::TooltipBorderText;
			break;
		case TooltipCategory::HOUSE:
			role = Theme::Role::TooltipBorderHouse;
			break;
		case TooltipCategory::ZONE:
			role = Theme::Role::TooltipBorderZone;
			break;
		case TooltipCategory::ITEM:
		default:
			role = Theme::Role::TooltipBorderItem;
			break;
	}
	return ThemeNvColor(role);
}

int TooltipDrawer::getSpriteImage(NVGcontext* vg, uint16_t itemId) {
	if (itemId == 0) {
		return 0;
	}

	// Detect context change and clear cache
	if (vg != lastContext) {
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

	auto itCache = spriteCache.find(itemId);
	if (itCache != spriteCache.end()) {
		return itCache->second;
	}

	int image = NvgUtils::CreateItemTexture(vg, itemId);
	spriteCache[itemId] = image;
	return image;
}

void TooltipDrawer::prepareFields(const TooltipData& tooltip) {
	scratch_fields_count = 0;
	scratch_text_lines.clear();
	sanitized_text.clear();
	storage.clear();
	if (storage.capacity() < 4096) {
		storage.reserve(4096);
	}

	// 1. Format Title (zero-allocation into storage scratch buffer)
	const size_t titleStart = storage.size();
	if (tooltip.count > 1) {
		std::format_to(std::back_inserter(storage), "{} • {}", !tooltip.itemName.empty() ? tooltip.itemName : "Item", tooltip.count);
	} else {
		std::format_to(std::back_inserter(storage), "{}", !tooltip.itemName.empty() ? tooltip.itemName : "Object");
	}
	const size_t titleLen = storage.size() - titleStart;
	storage.push_back('\0'); // Null terminator for NanoVG C-string APIs

	// 2. Format Subtitle
	const size_t subStart = storage.size();
	if (!tooltip.subtitle.empty()) {
		std::format_to(std::back_inserter(storage), "{}", tooltip.subtitle);
	} else if (!tooltip.townName.empty()) {
		std::format_to(std::back_inserter(storage), "Town: {}", tooltip.townName);
	} else if (tooltip.category == TooltipCategory::WAYPOINT) {
		std::format_to(std::back_inserter(storage), "Waypoint");
	}
	const size_t subLen = storage.size() - subStart;
	storage.push_back('\0');

	if (!tooltip.text.empty()) {
		sanitized_text.reserve(tooltip.text.size());
		for (size_t i = 0; i < tooltip.text.size(); ++i) {
			char c = tooltip.text[i];
			if (c == '\r') {
				// Strip '\r'. If followed by '\n', let '\n' handle the newline.
				if (i + 1 < tooltip.text.size() && tooltip.text[i + 1] == '\n') {
					continue;
				}
				sanitized_text.push_back('\n');
			} else {
				sanitized_text.push_back(c);
			}
		}
	}

	auto addStorageField = [&](std::string_view label, size_t start, Theme::Role colorRole) {
		if (scratch_fields_count >= scratch_fields.size()) {
			scratch_fields.emplace_back();
		}
		FieldLine& field = scratch_fields[scratch_fields_count++];
		field.label = label;
		field.valOffset = start;
		field.valLength = storage.size() - start;
		field.fromStorage = true;
		wxColour c = Theme::Get(colorRole);
		field.r = c.Red();
		field.g = c.Green();
		field.b = c.Blue();
		field.wrappedLines.clear();
	};

	auto addDirectField = [&](std::string_view label, std::string_view value, Theme::Role colorRole) {
		if (scratch_fields_count >= scratch_fields.size()) {
			scratch_fields.emplace_back();
		}
		FieldLine& field = scratch_fields[scratch_fields_count++];
		field.label = label;
		field.value = value;
		field.fromStorage = false;
		wxColour c = Theme::Get(colorRole);
		field.r = c.Red();
		field.g = c.Green();
		field.b = c.Blue();
		field.wrappedLines.clear();
	};

	if (tooltip.category == TooltipCategory::HOUSE) {
		size_t start = storage.size();
		std::format_to(std::back_inserter(storage), "{}", tooltip.houseId);
		addStorageField("House ID", start, Theme::Role::TooltipUniqueId);

		if (tooltip.doorId > 0) {
			start = storage.size();
			std::format_to(std::back_inserter(storage), "{}", tooltip.doorId);
			addStorageField("Door ID", start, Theme::Role::TooltipDoorId);
		}

		if (tooltip.actionId > 0) {
			start = storage.size();
			std::format_to(std::back_inserter(storage), "{}", tooltip.actionId);
			addStorageField("Action ID", start, Theme::Role::TooltipActionId);
		}

		if (tooltip.uniqueId > 0) {
			start = storage.size();
			std::format_to(std::back_inserter(storage), "{}", tooltip.uniqueId);
			addStorageField("Unique ID", start, Theme::Role::TooltipUniqueId);
		}

		start = storage.size();
		std::format_to(std::back_inserter(storage), "{} sqm", tooltip.houseSize);
		addStorageField("Tile Count", start, Theme::Role::TooltipDoorId);

		if (tooltip.houseRent > 0) {
			start = storage.size();
			AppendNumberWithCommas(tooltip.houseRent, storage);
			storage.append(" gp");
			addStorageField("Rent", start, Theme::Role::TooltipTextValue);
		}

		if (tooltip.destination.x > 0) {
			start = storage.size();
			std::format_to(std::back_inserter(storage), "{}, {}, {}", tooltip.destination.x, tooltip.destination.y, tooltip.destination.z);
			addStorageField("Destination", start, Theme::Role::TooltipTeleport);
		}

		if (!tooltip.description.empty()) {
			addDirectField("Description", tooltip.description, Theme::Role::TooltipBodyText);
		}
	} else if (tooltip.category == TooltipCategory::ZONE) {
		size_t start = storage.size();
		std::format_to(std::back_inserter(storage), "Area #{}", tooltip.zoneAreaIndex + 1);
		addStorageField("Area", start, Theme::Role::TooltipTextValue);

		start = storage.size();
		std::format_to(std::back_inserter(storage), "{} sqm", tooltip.zoneAreaTileCount);
		addStorageField("Tile Count", start, Theme::Role::TooltipTextValue);

		if (tooltip.zoneAreaCount > 1) {
			start = storage.size();
			std::format_to(std::back_inserter(storage), "{} sqm ({} areas)", tooltip.zoneTileCount, tooltip.zoneAreaCount);
			addStorageField("Total", start, Theme::Role::TooltipTextValue);
		}

		if (!tooltip.zoneFlags.empty()) {
			addDirectField("Flags", tooltip.zoneFlags, Theme::Role::TooltipTextValue);
		}
	} else if (tooltip.category == TooltipCategory::WAYPOINT) {
		size_t start = storage.size();
		std::format_to(std::back_inserter(storage), "{}, {}, {}", tooltip.pos.x, tooltip.pos.y, tooltip.pos.z);
		addStorageField("Position", start, Theme::Role::TooltipTextValue);
	} else {
		if (tooltip.houseId > 0) {
			size_t start = storage.size();
			std::format_to(std::back_inserter(storage), "{}", tooltip.houseId);
			addStorageField("House ID", start, Theme::Role::TooltipUniqueId);
		}
		if (tooltip.actionId > 0) {
			size_t start = storage.size();
			std::format_to(std::back_inserter(storage), "{}", tooltip.actionId);
			addStorageField("Action ID", start, Theme::Role::TooltipActionId);
		}
		if (tooltip.uniqueId > 0) {
			size_t start = storage.size();
			std::format_to(std::back_inserter(storage), "{}", tooltip.uniqueId);
			addStorageField("Unique ID", start, Theme::Role::TooltipUniqueId);
		}
		if (tooltip.doorId > 0) {
			size_t start = storage.size();
			std::format_to(std::back_inserter(storage), "{}", tooltip.doorId);
			addStorageField("Door ID", start, Theme::Role::TooltipDoorId);
		}
		if (tooltip.depotId > 0) {
			size_t start = storage.size();
			if (!tooltip.townName.empty()) {
				std::format_to(std::back_inserter(storage), "{} ({})", tooltip.depotId, tooltip.townName);
			} else {
				std::format_to(std::back_inserter(storage), "{}", tooltip.depotId);
			}
			addStorageField("Depot ID", start, Theme::Role::TooltipDepotId);
		}
		if (tooltip.houseSize > 0) {
			size_t start = storage.size();
			std::format_to(std::back_inserter(storage), "{} sqm", tooltip.houseSize);
			addStorageField("Tile Count", start, Theme::Role::TooltipDoorId);
		}
		if (tooltip.houseRent > 0) {
			size_t start = storage.size();
			AppendNumberWithCommas(tooltip.houseRent, storage);
			storage.append(" gp");
			addStorageField("Rent", start, Theme::Role::TooltipTextValue);
		}
		if (tooltip.destination.x > 0) {
			size_t start = storage.size();
			std::format_to(std::back_inserter(storage), "{}, {}, {}", tooltip.destination.x, tooltip.destination.y, tooltip.destination.z);
			addStorageField("Destination", start, Theme::Role::TooltipTeleport);
		}
		if (!tooltip.description.empty()) {
			addDirectField("Description", tooltip.description, Theme::Role::TooltipBodyText);
		}
	}

	// 3. Materialize all views pointing into storage after all appends have completed
	scratch_title = std::string_view(storage.data() + titleStart, titleLen);
	scratch_subtitle = (subLen > 0) ? std::string_view(storage.data() + subStart, subLen) : std::string_view{};
	for (size_t i = 0; i < scratch_fields_count; ++i) {
		auto& field = scratch_fields[i];
		if (field.fromStorage) {
			field.value = std::string_view(storage.data() + field.valOffset, field.valLength);
		}
	}
}

TooltipDrawer::LayoutMetrics TooltipDrawer::calculateLayout(NVGcontext* vg, const TooltipData& tooltip, float maxWidth, float minWidth, float padding, float fontSize) {
	LayoutMetrics lm = {};

	// 1. Measure Header
	constexpr float kSlotSize = 32.0f;
	constexpr float kHeaderGap = 8.0f;
	lm.headerHeight = kSlotSize + padding * 2.0f;

	nvgFontSize(vg, 12.0f);
	nvgFontFace(vg, "sans");
	float nameBounds[4] = { 0 };
	nvgTextBounds(vg, 0, 0, scratch_title.data(), nullptr, nameBounds);
	float nameWidth = nameBounds[2] - nameBounds[0];

	// Subtitle width
	float subWidth = 0.0f;
	if (!scratch_subtitle.empty()) {
		nvgFontSize(vg, 10.0f);
		float subBounds[4] = { 0 };
		nvgTextBounds(vg, 0, 0, scratch_subtitle.data(), nullptr, subBounds);
		subWidth = subBounds[2] - subBounds[0];
	}

	float headerContentWidth = padding + kSlotSize + kHeaderGap + std::max(nameWidth, subWidth) + padding;

	// 2. Measure Fields
	nvgFontSize(vg, fontSize);
	nvgFontFace(vg, "sans");

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

	lm.valueStartX = maxLabelWidth + 12.0f;
	float maxValueWidth = maxWidth - lm.valueStartX - padding * 2.0f;

	int totalFieldLines = 0;
	float actualFieldsMaxWidth = minWidth;

	for (size_t i = 0; i < scratch_fields_count; ++i) {
		auto& field = scratch_fields[i];
		const char* start = field.value.data();
		const char* end = start + field.value.length();

		float valueBounds[4];
		nvgTextBounds(vg, 0, 0, start, end, valueBounds);
		float valueWidth = valueBounds[2] - valueBounds[0];

		if (valueWidth <= maxValueWidth) {
			field.wrappedLines.push_back(field.value);
			totalFieldLines++;
			float lineWidth = lm.valueStartX + valueWidth + padding * 2.0f;
			if (lineWidth > actualFieldsMaxWidth) {
				actualFieldsMaxWidth = lineWidth;
			}
		} else {
			NVGtextRow rows[16];
			constexpr size_t kMaxFieldLines = 8;
			const char* cur = start;
			bool fieldTruncated = false;

			while (cur < end && !fieldTruncated) {
				int nRows = nvgTextBreakLines(vg, cur, end, maxValueWidth, rows, 16);
				if (nRows == 0) {
					break;
				}
				for (int j = 0; j < nRows; ++j) {
					if (field.wrappedLines.size() >= kMaxFieldLines) {
						fieldTruncated = true;
						break;
					}
					field.wrappedLines.push_back(std::string_view(rows[j].start, rows[j].end - rows[j].start));
					totalFieldLines++;
					float lineWidth = lm.valueStartX + rows[j].width + padding * 2.0f;
					if (lineWidth > actualFieldsMaxWidth) {
						actualFieldsMaxWidth = lineWidth;
					}
					cur = rows[j].next;
				}
			}

			if (fieldTruncated) {
				field.wrappedLines.push_back(std::string_view("[...]"));
				totalFieldLines++;
			} else if (field.wrappedLines.empty()) {
				field.wrappedLines.push_back(field.value);
				totalFieldLines++;
			}
		}
	}

	const float lineHeight = fontSize * 1.35f;
	lm.fieldsHeight = totalFieldLines * lineHeight;

	// 3. Measure Container Grid
	lm.containerCols = 0;
	lm.containerRows = 0;
	lm.gridSlotSize = 34.0f; // 32px + 2px gutter
	lm.containerHeight = 0.0f;

	lm.numContainerItems = static_cast<int>(tooltip.containerItems.size());
	int capacity = static_cast<int>(tooltip.containerCapacity);
	lm.emptyContainerSlots = std::max(0, capacity - lm.numContainerItems);
	lm.totalContainerSlots = lm.numContainerItems;

	if (lm.emptyContainerSlots > 0) {
		lm.totalContainerSlots++;
	}
	if (lm.totalContainerSlots > 33) {
		lm.totalContainerSlots = 33;
	}

	if (capacity > 0 || lm.numContainerItems > 0) {
		constexpr int kMaxCols = 5;
		lm.containerCols = std::clamp(lm.totalContainerSlots, 1, kMaxCols);

		if (lm.containerCols > 0) {
			lm.containerRows = (lm.totalContainerSlots + lm.containerCols - 1) / lm.containerCols;
			lm.containerHeight = lm.containerRows * lm.gridSlotSize + 2.0f;
		}
	}

	// 4. Calculate Card Width
	float minTargetWidth = minWidth;
	if (!sanitized_text.empty()) {
		minTargetWidth = std::max(minTargetWidth, 220.0f);
	}
	float gridWidth = (lm.containerCols > 0) ? (lm.containerCols * lm.gridSlotSize + padding * 2.0f) : 0.0f;
	float effectiveMaxWidth = std::max(maxWidth, gridWidth);
	lm.width = std::clamp(std::max({ minTargetWidth, headerContentWidth, actualFieldsMaxWidth, gridWidth }), minWidth, effectiveMaxWidth);

	// 5. Measure Readable Text Box against exact well inner width
	lm.textBoxHeight = 0.0f;
	if (!sanitized_text.empty()) {
		nvgFontSize(vg, 11.0f);
		nvgFontFace(vg, "sans");
		const float wellInnerWidth = (lm.width - padding * 2.0f) - 14.0f;
		constexpr size_t kMaxTextLines = 12;
		bool textTruncated = false;

		size_t pStart = 0;
		while (pStart < sanitized_text.size() && !textTruncated) {
			size_t pEnd = sanitized_text.find('\n', pStart);
			if (pEnd == std::string::npos) {
				pEnd = sanitized_text.size();
			}

			if (pStart == pEnd) {
				// Empty line / paragraph break
				if (scratch_text_lines.size() < kMaxTextLines) {
					scratch_text_lines.push_back(std::string_view{});
				} else {
					textTruncated = true;
				}
			} else {
				const char* segStart = sanitized_text.data() + pStart;
				const char* segEnd = sanitized_text.data() + pEnd;

				float bounds[4];
				nvgTextBounds(vg, 0, 0, segStart, segEnd, bounds);
				float segWidth = bounds[2] - bounds[0];

				if (segWidth <= wellInnerWidth) {
					if (scratch_text_lines.size() < kMaxTextLines) {
						scratch_text_lines.push_back(std::string_view(segStart, segEnd - segStart));
					} else {
						textTruncated = true;
					}
				} else {
					NVGtextRow rows[16];
					const char* cur = segStart;
					while (cur < segEnd && !textTruncated) {
					int nRows = nvgTextBreakLines(vg, cur, segEnd, wellInnerWidth, rows, 16);
					if (nRows == 0) {
						break;
					}
					for (int r = 0; r < nRows; ++r) {
						if (scratch_text_lines.size() >= kMaxTextLines) {
							textTruncated = true;
							break;
						}
						// If row width still exceeds wellInnerWidth (e.g. unbroken string like "asdasdasd..."),
						// break character by character (advancing by full UTF-8 codepoints)
						if (rows[r].width > wellInnerWidth) {
							const char* cStart = rows[r].start;
							const char* cEnd = rows[r].end;
							const char* charCur = cStart;
							const char* lineStart = charCur;
							while (charCur < cEnd && !textTruncated) {
								// Advance charCur to end of current UTF-8 code point
								const char* nextChar = charCur + 1;
								while (nextChar < cEnd && (static_cast<unsigned char>(*nextChar) & 0xC0) == 0x80) {
									++nextChar;
								}

								float cBounds[4];
								nvgTextBounds(vg, 0, 0, lineStart, nextChar, cBounds);
								if (cBounds[2] - cBounds[0] > wellInnerWidth && charCur > lineStart) {
									scratch_text_lines.push_back(std::string_view(lineStart, charCur - lineStart));
									lineStart = charCur;
									if (scratch_text_lines.size() >= kMaxTextLines) {
										textTruncated = true;
										break;
									}
								}
								charCur = nextChar;
							}
							if (!textTruncated && lineStart < cEnd) {
								if (scratch_text_lines.size() < kMaxTextLines) {
									scratch_text_lines.push_back(std::string_view(lineStart, cEnd - lineStart));
								} else {
									textTruncated = true;
								}
							}
						} else {
							scratch_text_lines.push_back(std::string_view(rows[r].start, rows[r].end - rows[r].start));
						}
						cur = rows[r].next;
					}
				}
			}
		}

			if (textTruncated) {
				break;
			}
			pStart = pEnd + 1;
		}

		if (textTruncated) {
			scratch_text_lines.push_back(std::string_view("[...]"));
		}

		if (scratch_text_lines.empty()) {
			scratch_text_lines.push_back(sanitized_text);
		}
		lm.textBoxHeight = scratch_text_lines.size() * (11.0f * 1.35f) + 12.0f; // 6px padding top/bottom
	}

	// 6. Calculate Final Box Dimensions
	lm.height = lm.headerHeight;
	if (scratch_fields_count > 0) {
		lm.height += 2.0f + lm.fieldsHeight + 8.0f; // 2px divider + fields + spacer
	}
	if (!scratch_text_lines.empty()) {
		lm.height += 2.0f + lm.textBoxHeight + 8.0f; // 2px divider + text box + spacer
	}
	if (lm.totalContainerSlots > 0) {
		lm.height += 2.0f + lm.containerHeight + 8.0f; // 2px divider + container grid + spacer
	}
	lm.height += 4.0f; // Bottom subtle padding

	return lm;
}

void TooltipDrawer::drawWindowFrame(NVGcontext* vg, float x, float y, float width, float height, const TooltipData& tooltip) {
	// 1. Soft GPU box gradient drop shadow (0.0f corner radius for square window)
	NVGpaint shadowPaint = nvgBoxGradient(vg, x, y + 2.0f, width, height, 0.0f, 10.0f, nvgRGBA(0, 0, 0, 140), nvgRGBA(0, 0, 0, 0));
	nvgBeginPath(vg);
	nvgRect(vg, x - 10.0f, y - 8.0f, width + 20.0f, height + 20.0f);
	nvgFillPaint(vg, shadowPaint);
	nvgFill(vg);

	// 2. Outer 1px solid border
	nvgBeginPath(vg);
	nvgRect(vg, x, y, width, height);
	nvgFillColor(vg, ThemeNvColor(Theme::Role::TooltipWindowOutline));
	nvgFill(vg);

	// 3. Category-colored 1px square border
	nvgBeginPath(vg);
	nvgRect(vg, x + 1.0f, y + 1.0f, width - 2.0f, height - 2.0f);
	nvgStrokeColor(vg, getHeaderColor(tooltip));
	nvgStrokeWidth(vg, 1.0f);
	nvgStroke(vg);

	// 4. Interior Well Background Fill
	nvgBeginPath(vg);
	nvgRect(vg, x + 2.0f, y + 2.0f, width - 4.0f, height - 4.0f);
	nvgFillColor(vg, ThemeNvColor(Theme::Role::TooltipWindowBg, 252));
	nvgFill(vg);

	// 5. Classic Inset Outer Sunken Groove (1px shadow top/left, 1px lip bottom/right)
	nvgBeginPath(vg);
	nvgMoveTo(vg, x + 2.0f, y + height - 2.0f);
	nvgLineTo(vg, x + 2.0f, y + 2.0f);
	nvgLineTo(vg, x + width - 2.0f, y + 2.0f);
	nvgStrokeColor(vg, ThemeNvColor(Theme::Role::TooltipWindowBevelDark));
	nvgStrokeWidth(vg, 1.0f);
	nvgStroke(vg);

	nvgBeginPath(vg);
	nvgMoveTo(vg, x + width - 2.5f, y + 2.0f);
	nvgLineTo(vg, x + width - 2.5f, y + height - 2.5f);
	nvgLineTo(vg, x + 2.0f, y + height - 2.5f);
	nvgStrokeColor(vg, ThemeNvColor(Theme::Role::TooltipWindowBevelLight));
	nvgStrokeWidth(vg, 1.0f);
	nvgStroke(vg);
}

void TooltipDrawer::drawSunkenSlot(NVGcontext* vg, float sx, float sy, float size, const TooltipData& tooltip) {
	// Recessed well fill
	nvgBeginPath(vg);
	nvgRect(vg, sx + 1.0f, sy + 1.0f, size - 2.0f, size - 2.0f);
	nvgFillColor(vg, ThemeNvColor(Theme::Role::TooltipSlotBg));
	nvgFill(vg);

	// Sunken 1px groove
	nvgBeginPath(vg);
	nvgMoveTo(vg, sx, sy + size);
	nvgLineTo(vg, sx, sy);
	nvgLineTo(vg, sx + size, sy);
	nvgStrokeColor(vg, ThemeNvColor(Theme::Role::TooltipSlotBevelDark));
	nvgStrokeWidth(vg, 1.0f);
	nvgStroke(vg);

	nvgBeginPath(vg);
	nvgMoveTo(vg, sx + size - 0.5f, sy);
	nvgLineTo(vg, sx + size - 0.5f, sy + size - 0.5f);
	nvgLineTo(vg, sx, sy + size - 0.5f);
	nvgStrokeColor(vg, ThemeNvColor(Theme::Role::TooltipSlotBevelLight));
	nvgStrokeWidth(vg, 1.0f);
	nvgStroke(vg);

	// Content inside slot
	if (tooltip.badgeType != rme::rendering::TileIndicatorType::None) {
		rme::rendering::DrawNanoVGIndicatorBadge(vg, tooltip.badgeType, sx, sy, size);
	} else if (tooltip.category == TooltipCategory::ZONE) {
		rme::rendering::DrawNanoVGCustomIndicatorBadge(vg, sx, sy, size, tooltip.zoneRgb, "zone");
	} else if (tooltip.itemId > 0) {
		int img = getSpriteImage(vg, tooltip.itemId);
		if (img > 0) {
			int imgW = 0;
			int imgH = 0;
			nvgImageSize(vg, img, &imgW, &imgH);
			if (imgW > 0 && imgH > 0) {
				const float scale = std::min(size / static_cast<float>(imgW), size / static_cast<float>(imgH));
				const float drawW = static_cast<float>(imgW) * scale;
				const float drawH = static_cast<float>(imgH) * scale;
				const float drawX = sx + (size - drawW) * 0.5f;
				const float drawY = sy + (size - drawH) * 0.5f;

				nvgBeginPath(vg);
				nvgRect(vg, drawX, drawY, drawW, drawH);
				nvgFillPaint(vg, nvgImagePattern(vg, drawX, drawY, drawW, drawH, 0.0f, img, 1.0f));
				nvgFill(vg);
			}
		}
	}
}

void TooltipDrawer::drawSunkenDivider(NVGcontext* vg, float dx, float dy, float dw) {
	// Top shadow line
	nvgBeginPath(vg);
	nvgMoveTo(vg, dx, dy - 0.5f);
	nvgLineTo(vg, dx + dw, dy - 0.5f);
	nvgStrokeColor(vg, ThemeNvColor(Theme::Role::TooltipDividerDark));
	nvgStrokeWidth(vg, 1.0f);
	nvgStroke(vg);

	// Bottom highlight lip
	nvgBeginPath(vg);
	nvgMoveTo(vg, dx, dy + 0.5f);
	nvgLineTo(vg, dx + dw, dy + 0.5f);
	nvgStrokeColor(vg, ThemeNvColor(Theme::Role::TooltipDividerLight));
	nvgStrokeWidth(vg, 1.0f);
	nvgStroke(vg);
}

void TooltipDrawer::drawHeader(NVGcontext* vg, float x, float y, float, const TooltipData& tooltip, const LayoutMetrics&, float padding) {
	const float slotX = x + padding;
	const float slotY = y + padding;
	constexpr float kSlotSize = 32.0f;
	constexpr float kHeaderGap = 8.0f;

	// Draw 32x32 Sunken Slot
	drawSunkenSlot(vg, slotX, slotY, kSlotSize, tooltip);

	// Text block to the right of the slot
	const float textX = slotX + kSlotSize + kHeaderGap;

	nvgFontSize(vg, 12.0f);
	nvgFontFace(vg, "sans");
	nvgFillColor(vg, ThemeNvColor(Theme::Role::TooltipTitleText));

	if (scratch_subtitle.empty()) {
		// Vertically centered when no subtitle
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgText(vg, textX, slotY + kSlotSize * 0.5f, scratch_title.data(), nullptr);
	} else {
		// Two-line layout: title at top, subtitle at bottom
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
		nvgText(vg, textX, slotY + 2.0f, scratch_title.data(), nullptr);

		nvgFontSize(vg, 10.0f);
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
		nvgFillColor(vg, ThemeNvColor(Theme::Role::TooltipSubtitleText));
		nvgText(vg, textX, slotY + 18.0f, scratch_subtitle.data(), nullptr);
	}
}

void TooltipDrawer::drawFields(NVGcontext* vg, float x, float y, float valueStartX, float lineHeight, float padding, float fontSize) {
	float contentX = x + padding;
	float cursorY = y;

	nvgFontSize(vg, fontSize);
	nvgFontFace(vg, "sans");
	nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

	for (size_t i = 0; i < scratch_fields_count; ++i) {
		const auto& field = scratch_fields[i];
		bool firstLine = true;
		for (const auto& line : field.wrappedLines) {
			if (firstLine) {
				wxColour labelCol = Theme::Get(Theme::Role::TooltipLabel);
				nvgFillColor(vg, nvgRGBA(labelCol.Red(), labelCol.Green(), labelCol.Blue(), 200));
				nvgText(vg, contentX, cursorY, field.label.data(), field.label.data() + field.label.size());
				firstLine = false;
			}

			nvgFillColor(vg, nvgRGBA(field.r, field.g, field.b, 255));
			nvgText(vg, contentX + valueStartX, cursorY, line.data(), line.data() + line.size());
			cursorY += lineHeight;
		}
	}
}

void TooltipDrawer::drawSunkenTextBox(NVGcontext* vg, float x, float y, float width, float height, float lineHeight, float padding, float fontSize) {
	const float wellX = x + padding;
	const float wellY = y;
	const float wellW = width - padding * 2.0f;
	const float wellH = height;

	// Inset well background
	nvgBeginPath(vg);
	nvgRect(vg, wellX + 1.0f, wellY + 1.0f, wellW - 2.0f, wellH - 2.0f);
	nvgFillColor(vg, ThemeNvColor(Theme::Role::TooltipTextWellBg));
	nvgFill(vg);

	// Sunken groove border
	nvgBeginPath(vg);
	nvgMoveTo(vg, wellX, wellY + wellH);
	nvgLineTo(vg, wellX, wellY);
	nvgLineTo(vg, wellX + wellW, wellY);
	nvgStrokeColor(vg, ThemeNvColor(Theme::Role::TooltipSlotBevelDark));
	nvgStrokeWidth(vg, 1.0f);
	nvgStroke(vg);

	nvgBeginPath(vg);
	nvgMoveTo(vg, wellX + wellW - 0.5f, wellY);
	nvgLineTo(vg, wellX + wellW - 0.5f, wellY + wellH - 0.5f);
	nvgLineTo(vg, wellX, wellY + wellH - 0.5f);
	nvgStrokeColor(vg, ThemeNvColor(Theme::Role::TooltipSlotBevelLight));
	nvgStrokeWidth(vg, 1.0f);
	nvgStroke(vg);

	// Text inside well
	float textCursorY = wellY + 6.0f;
	nvgFontSize(vg, fontSize);
	nvgFontFace(vg, "sans");
	nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

	wxColour textCol = Theme::Get(Theme::Role::TooltipTextValue); // Gold
	nvgFillColor(vg, nvgRGBA(textCol.Red(), textCol.Green(), textCol.Blue(), 255));

	for (const auto& line : scratch_text_lines) {
		if (!line.empty()) {
			nvgText(vg, wellX + 7.0f, textCursorY, line.data(), line.data() + line.size());
		}
		textCursorY += lineHeight;
	}
}

void TooltipDrawer::drawContainerGrid(NVGcontext* vg, float x, float y, const TooltipData& tooltip, const LayoutMetrics& layout, float padding) {
	if (layout.totalContainerSlots <= 0) {
		return;
	}

	const float startX = x + padding;
	const float startY = y;

	for (int idx = 0; idx < layout.totalContainerSlots; ++idx) {
		int col = idx % layout.containerCols;
		int row = idx / layout.containerCols;

		float itemX = startX + col * layout.gridSlotSize;
		float itemY = startY + row * layout.gridSlotSize;

		// Sunken slot background
		nvgBeginPath(vg);
		nvgRect(vg, itemX + 1.0f, itemY + 1.0f, 30.0f, 30.0f);
		nvgFillColor(vg, ThemeNvColor(Theme::Role::TooltipSlotBg));
		nvgFill(vg);

		nvgBeginPath(vg);
		nvgMoveTo(vg, itemX, itemY + 32.0f);
		nvgLineTo(vg, itemX, itemY);
		nvgLineTo(vg, itemX + 32.0f, itemY);
		nvgStrokeColor(vg, ThemeNvColor(Theme::Role::TooltipSlotBevelDark));
		nvgStrokeWidth(vg, 1.0f);
		nvgStroke(vg);

		nvgBeginPath(vg);
		nvgMoveTo(vg, itemX + 31.5f, itemY);
		nvgLineTo(vg, itemX + 31.5f, itemY + 31.5f);
		nvgLineTo(vg, itemX, itemY + 31.5f);
		nvgStrokeColor(vg, ThemeNvColor(Theme::Role::TooltipSlotBevelLight));
		nvgStrokeWidth(vg, 1.0f);
		nvgStroke(vg);

		bool isSummarySlot = (layout.emptyContainerSlots > 0 && idx == layout.totalContainerSlots - 1);

		if (isSummarySlot) {
			char summaryBuf[16];
			summaryBuf[0] = '+';
			auto [ptr, ec] = std::to_chars(summaryBuf + 1, summaryBuf + sizeof(summaryBuf) - 1, layout.emptyContainerSlots);
			*ptr = '\0';
			nvgFontSize(vg, 11.5f);
			nvgFontFace(vg, "sans");
			nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

			nvgFillColor(vg, nvgRGBA(0, 0, 0, 255));
			nvgText(vg, itemX + 17.0f, itemY + 17.0f, summaryBuf, nullptr);

			nvgFillColor(vg, ThemeNvColor(Theme::Role::TooltipCountText));
			nvgText(vg, itemX + 16.0f, itemY + 16.0f, summaryBuf, nullptr);
		} else if (idx < layout.numContainerItems) {
			const auto& item = tooltip.containerItems[idx];
			int img = getSpriteImage(vg, item.id);
			if (img > 0) {
				int imgW = 0;
				int imgH = 0;
				nvgImageSize(vg, img, &imgW, &imgH);
				if (imgW > 0 && imgH > 0) {
					const float scale = std::min(32.0f / static_cast<float>(imgW), 32.0f / static_cast<float>(imgH));
					const float drawW = static_cast<float>(imgW) * scale;
					const float drawH = static_cast<float>(imgH) * scale;
					const float drawX = itemX + (32.0f - drawW) * 0.5f;
					const float drawY = itemY + (32.0f - drawH) * 0.5f;

					nvgBeginPath(vg);
					nvgRect(vg, drawX, drawY, drawW, drawH);
					nvgFillPaint(vg, nvgImagePattern(vg, drawX, drawY, drawW, drawH, 0.0f, img, 1.0f));
					nvgFill(vg);
				}
			}

			if (item.count > 1) {
				char countBuf[16];
				auto [ptr, ec] = std::to_chars(countBuf, countBuf + sizeof(countBuf) - 1, item.count);
				*ptr = '\0';
				nvgFontSize(vg, 10.0f);
				nvgFontFace(vg, "sans");
				nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM);

				nvgFillColor(vg, nvgRGBA(0, 0, 0, 255));
				nvgText(vg, itemX + 32.0f, itemY + 32.0f, countBuf, nullptr);

				nvgFillColor(vg, ThemeNvColor(Theme::Role::TooltipCountText));
				nvgText(vg, itemX + 31.0f, itemY + 31.0f, countBuf, nullptr);
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

	if (view.zoom > 10.0f) {
		return;
	}

	const float screen_max_x = static_cast<float>(view.screensize_x) + 150.0f;
	const float screen_max_y = static_cast<float>(view.screensize_y) + 300.0f;

	for (size_t i = 0; i < active_count; ++i) {
		const auto& tooltip = tooltips[i];

		// Far-zoom LOD: when zoomed out very far (zoom > 3.0f), cull dense item tooltips
		// so street signs/blackboards don't flood whole towns. Macro entities (houses, towns, waypoints) stay visible.
		if (view.zoom > 3.0f && tooltip.category != TooltipCategory::HOUSE &&
		    tooltip.category != TooltipCategory::ZONE && tooltip.category != TooltipCategory::WAYPOINT) {
			continue;
		}

		int unscaled_x, unscaled_y;
		if (!view.IsTileVisible(tooltip.pos.x, tooltip.pos.y, tooltip.pos.z, unscaled_x, unscaled_y)) {
			continue;
		}

		float screen_x = static_cast<float>(unscaled_x) * inv_zoom;
		float screen_y = static_cast<float>(unscaled_y) * inv_zoom;

		screen_x += tile_size_screen * 0.5f;
		screen_y += tile_size_screen * 0.5f;

		// Frustum culling
		if (screen_x < -150.0f || screen_x > screen_max_x || screen_y < -50.0f || screen_y > screen_max_y) {
			continue;
		}

		constexpr float fontSize = 11.0f;
		constexpr float padding = 8.0f;
		constexpr float minWidth = 160.0f;
		constexpr float maxWidth = 260.0f;

		// 1. Prepare fields
		prepareFields(tooltip);

		// Skip if nothing to show
		if (scratch_fields_count == 0 && tooltip.containerItems.empty() && tooltip.text.empty() &&
		    tooltip.category != TooltipCategory::HOUSE && tooltip.category != TooltipCategory::ZONE &&
		    tooltip.category != TooltipCategory::WAYPOINT) {
			continue;
		}

		// 2. Calculate Layout
		LayoutMetrics layout = calculateLayout(vg, tooltip, maxWidth, minWidth, padding, fontSize);

		// Center horizontally above tile (pixel-perfect 1:1, no blurry fractional scaling)
		float tooltipX = std::floor(screen_x - (layout.width * 0.5f));
		float tooltipY = std::floor(screen_y - layout.height - 12.0f);

		nvgSave(vg);
		nvgTranslate(vg, tooltipX, tooltipY);

		// 3. Draw Window Frame & Background
		drawWindowFrame(vg, 0.0f, 0.0f, layout.width, layout.height, tooltip);

		// 4. Draw Header Section (Slot + Name + Subtitle)
		drawHeader(vg, 0.0f, 0.0f, layout.width, tooltip, layout, padding);

		float currentY = layout.headerHeight;

		// 5. Draw Attribute Fields Section (if any)
		if (scratch_fields_count > 0) {
			drawSunkenDivider(vg, 3.0f, currentY, layout.width - 6.0f);
			currentY += 5.0f;
			drawFields(vg, 0.0f, currentY, layout.valueStartX, fontSize * 1.35f, padding, fontSize);
			currentY += layout.fieldsHeight + 4.0f;
		}

		// 6. Draw Sunken Text Well Section (if readable text present)
		if (!scratch_text_lines.empty()) {
			drawSunkenDivider(vg, 3.0f, currentY, layout.width - 6.0f);
			currentY += 5.0f;
			drawSunkenTextBox(vg, 0.0f, currentY, layout.width, layout.textBoxHeight, 11.0f * 1.35f, padding, 11.0f);
			currentY += layout.textBoxHeight + 4.0f;
		}

		// 7. Draw Container Grid Section (if container contents present)
		if (layout.totalContainerSlots > 0) {
			drawSunkenDivider(vg, 3.0f, currentY, layout.width - 6.0f);
			currentY += 5.0f;
			drawContainerGrid(vg, 0.0f, currentY, tooltip, layout, padding);
		}

		nvgRestore(vg);
	}
}
