#include "app/main.h"

#include "ui/result_window.h"

#include "ui/gui.h"
#include "ui/theme.h"
#include "util/image_manager.h"
#include "util/nanovg_canvas.h"
#include "util/nvg_utils.h"

#include <nanovg.h>

#include <algorithm>
#include <limits>

#include <wx/file.h>
#include <wx/filedlg.h>
#include <wx/stattext.h>

namespace {
	constexpr int kEmptyStateInset = 18;
}

class SearchResultWindow::ResultCanvas final : public NanoVGCanvas {
public:
	explicit ResultCanvas(SearchResultWindow* parent) :
		NanoVGCanvas(parent, wxID_ANY, wxVSCROLL | wxWANTS_CHARS),
		owner_(parent) {
		SetBackgroundColour(Theme::Get(Theme::Role::Surface));
		SetScrollStep(rowHeight());

		Bind(wxEVT_SIZE, &ResultCanvas::onSize, this);
		Bind(wxEVT_LEFT_DOWN, &ResultCanvas::onMouseDown, this);
		Bind(wxEVT_LEFT_DCLICK, &ResultCanvas::onLeftDoubleClick, this);
		Bind(wxEVT_MOTION, &ResultCanvas::onMouseMove, this);
		Bind(wxEVT_LEAVE_WINDOW, &ResultCanvas::onMouseLeave, this);
		Bind(wxEVT_KEY_DOWN, &ResultCanvas::onKeyDown, this);
	}

	void RefreshContent() {
		const int content_height = static_cast<int>(owner_->rows_.size()) * rowHeight();
		UpdateScrollbar(content_height);
		if (selected_index_ >= static_cast<int>(owner_->rows_.size())) {
			selected_index_ = owner_->rows_.empty() ? -1 : static_cast<int>(owner_->rows_.size()) - 1;
		}
		Refresh();
	}

	void ClearSelection() {
		selected_index_ = -1;
		hover_index_ = -1;
		SetScrollPosition(0);
		RefreshContent();
	}

	void SetSelection(int index, bool ensure_visible, bool activate) {
		if (owner_->rows_.empty()) {
			selected_index_ = -1;
			RefreshContent();
			return;
		}

		index = std::clamp(index, 0, static_cast<int>(owner_->rows_.size()) - 1);
		selected_index_ = index;
		if (ensure_visible) {
			EnsureSelectionVisible();
		}
		Refresh();

		if (activate) {
			owner_->activateRow(index);
		}
	}

