#include "app/main.h"
#include "palette/controls/virtual_brush_grid.h"
#include "palette/palette_window.h"
#include "palette/panels/brush_palette_panel.h"
#include "ui/gui.h"
#include "rendering/core/graphics.h"

#include <glad/glad.h>

#include <nanovg.h>
#include <nanovg_gl.h>

#include "util/nvg_utils.h"
#include "ui/theme.h"
#include "brushes/raw/raw_brush.h"
#include "brushes/creature/creature_brush.h"
#include "game/creatures.h"
#include "ui/find_item_window_model.h"

#include <algorithm>
#include <unordered_set>

#include <spdlog/spdlog.h>

namespace {
	static constexpr float GROW_FACTOR = 2.0f;
	static constexpr float SHADOW_ALPHA_BASE = 20.0f;
	static constexpr float SHADOW_ALPHA_FACTOR = 64.0f;
	static constexpr float SHADOW_BLUR_BASE = 6.0f;
	static constexpr float SHADOW_BLUR_FACTOR = 4.0f;
	static constexpr int TIMER_INTERVAL = 16;
	static constexpr float INTER_THRESHOLD = 0.01f;
	static constexpr float INTER_FACTOR = 0.2f;

	uint32_t GetBrushSortID(const Brush* brush) {
		if (!brush) {
			return 0;
		}
		if (const auto* raw = dynamic_cast<const RAWBrush*>(brush)) {
			return raw->getItemID();
		}
		if (const auto* cb = dynamic_cast<const CreatureBrush*>(brush)) {
			if (cb->getType()) {
				if (cb->getType()->outfit.lookType != 0) {
					return static_cast<uint32_t>(cb->getType()->outfit.lookType);
				}
				if (cb->getType()->outfit.lookItem != 0) {
					return static_cast<uint32_t>(cb->getType()->outfit.lookItem);
				}
			}
		}
		if (brush->getLookID() != 0) {
			return static_cast<uint32_t>(brush->getLookID());
		}
		return brush->getID();
	}
}

VirtualBrushGrid::VirtualBrushGrid(wxWindow* parent, const DynamicTilesetDefinition* _tileset, int iconSizePx) :
	NanoVGCanvas(parent, wxID_ANY, wxVSCROLL | wxWANTS_CHARS),
	BrushBoxInterface(_tileset),
	icon_size_px(std::clamp(iconSizePx, 32, 128)),
	selected_index(-1),
	hover_index(-1),
	columns(1),
	item_size(0),
	padding(4),
	observed_tileset_size(_tileset ? _tileset->size() : 0),
	m_display_brushes(),
	m_animTimer(this) {

	item_size = icon_size_px + 2 * ICON_OFFSET;

	Bind(wxEVT_LEFT_DOWN, &VirtualBrushGrid::OnMouseDown, this);
	Bind(wxEVT_MOTION, &VirtualBrushGrid::OnMotion, this);
	Bind(wxEVT_SIZE, &VirtualBrushGrid::OnSize, this);
	Bind(wxEVT_TIMER, &VirtualBrushGrid::OnTimer, this);

	RefreshBrushList();
	UpdateLayout();
}

VirtualBrushGrid::~VirtualBrushGrid() = default;

void VirtualBrushGrid::SetDisplayMode(DisplayMode mode) {
	if (display_mode != mode) {
		display_mode = mode;
		m_truncatedLabelCache.clear();
		UpdateLayout();
		Refresh();
	}
}

