#include "util/virtual_item_grid.h"
#include "util/nvg_utils.h"
#include "util/common.h"
#include "item_definitions/core/item_definition_store.h"
#include "rendering/indicators/indicator_drawing_utils.h"
#include "ui/theme.h"
#include <algorithm>
#include <format>
#include <cmath>
#include <ranges>

namespace {
	// Two lines of 12 px text plus breathing room, matching the palette grid
	// label area, and the padding above the icon inside a card.
	constexpr int kLabelArea = 32;
	constexpr int kIconTopPadding = 8;
	constexpr int kIconBottomPadding = 4;
	constexpr int kMinIconSize = 16;
	constexpr int kMaxIconSize = 128;
} // namespace

VirtualItemGrid::VirtualItemGrid(wxWindow* parent, wxWindowID id) :
	NanoVGCanvas(parent, id, wxVSCROLL | wxWANTS_CHARS),
	m_padding(6),
	m_iconSize(32),
	m_showLabels(true),
	m_columns(1),
	m_selectedIndex(-1),
	m_hoverIndex(-1),
	m_animTimer(this) {
	Bind(wxEVT_SIZE, &VirtualItemGrid::OnSize, this);
	Bind(wxEVT_LEFT_DOWN, &VirtualItemGrid::OnMouseDown, this);
	Bind(wxEVT_MOTION, &VirtualItemGrid::OnMotion, this);
	Bind(wxEVT_LEAVE_WINDOW, &VirtualItemGrid::OnLeave, this);
}

VirtualItemGrid::~VirtualItemGrid() {
}

int VirtualItemGrid::CardSize() const {
	return kIconTopPadding + m_iconSize + (m_showLabels ? kLabelArea : 0) + kIconBottomPadding;
}

void VirtualItemGrid::SetShowLabels(bool show) {
	if (m_showLabels == show) {
		return;
	}
	m_showLabels = show;
	RefreshGrid();
}

void VirtualItemGrid::SetIconSize(int sizePx) {
	const int clamped = std::clamp(sizePx, kMinIconSize, kMaxIconSize);
	if (m_iconSize == clamped) {
		return;
	}
	m_iconSize = clamped;
	RefreshGrid();
}

void VirtualItemGrid::SetSort(TilesetSortKey key, TilesetSortDirection dir) {
	m_sortKey = key;
	m_sortDir = dir;
	m_hasSort = true;
}

void VirtualItemGrid::ClearSort() {
	m_hasSort = false;
}

void VirtualItemGrid::ApplySortTo(std::vector<uint16_t>& items) const {
	if (!m_hasSort || items.size() < 2) {
		return;
	}

	const bool ascending = m_sortDir == TilesetSortDirection::Ascending;
	if (m_sortKey == TilesetSortKey::ID) {
		std::ranges::sort(items, [ascending](uint16_t a, uint16_t b) {
			return ascending ? a < b : a > b;
		});
		return;
	}

	// Name ordering: resolve the names once instead of on every comparison.
	std::vector<std::pair<std::string, uint16_t>> keyed;
	keyed.reserve(items.size());
	for (uint16_t id : items) {
		keyed.emplace_back(GetSortName(id), id);
	}

	std::ranges::sort(keyed, [ascending](const auto& lhs, const auto& rhs) {
		if (lhs.first != rhs.first) {
			return ascending ? lhs.first < rhs.first : lhs.first > rhs.first;
		}
		return lhs.second < rhs.second;
	});

	std::ranges::transform(keyed, items.begin(), [](const auto& entry) {
		return entry.second;
	});
}

std::string VirtualItemGrid::GetSortName(uint16_t id) const {
	const auto definition = g_item_definitions.get(id);
	return definition ? std::string(definition.name()) : std::string();
}

void VirtualItemGrid::RefreshGrid() {
	UpdateLayout();
	Refresh();
}

void VirtualItemGrid::UpdateLayout() {
	int width = GetClientSize().x;
	if (width <= 0) {
		width = 200;
	}

	const int card = CardSize();
	m_columns = std::max(1, (width - m_padding) / (card + m_padding));
	size_t count = GetItemCount();
	int rows = (count + m_columns - 1) / m_columns;
	int contentHeight = rows * (card + m_padding) + m_padding;

	UpdateScrollbar(contentHeight);
}

wxSize VirtualItemGrid::DoGetBestClientSize() const {
	return FromDIP(wxSize(250, 400));
}

