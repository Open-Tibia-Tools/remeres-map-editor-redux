#ifndef RME_FIND_ITEM_WINDOW_VIEWS_H_
#define RME_FIND_ITEM_WINDOW_VIEWS_H_

#include "ui/find_item_window_model.h"
#include "util/nanovg_canvas.h"

#include <wx/event.h>
#include <wx/wx.h>

#include <string>
#include <vector>

class Sprite;
struct NVGcontext;

wxDECLARE_EVENT(EVT_ADVANCED_FINDER_RESULT_RIGHT_ACTIVATE, wxCommandEvent);

enum class AdvancedFinderResultViewMode : uint8_t {
	List = 0,
	Grid,
};

class AdvancedFinderResultsView final : public NanoVGCanvas {
public:
	AdvancedFinderResultsView(wxWindow* parent, wxWindowID id);

	void SetMode(AdvancedFinderResultViewMode mode);
	[[nodiscard]] AdvancedFinderResultViewMode GetMode() const {
		return mode_;
	}

	void SetRows(std::vector<const AdvancedFinderCatalogRow*> rows, const AdvancedFinderSelectionKey& preferred_selection);
	void SetPrompt(std::string primary, std::string secondary);
	void SetNoMatches(std::string primary, std::string secondary);

	[[nodiscard]] const AdvancedFinderCatalogRow* GetSelectedRow() const;
	[[nodiscard]] int GetSelectionIndex() const;
	void SetSelectionIndex(int index);
	void EnsureSelectionVisible();

protected:
	void OnNanoVGPaint(NVGcontext* vg, int width, int height) override;

private:
	enum class EmptyState : uint8_t {
		Prompt = 0,
		NoMatches,
		Rows,
	};

	void bindEvents();
	void onSize(wxSizeEvent& event);
	void onMouseDown(wxMouseEvent& event);
	void onMouseMove(wxMouseEvent& event);
	void onMouseLeave(wxMouseEvent& event);
	void onMouseWheel(wxMouseEvent& event);
	void onKeyDown(wxKeyEvent& event);
	void onLeftDoubleClick(wxMouseEvent& event);
	void onRightDoubleClick(wxMouseEvent& event);
	void onContextMenu(wxContextMenuEvent& event);

	void updateLayoutMetrics(int width);
	void updateScrollbarForLayout();
	void setSelectionInternal(int index, bool notify);
	void selectDefaultRow(const AdvancedFinderSelectionKey& preferred_selection);
	void refreshNow();
	void sendSelectionEvent();
	void sendActivateEvent();

	[[nodiscard]] int hitTest(int x, int y) const;
	[[nodiscard]] wxRect itemRect(size_t index) const;
	[[nodiscard]] Sprite* spriteForRow(const AdvancedFinderCatalogRow& row) const;
	[[nodiscard]] int columnsForWidth(int width) const;
	[[nodiscard]] int listHeaderHeight() const;
	[[nodiscard]] int listItemHeight() const;
	[[nodiscard]] int gridCardWidth() const;
	[[nodiscard]] int gridCardHeight() const;
	[[nodiscard]] int gridPadding() const;
	[[nodiscard]] int gridGap() const;
	[[nodiscard]] int visibleRows() const;

	void drawEmptyState(NVGcontext* vg, int width, int height) const;
	void drawListRow(NVGcontext* vg, const wxRect& rect, const AdvancedFinderCatalogRow& row, bool selected, bool hovered) const;
	void drawGridCard(NVGcontext* vg, const wxRect& rect, const AdvancedFinderCatalogRow& row, bool selected, bool hovered) const;
	void drawGridHoverInfo(NVGcontext* vg, int width, int height, int scroll_pos) const;
	void drawSpriteBadge(NVGcontext* vg, const wxRect& rect, Sprite* sprite) const;
	void drawRowBadge(NVGcontext* vg, const wxRect& rect, const AdvancedFinderCatalogRow& row) const;

	EmptyState empty_state_ = EmptyState::Prompt;
	std::string primary_message_;
	std::string secondary_message_;
	std::vector<const AdvancedFinderCatalogRow*> rows_;
	AdvancedFinderResultViewMode mode_ = AdvancedFinderResultViewMode::List;
	int selected_index_ = -1;
	int hover_index_ = -1;
	int columns_ = 1;
	int cached_width_ = -1;
	int cached_height_ = -1;
	int list_item_height_ = 0;
	int card_width_ = 0;
	int card_height_ = 0;
	int padding_ = 0;
	int gap_ = 0;
	wxPoint hover_position_ = wxDefaultPosition;
};

#endif