void VirtualBrushGrid::RefreshBrushList() {
	m_truncatedLabelCache.clear();
	Brush* selectedBrush = GetSelectedBrush();
	m_display_brushes.clear();

	static const std::vector<Brush*> s_emptyBrushes;
	const std::vector<Brush*>& sourceBrushes = m_hasOverrideBrushes ? m_overrideBrushes : (tileset ? tileset->brushes : s_emptyBrushes);

	std::vector<Brush*> uniqueSource;
	uniqueSource.reserve(sourceBrushes.size());
	std::unordered_set<const Brush*> seen;
	for (Brush* b : sourceBrushes) {
		if (b && seen.insert(b).second) {
			uniqueSource.push_back(b);
		}
	}

	if (!m_filterQuery.empty()) {
		m_display_brushes = FilterBrushesWithAdvancedFinder(uniqueSource, m_filterQuery);
		if (m_hasSort) {
			ApplySort();
		}
	} else {
		m_display_brushes = std::move(uniqueSource);
		if (m_hasSort) {
			ApplySort();
		}
	}

	selected_index = -1;
	if (selectedBrush) {
		for (size_t i = 0; i < m_display_brushes.size(); ++i) {
			if (m_display_brushes[i] == selectedBrush) {
				selected_index = static_cast<int>(i);
				break;
			}
		}
	}
}

void VirtualBrushGrid::ApplySort() {
	if (!m_hasSort) {
		return;
	}

	auto compare = [this](const Brush* a, const Brush* b) {
		if (!a && !b) return false;
		if (!a) return false;
		if (!b) return true;

		if (m_sortKey == TilesetSortKey::ID) {
			uint32_t idA = GetBrushSortID(a);
			uint32_t idB = GetBrushSortID(b);
			if (idA != idB) {
				return m_sortDir == TilesetSortDirection::Ascending ? (idA < idB) : (idA > idB);
			}
			int cmp = wxStricmp(wxstr(a->getName()), wxstr(b->getName()));
			if (cmp != 0) {
				return m_sortDir == TilesetSortDirection::Ascending ? (cmp < 0) : (cmp > 0);
			}
		} else {
			std::string nameA = a->getName();
			std::string nameB = b->getName();
			int cmp = wxStricmp(wxstr(nameA), wxstr(nameB));
			if (cmp != 0) {
				return m_sortDir == TilesetSortDirection::Ascending ? (cmp < 0) : (cmp > 0);
			}
			uint32_t idA = GetBrushSortID(a);
			uint32_t idB = GetBrushSortID(b);
			if (idA != idB) {
				return m_sortDir == TilesetSortDirection::Ascending ? (idA < idB) : (idA > idB);
			}
		}
		return false;
	};

	std::stable_sort(m_display_brushes.begin(), m_display_brushes.end(), compare);
}

void VirtualBrushGrid::SetSort(TilesetSortKey key, TilesetSortDirection dir) {
	m_sortKey = key;
	m_sortDir = dir;
	m_hasSort = true;
	RefreshBrushList();
	UpdateLayout();
	Refresh();
}

void VirtualBrushGrid::ClearSort() {
	m_hasSort = false;
	RefreshBrushList();
	UpdateLayout();
	Refresh();
}

void VirtualBrushGrid::SetShowLabels(bool show) {
	if (m_showLabels != show) {
		m_showLabels = show;
		m_truncatedLabelCache.clear();
		UpdateLayout();
		Refresh();
	}
}

void VirtualBrushGrid::SetTileSize(int sizePx) {
	sizePx = std::clamp(sizePx, 32, 128);
	if (icon_size_px != sizePx) {
		icon_size_px = sizePx;
		item_size = icon_size_px + 2 * ICON_OFFSET;
		m_truncatedLabelCache.clear();
		UpdateLayout();
		Refresh();
	}
}

void VirtualBrushGrid::SetFilterQuery(const std::string& query, const std::vector<Brush*>* overrideSource) {
	bool filterChanged = (m_filterQuery != query) || (m_hasOverrideBrushes != (overrideSource != nullptr));
	m_filterQuery = query;
	if (overrideSource) {
		m_overrideBrushes = *overrideSource;
		m_hasOverrideBrushes = true;
	} else {
		m_overrideBrushes.clear();
		m_hasOverrideBrushes = false;
	}
	RefreshBrushList();
	if (filterChanged) {
		SetScrollPosition(0);
	}
	UpdateLayout();
	Refresh();
}

