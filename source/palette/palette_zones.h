//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_PALETTE_ZONES_H_
#define RME_PALETTE_ZONES_H_

#include "palette/palette_common.h"
#include "palette/controls/tileset_grid_toolbar.h"
#include "map/map.h"

#include <unordered_set>
#include <string>
#include <vector>

class Brush;

class ZonePalettePanel : public PalettePanel {
public:
	explicit ZonePalettePanel(wxWindow* parent, wxWindowID id = wxID_ANY);
	~ZonePalettePanel() override;

	wxString GetName() const override;

	void SelectFirstBrush() override;
	Brush* GetSelectedBrush() const override;
	int GetSelectedBrushSize() const override;
	bool SelectBrush(const Brush* whatbrush) override;

	void OnUpdate() override;
	void OnSwitchIn() override;
	void OnSwitchOut() override;

	void SetMap(Map* map);

	void UpdateZone(uint16_t zone_id = 0) { OnUpdate(); }
	void UpdateTileCounts() { OnUpdate(); }

	void SelectZoneById(uint16_t zone_id, int cluster = -1);
	[[nodiscard]] uint16_t GetSelectedZoneId() const { return selected_zone_id_; }

	// Actions
	void OnClickAddZone();
	void OnClickRenameZone();
	void OnClickRemoveZone();
	void OnGoToZone();

	void ExpandAll();
	void CollapseAll();

private:
	class ZoneCanvas;

	Map* map_ = nullptr;
	uint16_t selected_zone_id_ = 0;
	int selected_cluster_ = -1;

	TilesetGridToolbar* toolbar_ = nullptr;
	ZoneCanvas* canvas_ = nullptr;

	wxButton* add_button_ = nullptr;
	wxButton* rename_button_ = nullptr;
	wxButton* remove_button_ = nullptr;
};

#endif // RME_PALETTE_ZONES_H_