	void EnsureSelectionVisible() {
		if (selected_index_ < 0 || selected_index_ >= static_cast<int>(owner_->rows_.size())) {
			return;
		}

		const wxRect rect = rowRect(selected_index_);
		const int scroll_pos = GetScrollPosition();
		const int view_height = GetClientSize().y;

		if (rect.y < scroll_pos) {
			SetScrollPosition(rect.y);
		} else if (rect.GetBottom() > scroll_pos + view_height) {
			SetScrollPosition(rect.GetBottom() - view_height);
		}
	}

protected:
	void OnNanoVGPaint(NVGcontext* vg, int width, int height) override {
		const wxColour surface = Theme::Get(Theme::Role::Surface);
		nvgBeginPath(vg);
		nvgRect(vg, 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
		nvgFillColor(vg, NvgUtils::ToNvColor(surface));
		nvgFill(vg);

		if (owner_->rows_.empty()) {
			drawEmptyState(vg, width, height);
			return;
		}

		const int scroll_pos = GetScrollPosition();
		const int row_height = rowHeight();
		const int start_index = std::max(0, scroll_pos / row_height);
		const int end_index = std::min(
			static_cast<int>(owner_->rows_.size()),
			(scroll_pos + height + row_height - 1) / row_height + 1
		);

		for (int index = start_index; index < end_index; ++index) {
			drawRow(vg, width, rowRect(index), owner_->rows_[static_cast<size_t>(index)], index == selected_index_, index == hover_index_);
		}
	}

private:
	void onSize(wxSizeEvent& event) {
		RefreshContent();
		event.Skip();
	}

	void onMouseDown(wxMouseEvent& event) {
		if (const int index = hitTest(event.GetX(), event.GetY()); index >= 0) {
			SetSelection(index, false, true);
		}
		SetFocus();
	}

	void onLeftDoubleClick(wxMouseEvent& event) {
		onMouseDown(event);
	}

	void onMouseMove(wxMouseEvent& event) {
		const int index = hitTest(event.GetX(), event.GetY());
		if (index != hover_index_) {
			hover_index_ = index;
			Refresh();
		}
		event.Skip();
	}

	void onMouseLeave(wxMouseEvent& event) {
		if (hover_index_ != -1) {
			hover_index_ = -1;
			Refresh();
		}
		event.Skip();
	}

	void onKeyDown(wxKeyEvent& event) {
		if (owner_->rows_.empty()) {
			event.Skip();
			return;
		}

		int next_index = selected_index_ >= 0 ? selected_index_ : 0;
		switch (event.GetKeyCode()) {
			case WXK_UP:
				next_index -= 1;
				break;
			case WXK_DOWN:
				next_index += 1;
				break;
			case WXK_PAGEUP:
				next_index -= std::max(1, GetClientSize().y / std::max(1, rowHeight()));
				break;
			case WXK_PAGEDOWN:
				next_index += std::max(1, GetClientSize().y / std::max(1, rowHeight()));
				break;
			case WXK_HOME:
				next_index = 0;
				break;
			case WXK_END:
				next_index = static_cast<int>(owner_->rows_.size()) - 1;
				break;
			case WXK_RETURN:
			case WXK_NUMPAD_ENTER:
				if (selected_index_ >= 0) {
					owner_->activateRow(selected_index_);
				}
				return;
			default:
				event.Skip();
				return;
		}

		SetSelection(next_index, true, false);
	}

	[[nodiscard]] int hitTest(int x, int y) const {
		if (owner_->rows_.empty()) {
			return -1;
		}

		const int content_y = y + GetScrollPosition();
		const int index = content_y / std::max(1, rowHeight());
		if (index < 0 || index >= static_cast<int>(owner_->rows_.size())) {
			return -1;
		}

		return index;
	}

	[[nodiscard]] wxRect rowRect(int index) const {
		return wxRect(0, index * rowHeight(), GetClientSize().x, rowHeight());
	}

	[[nodiscard]] int rowHeight() const {
		return FromDIP(36);
	}

	[[nodiscard]] int leftInset() const {
		return FromDIP(10);
	}

	[[nodiscard]] int indexWidth() const {
		return FromDIP(68);
	}

	void drawEmptyState(NVGcontext* vg, int width, int height) const {
		nvgFontFace(vg, "sans");
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

		nvgFontSize(vg, 14.0f);
		nvgFillColor(vg, NvgUtils::ToNvColor(Theme::Get(Theme::Role::TextSubtle)));
		nvgText(vg, static_cast<float>(FromDIP(kEmptyStateInset)), height * 0.45f, "No search results", nullptr);

		nvgFontSize(vg, 12.0f);
		nvgFillColor(vg, NvgUtils::ToNvColor(Theme::Get(Theme::Role::TextSubtle)));
		nvgText(vg, static_cast<float>(FromDIP(kEmptyStateInset)), height * 0.54f, "Run a map search to populate this list.", nullptr);
	}

	void drawRow(NVGcontext* vg, int width, const wxRect& rect, const SearchResultRow& row, bool selected, bool hovered) const {
		const wxColour row_fill = Theme::Get(Theme::Role::Surface);
		const wxColour hover_fill = Theme::Get(Theme::Role::CardBaseHover);
		const wxColour selected_fill = Theme::Get(Theme::Role::Accent);
		const wxColour separator = Theme::Get(Theme::Role::Border);
		const wxColour name_text = selected ? Theme::Get(Theme::Role::TextOnAccent) : Theme::Get(Theme::Role::Text);
		const wxColour meta_text = selected ? Theme::Get(Theme::Role::TextOnAccent) : Theme::Get(Theme::Role::TextSubtle);

		const wxColour fill = selected ? selected_fill : (hovered ? hover_fill : row_fill);
		nvgBeginPath(vg);
		nvgRect(vg, static_cast<float>(rect.x), static_cast<float>(rect.y), static_cast<float>(rect.width), static_cast<float>(rect.height));
		nvgFillColor(vg, NvgUtils::ToNvColor(fill));
		nvgFill(vg);

		if (selected) {
			nvgBeginPath(vg);
			nvgRect(vg, 0.0f, static_cast<float>(rect.y), static_cast<float>(FromDIP(3)), static_cast<float>(rect.height));
			nvgFillColor(vg, NvgUtils::ToNvColor(Theme::Get(Theme::Role::AccentHover)));
			nvgFill(vg);
		}

		nvgBeginPath(vg);
		nvgMoveTo(vg, 0.0f, static_cast<float>(rect.GetBottom()));
		nvgLineTo(vg, static_cast<float>(width), static_cast<float>(rect.GetBottom()));
		nvgStrokeWidth(vg, 1.0f);
		nvgStrokeColor(vg, NvgUtils::ToNvColor(separator));
		nvgStroke(vg);

		nvgFontFace(vg, "sans");
		const float left = static_cast<float>(leftInset());
		const float index_width = static_cast<float>(indexWidth());
		const float text_left = left + index_width;
		const float text_width = static_cast<float>(width) - text_left - left;
		const float name_y = static_cast<float>(rect.y + FromDIP(12));
		const float coords_y = static_cast<float>(rect.y + rect.height - FromDIP(10));

		nvgFillColor(vg, NvgUtils::ToNvColor(meta_text));
		nvgFontSize(vg, 11.0f);
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgText(vg, left, static_cast<float>(rect.y + rect.height / 2), wxString::Format("#%u", row.index).utf8_str(), nullptr);

		nvgFillColor(vg, NvgUtils::ToNvColor(name_text));
		nvgFontSize(vg, 12.0f);
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgSave(vg);
		nvgScissor(vg, text_left, static_cast<float>(rect.y), text_width, static_cast<float>(rect.height));
		nvgText(vg, text_left, name_y, row.name.utf8_str(), nullptr);
		nvgRestore(vg);

		const wxString coords = wxString::Format("[%d, %d, %d]", row.position.x, row.position.y, row.position.z);
		nvgFillColor(vg, NvgUtils::ToNvColor(meta_text));
		nvgFontSize(vg, 10.0f);
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgSave(vg);
		nvgScissor(vg, text_left, static_cast<float>(rect.y), text_width, static_cast<float>(rect.height));
		nvgText(vg, text_left, coords_y, coords.utf8_str(), nullptr);
		nvgRestore(vg);
	}

	SearchResultWindow* owner_ = nullptr;
	int selected_index_ = -1;
	int hover_index_ = -1;
};

SearchResultWindow::SearchResultWindow(wxWindow* parent) :
	wxPanel(parent, wxID_ANY) {
	auto* root_sizer = newd wxBoxSizer(wxVERTICAL);

	result_list = newd ResultCanvas(this);
	root_sizer->Add(result_list, wxSizerFlags(1).Expand());

	auto* footer_sizer = newd wxBoxSizer(wxVERTICAL);
	summary_label = newd wxStaticText(this, wxID_ANY, "Results: 0");
	footer_sizer->Add(summary_label, wxSizerFlags(0).Expand());

	auto* action_sizer = newd wxBoxSizer(wxHORIZONTAL);

	previous_page_button_ = newd wxButton(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
	previous_page_button_->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_ARROW_LEFT));
	action_sizer->Add(previous_page_button_, wxSizerFlags(0).CenterVertical().Border(wxRIGHT, FromDIP(6)));

