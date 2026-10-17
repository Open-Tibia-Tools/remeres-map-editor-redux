#ifndef RME_PALETTE_ZONES_H_
#define RME_PALETTE_ZONES_H_

#include "palette/palette_common.h"
#include "palette/controls/palette_search_ctrl.h"
#include "util/nanovg_table_ctrl.h"

#include <unordered_set>
#include <vector>

struct ZoneCluster;

// Lists OTBM 5 zones (Canary / Crystal Server) with live vector swatches and feeds the selected id to the zone brush
class ZonePalettePanel : public PalettePanel {
public:
	explicit ZonePalettePanel(wxWindow* parent, wxWindowID id = wxID_ANY);
	~ZonePalettePanel() override = default;

	wxString GetName() const override;

	void SelectFirstBrush() override { }
	Brush* GetSelectedBrush() const override;
	int GetSelectedBrushSize() const override {
		return 0;
	}
	bool SelectBrush(const Brush* whatbrush) override;
	void OnUpdate() override;
	void OnSwitchIn() override;

	void SetMap(Map* map);

	void UpdateZone(uint16_t zone_id);
	void UpdateTileCounts();

	// Table callbacks
	void OnSelectZone(int index, const TableRow& row);
	void OnActivateZone(int index, const TableRow& row);
	void OnSortZones(int column, bool ascending);
	void OnZoneContextMenu(int index, const TableRow* row, const wxPoint& pos);
	void OnToggleGroup(int index, const TableRow& row);

	// Event handlers
	void OnSearchChange(wxCommandEvent& event);
	void OnGoToZone(wxCommandEvent& event);
	void OnClickAdd(wxCommandEvent& event);
	void OnClickRename(wxCommandEvent& event);
	void OnClickRemove(wxCommandEvent& event);

private:
	[[nodiscard]] bool supportsZones() const;
	[[nodiscard]] uint16_t selectedZoneId() const;

	struct RowRef {
		uint16_t zone = 0;
		int cluster = -1;
	};
	[[nodiscard]] RowRef selectedRow() const;

	Map* map = nullptr;
	std::unordered_set<uint16_t> expanded_zones_;
	uint16_t selected_zone_id_ = 0;
	int selected_cluster_ = -1;

	PaletteSearchCtrl* search_ctrl = nullptr;
	NanoVGTableCtrl* zone_table = nullptr;
	wxButton* add_button = nullptr;
	wxButton* rename_button = nullptr;
	wxButton* remove_button = nullptr;
};

#endif