void VirtualBrushGrid::UpdateLayout() {
	int width = GetClientSize().x;
	if (width <= 0) {
		width = 200; // Default
	}

	int totalItems = static_cast<int>(m_display_brushes.size());
	if (display_mode == DisplayMode::List) {
		columns = 1;
		int contentHeight = totalItems * LIST_ROW_HEIGHT + padding;
		UpdateScrollbar(contentHeight);
	} else {
		int cellWidth = item_size;
		int cellHeight = item_size + (m_showLabels ? LABEL_HEIGHT : 0);
		columns = std::max(1, (width - padding) / (cellWidth + padding));
		int rows = (totalItems + columns - 1) / columns;
		int contentHeight = rows * (cellHeight + padding) + padding;
		UpdateScrollbar(contentHeight);
	}
}

wxSize VirtualBrushGrid::DoGetBestClientSize() const {
	return FromDIP(wxSize(200, 300));
}

void VirtualBrushGrid::OnNanoVGPaint(NVGcontext* vg, int width, int height) {
	if (!m_hasOverrideBrushes && observed_tileset_size != (tileset ? tileset->size() : 0)) {
		observed_tileset_size = tileset ? tileset->size() : 0;
		RefreshBrushList();
		UpdateLayout();
	}

	// Calculate visible range
	int scrollPos = GetScrollPosition();
	int rowHeight = (display_mode == DisplayMode::List) ? LIST_ROW_HEIGHT : (item_size + (m_showLabels ? LABEL_HEIGHT : 0) + padding);
	int startRow = scrollPos / rowHeight;
	int endRow = (scrollPos + height + rowHeight - 1) / rowHeight + 1;

	int startIdx = startRow * columns;
	int endIdx = std::min(static_cast<int>(m_display_brushes.size()), endRow * columns);

	// Draw visible items
	for (int i = startIdx; i < endIdx; ++i) {
		DrawBrushItem(vg, i, GetItemRect(i));
	}
}

