//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_PALETTE_TOWNS_H_
#define RME_PALETTE_TOWNS_H_

#include "palette/palette_common.h"
#include "palette/controls/tileset_grid_toolbar.h"
#include "game/town.h"

#include <string>
#include <vector>

class Map;
class Town;
class Brush;

class TownPalettePanel : public PalettePanel {
public:
	explicit TownPalettePanel(wxWindow* parent, wxWindowID id = wxID_ANY);
	~TownPalettePanel() override;

	wxString GetName() const override;

	void SelectFirstBrush() override;
	Brush* GetSelectedBrush() const override;
	int GetSelectedBrushSize() const override;
	bool SelectBrush(const Brush* whatbrush) override;

	void OnUpdate() override;
	void OnSwitchIn() override;
	void OnSwitchOut() override;

	void SetMap(Map* map);

	void SelectTownById(uint32_t town_id);
	Town* GetSelectedTown() const;

	// Actions
	void OnClickAddTown();
	void OnClickEditTown();
	void OnClickRemoveTown();
	void OnClickChangeTemple();
	void OnGoToTemple();

protected:
	bool RunEditTownModal(wxWindow* parent, Map* map, Town* town, bool is_new);

private:
	class TownCanvas;

	Map* map_ = nullptr;
	uint32_t selected_town_id_ = 0;

	TilesetGridToolbar* toolbar_ = nullptr;
	TownCanvas* canvas_ = nullptr;
	wxButton* add_button_ = nullptr;
	wxButton* edit_button_ = nullptr;
	wxButton* remove_button_ = nullptr;
	wxButton* change_temple_button_ = nullptr;
};

#endif // RME_PALETTE_TOWNS_H_