int VirtualItemGrid::HitTest(int x, int y) const {
	const int card = CardSize();
	int scrollPos = GetScrollPosition();
	int realY = y + scrollPos;
	int col = (x - m_padding) / (card + m_padding);
	int row = (realY - m_padding) / (card + m_padding);

	if (col < 0 || col >= m_columns || row < 0) {
		return -1;
	}

	size_t index = row * m_columns + col;
	if (index < GetItemCount()) {
		wxRect r = GetItemRect(static_cast<int>(index));
		r.y -= scrollPos; // Convert back to local for test
		if (r.Contains(x, y)) {
			return static_cast<int>(index);
		}
	}
	return -1;
}

wxRect VirtualItemGrid::GetItemRect(int index) const {
	const int card = CardSize();
	int row = index / m_columns;
	int col = index % m_columns;
	return wxRect(
		m_padding + col * (card + m_padding),
		m_padding + row * (card + m_padding),
		card,
		card
	);
}

void VirtualItemGrid::SetSelection(int index) {
	if (index != m_selectedIndex) {
		m_selectedIndex = index;
		OnItemSelected(index);
		EnsureVisible(index);
		Refresh();
	}
}

uint16_t VirtualItemGrid::GetSelectedItemId() const {
	if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(GetItemCount())) {
		return GetItem(m_selectedIndex);
	}
	return 0;
}

void VirtualItemGrid::EnsureVisible(int index) {
	if (index < 0 || index >= static_cast<int>(GetItemCount())) {
		return;
	}

	wxRect rect = GetItemRect(index);
	int scrollPos = GetScrollPosition();
	int clientH = GetClientSize().y;

	if (rect.y < scrollPos) {
		SetScrollPosition(rect.y - m_padding);
	} else if (rect.y + rect.height > scrollPos + clientH) {
		SetScrollPosition(rect.y + rect.height - clientH + m_padding);
	}
}

void VirtualItemGrid::OnSize(wxSizeEvent& event) {
	UpdateLayout();
	Refresh();
	event.Skip();
}

void VirtualItemGrid::OnMouseDown(wxMouseEvent& event) {
	int index = HitTest(event.GetX(), event.GetY());
	if (index != -1) {
		SetSelection(index);
	}
	event.Skip();
}

void VirtualItemGrid::OnMotion(wxMouseEvent& event) {
	int index = HitTest(event.GetX(), event.GetY());
	if (index != m_hoverIndex) {
		m_hoverIndex = index;

		if (m_hoverIndex != -1) {
			SetCursor(wxCursor(wxCURSOR_HAND));
			SetToolTip(GetItemName(m_hoverIndex));
		} else {
			SetCursor(wxNullCursor);
			UnsetToolTip();
		}
		Refresh();
	}
	event.Skip();
}

void VirtualItemGrid::OnLeave(wxMouseEvent& event) {
	if (m_hoverIndex != -1) {
		m_hoverIndex = -1;
		Refresh();
	}
	event.Skip();
}

void VirtualItemGrid::OnTimer(wxTimerEvent&) {
}

void VirtualItemGrid::ClearCache() {
	ClearImageCache();
}

int VirtualItemGrid::GetOrCreateItemTexture(NVGcontext* vg, uint16_t id) {
	return GetOrCreateItemImage(id);
}

wxString VirtualItemGrid::GetItemName(size_t index) const {
	if (index < GetItemCount()) {
		uint16_t id = GetItem(index);
		const auto definition = g_item_definitions.get(id);
		return definition ? wxstr(definition.name()) : wxString();
	}
	return "";
}