void VirtualBrushGrid::DrawBrushItem(NVGcontext* vg, int i, const wxRect& rect) {
	float x = static_cast<float>(rect.x);
	float y = static_cast<float>(rect.y);
	float w = static_cast<float>(rect.width);
	float h = static_cast<float>(rect.height);

	// Animation scaling
	if (i == hover_index) {
		float grow = GROW_FACTOR * hover_anim;
		x -= grow;
		y -= grow;
		w += grow * 2.0f;
		h += grow * 2.0f;
	}

	// Shadow / Glow
	if (i == selected_index) {
		// Glow for selected
		NVGpaint shadowPaint = nvgBoxGradient(vg, x, y, w, h, 4.0f, 10.0f, nvgRGBA(100, 150, 255, 128), nvgRGBA(0, 0, 0, 0));
		nvgBeginPath(vg);
		nvgRect(vg, x - 10, y - 10, w + 20, h + 20);
		nvgRoundedRect(vg, x, y, w, h, 4.0f);
		nvgPathWinding(vg, NVG_HOLE);
		nvgFillPaint(vg, shadowPaint);
		nvgFill(vg);
	} else if (i == hover_index) {
		// Animated shadow for hover
		float shadowAlpha = SHADOW_ALPHA_FACTOR * hover_anim + SHADOW_ALPHA_BASE;
		float shadowBlur = SHADOW_BLUR_BASE + SHADOW_BLUR_FACTOR * hover_anim;
		NVGpaint shadowPaint = nvgBoxGradient(vg, x, y + 2, w, h, 4.0f, shadowBlur, nvgRGBA(0, 0, 0, static_cast<int>(shadowAlpha)), nvgRGBA(0, 0, 0, 0));
		nvgBeginPath(vg);
		nvgRect(vg, x - 10, y - 10, w + 20, h + 20);
		nvgRoundedRect(vg, x, y, w, h, 4.0f);
		nvgPathWinding(vg, NVG_HOLE);
		nvgFillPaint(vg, shadowPaint);
		nvgFill(vg);
	}

	// Card background
	nvgBeginPath(vg);
	nvgRoundedRect(vg, x, y, w, h, 4.0f);

	if (i == selected_index) {
		NVGcolor selCol = NvgUtils::ToNvColor(Theme::Get(Theme::Role::Accent));
		selCol.a = 1.0f; // Force opaque for background
		nvgFillColor(vg, selCol);
	} else if (i == hover_index) {
		nvgFillColor(vg, NvgUtils::ToNvColor(Theme::Get(Theme::Role::CardBaseHover)));
	} else {
		// Normal - theme card base
		nvgFillColor(vg, NvgUtils::ToNvColor(Theme::Get(Theme::Role::CardBase)));
	}
	nvgFill(vg);

	// Selection border
	if (i == selected_index) {
		nvgBeginPath(vg);
		nvgRoundedRect(vg, x + 0.5f, y + 0.5f, w - 1.0f, h - 1.0f, 4.0f);
		nvgStrokeColor(vg, NvgUtils::ToNvColor(Theme::Get(Theme::Role::Accent)));
		nvgStrokeWidth(vg, 2.0f);
		nvgStroke(vg);
	}

	// Draw brush sprite
	Brush* brush = (i < static_cast<int>(m_display_brushes.size())) ? m_display_brushes[i] : nullptr;
	if (brush) {
		Sprite* spr = brush->getSprite();
		if (!spr) {
			spr = g_gui.gfx.getSprite(brush->getLookID());
		}

		int tex = spr ? GetOrCreateSpriteTexture(vg, spr) : 0;
		int iconSize = (display_mode == DisplayMode::List) ? GRID_ITEM_SIZE_BASE : (item_size - 2 * ICON_OFFSET);
		int iconX = (display_mode == DisplayMode::List) ? (rect.x + ICON_OFFSET) : (rect.x + (rect.width - iconSize) / 2);
		int iconY = rect.y + ICON_OFFSET;

		if (tex > 0) {
			NVGpaint imgPaint = nvgImagePattern(vg, static_cast<float>(iconX), static_cast<float>(iconY), static_cast<float>(iconSize), static_cast<float>(iconSize), 0.0f, tex, 1.0f);

			nvgBeginPath(vg);
			nvgRoundedRect(vg, static_cast<float>(iconX), static_cast<float>(iconY), static_cast<float>(iconSize), static_cast<float>(iconSize), 3.0f);
			nvgFillPaint(vg, imgPaint);
			nvgFill(vg);
		} else {
			// Placeholder box for entries without sprite (e.g. completely transparent tile or missing sprite)
			nvgBeginPath(vg);
			nvgRoundedRect(vg, static_cast<float>(iconX), static_cast<float>(iconY), static_cast<float>(iconSize), static_cast<float>(iconSize), 3.0f);
			nvgFillColor(vg, nvgRGBA(255, 255, 255, 12));
			nvgFill(vg);
			nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 40));
			nvgStrokeWidth(vg, 1.0f);
			nvgStroke(vg);

			nvgFontSize(vg, static_cast<float>(iconSize) * 0.45f);
			nvgFontFace(vg, "sans");
			nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgFillColor(vg, nvgRGBA(255, 255, 255, 120));
			nvgText(vg, iconX + iconSize / 2.0f, iconY + iconSize / 2.0f, "?", nullptr);
		}

		if (display_mode == DisplayMode::List) {
			nvgFontSize(vg, 14.0f);
			nvgFontFace(vg, "sans");
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
			nvgFillColor(vg, NvgUtils::ToNvColor(Theme::Get(Theme::Role::Text)));

			auto it = m_utf8NameCache.find(brush);
			if (it == m_utf8NameCache.end()) {
				m_utf8NameCache[brush] = std::string(wxstr(brush->getName()).ToUTF8());
				it = m_utf8NameCache.find(brush);
			}
			nvgText(vg, rect.x + 40, rect.y + rect.height / 2.0f, it->second.c_str(), nullptr);
		} else if (m_showLabels) {
			nvgFontSize(vg, 11.0f);
			nvgFontFace(vg, "sans");
			nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			if (i == selected_index) {
				nvgFillColor(vg, NvgUtils::ToNvColor(Theme::Get(Theme::Role::TextOnAccent)));
			} else {
				nvgFillColor(vg, NvgUtils::ToNvColor(Theme::Get(Theme::Role::Text)));
			}

			auto it = m_truncatedLabelCache.find(brush);
			if (it == m_truncatedLabelCache.end()) {
				CachedLabel cachedLabel;
				wxString wxName = wxstr(brush->getName());
				std::string utf8Full = wxName.ToStdString();
				const float maxTextWidth = static_cast<float>(rect.width - 4);
				float bounds[4];
				nvgTextBounds(vg, 0, 0, utf8Full.c_str(), nullptr, bounds);
				float textWidth = bounds[2] - bounds[0];

				if (textWidth <= maxTextWidth) {
					cachedLabel.line1 = std::move(utf8Full);
					cachedLabel.line2 = "";
				} else {
					wxString wxLine1;
					wxString wxLine2;

					// Prefer splitting on " - " delimiter (e.g. "3263 - jungle grass")
					int dashPos = wxName.Find(" - ");
					if (dashPos != wxNOT_FOUND && dashPos > 0) {
						wxString prefix = wxName.substr(0, dashPos);
						std::string utf8Prefix = prefix.ToStdString();
						nvgTextBounds(vg, 0, 0, utf8Prefix.c_str(), nullptr, bounds);
						if ((bounds[2] - bounds[0]) <= maxTextWidth) {
							wxLine1 = prefix;
							wxLine2 = wxName.substr(dashPos + 3);
						}
					}

					if (wxLine1.empty()) {
						// Split by word boundary (' ', '-') if possible
						int bestSplit = -1;
						int len = static_cast<int>(wxName.length());
						for (int idx = 1; idx < len; ++idx) {
							wxChar ch = wxName[idx];
							if (ch == ' ' || ch == '-') {
								wxString cand1 = wxName.substr(0, (ch == '-') ? (idx + 1) : idx);
								std::string utf8Cand1 = cand1.ToStdString();
								nvgTextBounds(vg, 0, 0, utf8Cand1.c_str(), nullptr, bounds);
								if ((bounds[2] - bounds[0]) <= maxTextWidth) {
									bestSplit = idx;
								} else {
									break;
								}
							}
						}

						if (bestSplit != -1) {
							wxChar splitChar = wxName[bestSplit];
							if (splitChar == '-') {
								wxLine1 = wxName.substr(0, bestSplit + 1);
								wxLine2 = wxName.substr(bestSplit + 1);
							} else {
								wxLine1 = wxName.substr(0, bestSplit);
								wxLine2 = wxName.substr(bestSplit + 1);
							}
						} else {
							wxString cand1 = wxName;
							while (cand1.length() > 1) {
								cand1.RemoveLast();
								std::string utf8Cand1 = cand1.ToStdString();
								nvgTextBounds(vg, 0, 0, utf8Cand1.c_str(), nullptr, bounds);
								if ((bounds[2] - bounds[0]) <= maxTextWidth) {
									wxLine1 = cand1;
									wxLine2 = wxName.substr(cand1.length());
									break;
								}
							}
							if (wxLine1.empty()) {
								wxLine1 = wxName.substr(0, 1);
								wxLine2 = wxName.substr(1);
							}
						}
					}

					// Clean up delimiters and whitespace on boundary
					while (!wxLine1.empty() && (wxLine1.Last() == ' ' || wxLine1.Last() == '-' || wxLine1.Last() == '\t')) {
						wxLine1.RemoveLast();
					}
					while (!wxLine2.empty() && (wxLine2[0] == ' ' || wxLine2[0] == '-' || wxLine2[0] == '\t')) {
						wxLine2.Remove(0, 1);
					}

					cachedLabel.line1 = wxLine1.ToStdString();

					if (wxLine2.empty()) {
						cachedLabel.line2 = "";
					} else {
						std::string utf8Line2 = wxLine2.ToStdString();
						nvgTextBounds(vg, 0, 0, utf8Line2.c_str(), nullptr, bounds);
						if ((bounds[2] - bounds[0]) <= maxTextWidth) {
							cachedLabel.line2 = std::move(utf8Line2);
						} else {
							cachedLabel.line2 = "...";
							wxString truncated2 = wxLine2;
							while (truncated2.length() > 1) {
								truncated2.RemoveLast();
								wxString cand2 = truncated2 + "...";
								std::string utf8Cand2 = cand2.ToStdString();
								nvgTextBounds(vg, 0, 0, utf8Cand2.c_str(), nullptr, bounds);
								if ((bounds[2] - bounds[0]) <= maxTextWidth) {
									cachedLabel.line2 = std::move(utf8Cand2);
									break;
								}
							}
						}
					}
				}
				it = m_truncatedLabelCache.emplace(brush, std::move(cachedLabel)).first;
			}

			const CachedLabel& label = it->second;
			float labelX = rect.x + rect.width / 2.0f;
			if (label.line2.empty()) {
				float labelY = rect.y + item_size + (LABEL_HEIGHT / 2.0f);
				nvgText(vg, labelX, labelY, label.line1.c_str(), nullptr);
			} else {
				float line1Y = rect.y + item_size + 9.0f;
				float line2Y = rect.y + item_size + 23.0f;
				nvgText(vg, labelX, line1Y, label.line1.c_str(), nullptr);
				nvgText(vg, labelX, line2Y, label.line2.c_str(), nullptr);
			}
		}
	}
}

