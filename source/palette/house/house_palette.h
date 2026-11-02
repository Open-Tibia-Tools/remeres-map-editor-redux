//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_HOUSE_PALETTE_H_
#define RME_HOUSE_PALETTE_H_

#include "palette/palette_common.h"
#include "palette/controls/tileset_grid_toolbar.h"
#include "game/house.h"

#include <wx/choice.h>
#include <wx/tglbtn.h>
#include <string>
#include <vector>

class Map;
class House;
class Brush;

class HousePalette : public PalettePanel {
public:
	explicit HousePalette(wxWindow* parent, wxWindowID id = wxID_ANY);
	~HousePalette() override;

	wxString GetName() const override;

	void SelectFirstBrush() override;
	Brush* GetSelectedBrush() const override;
	int GetSelectedBrushSize() const override;
	bool SelectBrush(const Brush* whatbrush) override;

	void OnUpdate() override;
	void OnSwitchIn() override;
	void OnSwitchOut() override;

	void SetMap(Map* map);
	void UpdateHouses();
	void UpdateHouseSize(uint32_t house_id = 0);

	void SelectHouseById(uint32_t house_id);
	House* GetSelectedHouse() const;

	void SelectHouseBrush();
	void SelectExitBrush();

	// Actions
	void OnTownFilterChange(wxCommandEvent& event);
	void OnClickAddHouse();
	void OnClickEditHouse();
	void OnClickRemoveHouse();
	void OnGoToExit();

private:
	class HouseCanvas;

	Map* map_ = nullptr;
	uint32_t selected_house_id_ = 0;

	wxChoice* town_choice_ = nullptr;
	TilesetGridToolbar* toolbar_ = nullptr;
	HouseCanvas* canvas_ = nullptr;

	wxButton* add_button_ = nullptr;
	wxButton* edit_button_ = nullptr;
	wxButton* remove_button_ = nullptr;
	wxToggleButton* house_brush_toggle_ = nullptr;
	wxToggleButton* exit_brush_toggle_ = nullptr;
};

#endif // RME_HOUSE_PALETTE_H_