void VirtualItemGrid::OnNanoVGPaint(NVGcontext* vg, int width, int height) {
	const int card = CardSize();
	int rowHeight = card + m_padding;
	int scrollPos = GetScrollPosition();

	size_t count = GetItemCount();
	if (count == 0) {
		return;
	}

	// Calculate visible range
	int startRow = scrollPos / rowHeight;
	int endRow = (scrollPos + height + rowHeight - 1) / rowHeight;

	int startIdx = startRow * m_columns;
	int endIdx = std::min(static_cast<int>(count), (endRow + 1) * m_columns);

	for (int i = startIdx; i < endIdx; ++i) {
		if (i < 0) {
			continue;
		}

		wxRect r = GetItemRect(i);
		float x = static_cast<float>(r.x);
		float y = static_cast<float>(r.y);
		float w = static_cast<float>(r.width);
		float h = static_cast<float>(r.height);

		bool isSelected = (i == m_selectedIndex);
		bool isHovered = (i == m_hoverIndex);

		// Shadow / Glow
		if (isSelected) {
			NVGpaint shadowPaint = nvgBoxGradient(vg, x, y, w, h, 4.0f, 10.0f, nvgRGBA(100, 150, 255, 128), nvgRGBA(0, 0, 0, 0));
			nvgBeginPath(vg);
			nvgRect(vg, x - 10, y - 10, w + 20, h + 20);
			nvgRoundedRect(vg, x, y, w, h, 4.0f);
			nvgPathWinding(vg, NVG_HOLE);
			nvgFillPaint(vg, shadowPaint);
			nvgFill(vg);
		} else if (isHovered) {
			NVGpaint shadowPaint = nvgBoxGradient(vg, x, y + 2, w, h, 4.0f, 6.0f, nvgRGBA(0, 0, 0, 64), nvgRGBA(0, 0, 0, 0));
			nvgBeginPath(vg);
			nvgRect(vg, x - 5, y - 5, w + 10, h + 10);
			nvgRoundedRect(vg, x, y, w, h, 4.0f);
			nvgPathWinding(vg, NVG_HOLE);
			nvgFillPaint(vg, shadowPaint);
			nvgFill(vg);
		}

		// Card Background
		nvgBeginPath(vg);
		nvgRoundedRect(vg, x, y, w, h, 4.0f);
		if (isSelected) {
			wxColour selCol = Theme::Get(Theme::Role::Accent);
			nvgFillColor(vg, nvgRGBA(selCol.Red(), selCol.Green(), selCol.Blue(), 200));
		} else if (isHovered) {
			wxColour hoverCol = Theme::Get(Theme::Role::CardBaseHover);
			nvgFillColor(vg, nvgRGBA(hoverCol.Red(), hoverCol.Green(), hoverCol.Blue(), 255));
		} else {
			wxColour baseCol = Theme::Get(Theme::Role::CardBase);
			nvgFillColor(vg, nvgRGBA(baseCol.Red(), baseCol.Green(), baseCol.Blue(), 255));
		}
		nvgFill(vg);

		// Selection Border
		if (isSelected) {
			nvgBeginPath(vg);
			nvgRoundedRect(vg, x + 0.5f, y + 0.5f, w - 1, h - 1, 4.0f);
			wxColour accentCol = Theme::Get(Theme::Role::Accent);
			nvgStrokeColor(vg, nvgRGBA(accentCol.Red(), accentCol.Green(), accentCol.Blue(), 255));
			nvgStrokeWidth(vg, 2.0f);
			nvgStroke(vg);
		}

		// Draw Item Icon (fixed icon area, size follows the view options)
		uint16_t id = GetItem(i);
		const auto definition = g_item_definitions.get(id);
		const uint16_t cid = definition ? definition.clientId() : 0;
		const auto tech = rme::rendering::TechnicalItemRegistry::Classify(id, cid);
		const float iconSize = static_cast<float>(m_iconSize);
		if (tech != rme::rendering::TileIndicatorType::None) {
			const float bx = x + (w - iconSize) / 2.0f;
			const float by = y + static_cast<float>(kIconTopPadding);
			rme::rendering::DrawNanoVGIndicatorBadge(vg, tech, bx, by, iconSize);
		} else {
			int tex = GetOrCreateItemTexture(vg, id);
			if (tex > 0) {
				int tw, th;
				nvgImageSize(vg, tex, &tw, &th);

				float scale = iconSize / std::max(tw, th);
				if (scale > 1.0f && std::max(tw, th) >= 32) {
					scale = 1.0f;
				}

				float dw = tw * scale;
				float dh = th * scale;
				float dx = x + (w - dw) / 2.0f;
				float dy = y + static_cast<float>(kIconTopPadding);

				NVGpaint imgPaint = nvgImagePattern(vg, dx, dy, dw, dh, 0.0f, tex, 1.0f);
				nvgBeginPath(vg);
				nvgRect(vg, dx, dy, dw, dh);
				nvgFillPaint(vg, imgPaint);
				nvgFill(vg);
			}
		}

		// Draw the item name under the icon when labels are enabled
		wxString name = GetItemName(i);
		if (m_showLabels && !name.IsEmpty()) {
			nvgFontSize(vg, 12.0f); // Larger font
			nvgFontFace(vg, "sans");
			nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
			if (isSelected) {
				wxColour textOnAccent = Theme::Get(Theme::Role::TextOnAccent);
				nvgFillColor(vg, nvgRGBA(textOnAccent.Red(), textOnAccent.Green(), textOnAccent.Blue(), 255));
			} else {
				wxColour textCol = Theme::Get(Theme::Role::TextSubtle);
				nvgFillColor(vg, nvgRGBA(textCol.Red(), textCol.Green(), textCol.Blue(), 230));
			}

			float textY = y + static_cast<float>(kIconTopPadding + m_iconSize + kIconBottomPadding);
			float maxWidth = w - 4.0f;

			// NvgTextBox approach for wrapping
			nvgTextBox(vg, x + 2, textY, maxWidth, name.mb_str(), nullptr);
		}
	}
}
