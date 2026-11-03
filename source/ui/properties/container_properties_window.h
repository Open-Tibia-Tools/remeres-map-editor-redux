//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_CONTAINER_PROPERTIES_WINDOW_H_
#define RME_CONTAINER_PROPERTIES_WINDOW_H_

#include "app/main.h"
#include "game/item.h"
#include "ui/properties/object_properties_base.h"

#include <memory>
#include <vector>

class ContainerGridCanvas;

class ContainerPropertiesWindow : public ObjectPropertiesWindowBase {
public:
	ContainerPropertiesWindow(wxWindow* parent, const Map* map, const Tile* tile, Item* item, wxPoint pos = wxDefaultPosition);
	virtual ~ContainerPropertiesWindow();

	void OnClickOK(wxCommandEvent&);
	void OnClickCancel(wxCommandEvent&);
	void Update() override;
	void OnContainerItemClick(wxCommandEvent& event);
	void OnContainerItemRightClick(wxContextMenuEvent& event);

	void OnAddItem(wxCommandEvent& event);
	void OnEditItem(wxCommandEvent& event);
	void OnRemoveItem(wxCommandEvent& event);

protected:
	wxSpinCtrl* action_id_field;
	wxSpinCtrl* unique_id_field;

	ContainerGridCanvas* grid_canvas;

	std::vector<std::unique_ptr<Item>> backup_contents;
	bool m_committed{false};
};

#endif