	next_page_button_ = newd wxButton(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
	next_page_button_->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_ARROW_RIGHT));
	action_sizer->Add(next_page_button_, wxSizerFlags(0).CenterVertical().Border(wxRIGHT, FromDIP(12)));

	export_button_ = newd wxButton(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
	export_button_->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_FILE_EXPORT));
	export_button_->SetToolTip("Export current page");
	action_sizer->Add(export_button_, wxSizerFlags(0).CenterVertical().Border(wxRIGHT, FromDIP(6)));

	clear_button_ = newd wxButton(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
	clear_button_->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_TRASH_CAN));
	clear_button_->SetToolTip("Clear results");
	action_sizer->Add(clear_button_, wxSizerFlags(0).CenterVertical());

	footer_sizer->Add(action_sizer, wxSizerFlags(0).Expand().Top());

	root_sizer->Add(footer_sizer, wxSizerFlags(0).Expand().DoubleBorder());
	SetSizerAndFit(root_sizer);

	previous_page_button_->Bind(wxEVT_BUTTON, &SearchResultWindow::OnClickPreviousPage, this);
	next_page_button_->Bind(wxEVT_BUTTON, &SearchResultWindow::OnClickNextPage, this);
	export_button_->Bind(wxEVT_BUTTON, &SearchResultWindow::OnClickExport, this);
	clear_button_->Bind(wxEVT_BUTTON, &SearchResultWindow::OnClickClear, this);

	updateSummary();
}

SearchResultWindow::~SearchResultWindow() = default;

void SearchResultWindow::Clear() {
	rows_.clear();
	total_count_ = 0;
	page_offset_ = 0;
	page_limit_ = 0;
	page_loader_ = {};

	if (result_list != nullptr) {
		result_list->ClearSelection();
	}
	updateSummary();
}

