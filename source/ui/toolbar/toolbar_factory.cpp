//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "ui/toolbar/toolbar_factory.h"

std::unique_ptr<ToolbarRegistry> ToolbarFactory::CreateToolbars(wxWindow* parent) {
	auto registry = std::make_unique<ToolbarRegistry>();

	// Use new macro as per project convention if applicable, or just new.
	// The original code used `new`.

	// Toolbars are owned by 'parent' (MainFrame) via wxWindow mechanism.
	// ToolbarRegistry holds non-owning pointers for access.
	registry->SetStandardToolbar(new StandardToolBar(parent));
	registry->SetBrushToolbar(new BrushToolBar(parent));
	registry->SetPositionToolbar(new PositionToolBar(parent));
	registry->SetSizeToolbar(new SizeToolBar(parent));
	registry->SetLightToolbar(new LightToolBar(parent));

	return registry;
}