wxRect VirtualBrushGrid::GetItemRect(int index) const {
	if (display_mode == DisplayMode::List) {
		int width = GetClientSize().x - 2 * padding;
		return wxRect(padding, padding + index * LIST_ROW_HEIGHT, width, LIST_ROW_HEIGHT);
	} else {
		int row = index / columns;
		int col = index % columns;
		int cellWidth = item_size;
		int cellHeight = item_size + (m_showLabels ? LABEL_HEIGHT : 0);

		return wxRect(
			padding + col * (cellWidth + padding),
			padding + row * (cellHeight + padding),
			cellWidth,
			cellHeight
		);
	}
}

int VirtualBrushGrid::HitTest(int x, int y) const {
	int scrollPos = GetScrollPosition();
	int realY = y + scrollPos;
	int realX = x;
	int totalItems = static_cast<int>(m_display_brushes.size());

	if (display_mode == DisplayMode::List) {
		int row = (realY - padding) / LIST_ROW_HEIGHT;

		if (row < 0 || row >= totalItems) {
			return -1;
		}

		int index = row;
		// Check horizontal bounds properly
		int width = GetClientSize().x - 2 * padding;
		if (realX >= padding && realX <= GetClientSize().x - padding) {
			return index;
		}
		return -1;
	} else {
		if (realX < padding || realY < padding) {
			return -1;
		}

		int cellWidth = item_size;
		int cellHeight = item_size + (m_showLabels ? LABEL_HEIGHT : 0);

		int col = (realX - padding) / (cellWidth + padding);
		int row = (realY - padding) / (cellHeight + padding);

		if (col < 0 || col >= columns || row < 0) {
			return -1;
		}

		int index = row * columns + col;
		if (index >= 0 && index < totalItems) {
			wxRect rect = GetItemRect(index);
			// Adjust rect to scroll position for contains check
			rect.y -= scrollPos;
			if (rect.Contains(x, y)) {
				return index;
			}
		}
		return -1;
	}
}

