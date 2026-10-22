#ifndef RME_UI_CONTROLS_COLLECTION_ITEM_GRID_H_
#define RME_UI_CONTROLS_COLLECTION_ITEM_GRID_H_

#include "util/virtual_item_grid.h"
#include <functional>
#include <string>
#include <vector>

namespace rme::ui {

class CollectionItemGrid : public VirtualItemGrid {
public:
	explicit CollectionItemGrid(wxWindow* parent, wxWindowID id = wxID_ANY);
	~CollectionItemGrid() override = default;

	void SetItems(const std::vector<uint16_t>& items);
	void SetFilter(const std::string& query);
	void SetSort(TilesetSortKey key, TilesetSortDirection dir);
	void ClearSort();
	void SetDiscardCallback(std::function<void(uint16_t)> cb) {
		m_discardCallback = std::move(cb);
	}

	size_t GetItemCount() const override {
		return m_filteredItems.size();
	}

	uint16_t GetItem(size_t index) const override {
		return (index < m_filteredItems.size()) ? m_filteredItems[index] : 0;
	}

	const std::vector<uint16_t>& GetItems() const {
		return m_allItems;
	}

protected:
	void OnNanoVGPaint(NVGcontext* vg, int width, int height) override;
	void OnMotion(wxMouseEvent& event) override;
	void OnMouseDown(wxMouseEvent& event) override;
	void OnLeave(wxMouseEvent& event) override;

private:
	void ApplyFilterAndSort();

	std::vector<uint16_t> m_allItems;
	std::vector<uint16_t> m_filteredItems;
	std::string m_filterQuery;
	bool m_badgeHovered = false;
	std::function<void(uint16_t)> m_discardCallback;
};

} // namespace rme::ui

#endif // RME_UI_CONTROLS_COLLECTION_ITEM_GRID_H_