void SearchResultWindow::SetResults(std::vector<SearchResultRow> rows, uint32_t total_count, uint32_t page_offset, uint32_t page_limit, std::function<void(uint32_t)> page_loader) {
	rows_ = std::move(rows);
	total_count_ = total_count == 0 ? static_cast<uint32_t>(rows_.size()) : total_count;
	page_offset_ = page_offset;
	page_limit_ = page_limit == 0 ? static_cast<uint32_t>(rows_.size()) : page_limit;
	page_loader_ = std::move(page_loader);

	if (result_list != nullptr) {
		result_list->RefreshContent();
	}
	focusFirstResult();
	updateSummary();
}

void SearchResultWindow::activateRow(int index) {
	if (index < 0 || index >= static_cast<int>(rows_.size())) {
		return;
	}

	g_gui.SetScreenCenterPosition(rows_[static_cast<size_t>(index)].position);
}

void SearchResultWindow::OnClickExport(wxCommandEvent& WXUNUSED(event)) {
	wxFileDialog dialog(this, "Save file...", "", "", "Text Documents (*.txt) | *.txt", wxFD_SAVE);
	if (dialog.ShowModal() != wxID_OK) {
		return;
	}

	wxFile file(dialog.GetPath(), wxFile::write);
	if (!file.IsOpened()) {
		return;
	}

	g_gui.CreateLoadBar("Exporting search result...");
	file.Write("Generated by Remere's Map Editor " + __RME_VERSION__);
	file.Write("\n=============================================\n\n");

	const size_t count = rows_.size();
	for (size_t i = 0; i < count; ++i) {
		const SearchResultRow& row = rows_[i];
		file.Write(wxString::Format("%u\t%s\t%d\t%d\t%d\n", row.index, row.name, row.position.x, row.position.y, row.position.z));
		const auto from = static_cast<int32_t>(std::min<size_t>(i, static_cast<size_t>(std::numeric_limits<int32_t>::max())));
		const auto to = static_cast<int32_t>(std::min<size_t>(count, static_cast<size_t>(std::numeric_limits<int32_t>::max())));
		g_gui.SetLoadScale(from, to);
	}

	file.Close();
	g_gui.DestroyLoadBar();
}

void SearchResultWindow::OnClickClear(wxCommandEvent& WXUNUSED(event)) {
	Clear();
}

void SearchResultWindow::OnClickPreviousPage(wxCommandEvent& WXUNUSED(event)) {
	if (!page_loader_ || page_limit_ == 0 || page_offset_ == 0) {
		return;
	}

	const uint32_t new_offset = (page_offset_ > page_limit_) ? (page_offset_ - page_limit_) : 0;
	page_loader_(new_offset);
}

void SearchResultWindow::OnClickNextPage(wxCommandEvent& WXUNUSED(event)) {
	if (!page_loader_ || page_limit_ == 0) {
		return;
	}

	const uint32_t current_end = page_offset_ + static_cast<uint32_t>(rows_.size());
	if (current_end >= total_count_) {
		return;
	}

	page_loader_(page_offset_ + page_limit_);
}

void SearchResultWindow::updateSummary() {
	if (summary_label == nullptr) {
		return;
	}

	if (rows_.empty() || total_count_ == 0) {
		summary_label->SetLabel("Results: 0");
	} else {
		const uint32_t first_visible = std::min(total_count_, page_offset_ + 1);
		const uint32_t last_visible = std::min(total_count_, page_offset_ + static_cast<uint32_t>(rows_.size()));
		summary_label->SetLabel(wxString::Format("Results: %u | %u-%u", total_count_, first_visible, last_visible));
	}

	if (previous_page_button_ != nullptr) {
		previous_page_button_->Enable(page_loader_ && page_offset_ > 0);
	}
	if (next_page_button_ != nullptr) {
		const uint32_t current_end = page_offset_ + static_cast<uint32_t>(rows_.size());
		next_page_button_->Enable(page_loader_ && current_end < total_count_);
	}
	if (export_button_ != nullptr) {
		export_button_->Enable(!rows_.empty());
	}
	if (clear_button_ != nullptr) {
		clear_button_->Enable(!rows_.empty());
	}

	summary_label->SetMinSize(summary_label->GetBestSize());
	Layout();
}

void SearchResultWindow::focusFirstResult() {
	if (result_list == nullptr || rows_.empty()) {
		return;
	}

	result_list->SetSelection(0, true, false);
}
