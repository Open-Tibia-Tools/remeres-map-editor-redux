//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_PALETTE_ZONES_H_
#define RME_PALETTE_ZONES_H_

#include <wx/listctrl.h>

#include "palette/palette_common.h"

// Lists Crystal Server zones (OTBM 5 maps only) and feeds the selected id to the zone brush
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

	void SetMap(Map* map);

private:
	[[nodiscard]] bool supportsZones() const;
	[[nodiscard]] uint16_t selectedZoneId() const;

	void OnSelectZone(wxListEvent& event);
	void OnClickAdd(wxCommandEvent& event);
	void OnClickRename(wxCommandEvent& event);
	void OnClickRemove(wxCommandEvent& event);

	Map* map = nullptr;
	bool refreshing = false;
	wxListCtrl* zone_list;
	wxButton* add_button;
	wxButton* rename_button;
	wxButton* remove_button;
};

#endif