void VirtualBrushGrid::OnMouseDown(wxMouseEvent& event) {
	int index = HitTest(event.GetX(), event.GetY());
	if (index != -1) {
		if (m_hasOverrideBrushes && index >= 0 && static_cast<size_t>(index) < m_display_brushes.size()) {
			Brush* clickedBrush = m_display_brushes[index];
			if (clickedBrush) {
				PaletteWindow* pw = nullptr;
				BrushPalettePanel* currentBrushPalettePanel = nullptr;
				wxWindow* w = GetParent();
				while (w) {
					if (!currentBrushPalettePanel) {
						currentBrushPalettePanel = dynamic_cast<BrushPalettePanel*>(w);
					}
					if (!pw) {
						pw = dynamic_cast<PaletteWindow*>(w);
					}
					if (pw && currentBrushPalettePanel) {
						break;
					}
					w = w->GetParent();
				}
				if (pw) {
					std::string preferredPal = currentBrushPalettePanel ? currentBrushPalettePanel->GetName().ToStdString() : "";
					if (pw->JumpToBrush(clickedBrush, preferredPal)) {
						return;
					}
				}
			}
		}

		if (index != selected_index) {
			selected_index = index;

			// Notify GUI - find PaletteWindow parent
			wxWindow* w = GetParent();
			while (w) {
				PaletteWindow* pw = dynamic_cast<PaletteWindow*>(w);
				if (pw) {
					g_gui.ActivatePalette(pw);
					break;
				}
				w = w->GetParent();
			}

			g_gui.SelectBrushInternal(m_display_brushes[selected_index]);
			Refresh();
		}
	}
}

void VirtualBrushGrid::OnMotion(wxMouseEvent& event) {
	int index = HitTest(event.GetX(), event.GetY());

	if (index != hover_index) {
		hover_index = index;
		if (index != -1) {
			hover_anim = 0.0f; // Reset animation for new target
		}
		if (!m_animTimer.IsRunning()) {
			m_animTimer.Start(TIMER_INTERVAL);
		}
		Refresh();
	} else if (hover_index != -1 && !m_animTimer.IsRunning()) {
		m_animTimer.Start(TIMER_INTERVAL);
	}

	// Tooltip
	if (index >= 0 && static_cast<size_t>(index) < m_display_brushes.size()) {
		Brush* brush = m_display_brushes[index];
		if (brush) {
			wxString tip = wxstr(brush->getName());
			if (GetToolTipText() != tip) {
				SetToolTip(tip);
			}
		}
	} else {
		UnsetToolTip();
	}

	event.Skip();
}

void VirtualBrushGrid::OnTimer(wxTimerEvent& event) {
	float target = (hover_index != -1) ? 1.0f : 0.0f;
	if (std::abs(hover_anim - target) > INTER_THRESHOLD) {
		hover_anim += (target - hover_anim) * INTER_FACTOR;
		Refresh();
	} else {
		hover_anim = target;
		if (hover_index == -1) {
			m_animTimer.Stop();
		}
	}
}

void VirtualBrushGrid::OnSize(wxSizeEvent& event) {
	UpdateLayout();
	Refresh();
	event.Skip();
}

void VirtualBrushGrid::SelectFirstBrush() {
	if (!m_display_brushes.empty()) {
		selected_index = 0;
		Refresh();
	}
}

Brush* VirtualBrushGrid::GetSelectedBrush() const {
	if (selected_index >= 0 && selected_index < static_cast<int>(m_display_brushes.size())) {
		return m_display_brushes[selected_index];
	}
	return nullptr;
}

bool VirtualBrushGrid::SelectBrush(const Brush* brush) {
	for (size_t i = 0; i < m_display_brushes.size(); ++i) {
		if (m_display_brushes[i] == brush) {
			selected_index = static_cast<int>(i);

			// Ensure visible
			wxRect rect = GetItemRect(selected_index);
			int scrollPos = GetScrollPosition();
			int clientHeight = GetClientSize().y;

			if (rect.y < scrollPos) {
				SetScrollPosition(rect.y - padding);
			} else if (rect.y + rect.height > scrollPos + clientHeight) {
				SetScrollPosition(rect.y + rect.height - clientHeight + padding);
			}

			Refresh();
			return true;
		}
	}
	selected_index = -1;
	Refresh();
	return false;
}
