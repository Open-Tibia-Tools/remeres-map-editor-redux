//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "ui/managers/layout_manager.h"
#include "app/managers/version_manager.h"
#include "ui/gui.h"
#include "app/application.h"
#include "palette/palette_window.h"
#include "palette/house/house_palette.h"
#include "palette/managers/palette_manager.h"

#include "rendering/ui/minimap_window.h"
#include "ui/main_menubar.h"
#include "ui/main_toolbar.h"
#include <wx/display.h>
#include <algorithm>
#include "ui/managers/minimap_manager.h"
#include "ingame_preview/ingame_preview_manager.h"
#include "ingame_preview/ingame_preview_window.h"
#include "ui/managers/status_manager.h"
#include "ui/tool_options_window.h"
#include "ui/brush_items/brush_items_window.h"
#include "ui/tile_properties/tile_properties_panel.h"
#include "lua/lua_scripts_window.h"

LayoutManager g_layout;

namespace {
	// Dragging a sidebar's edge resizes the wxAUI dock, not the pane, and
	// SavePaneInfo() only writes the pane, so the width was lost on restart.
	// wxAUI sizes a new dock from its panes' best sizes, so record each docked
	// pane's current size as its best size before saving it.
	wxString SavePaneInfoWithCurrentSize(wxAuiManager& manager, wxWindow* window) {
		wxAuiPaneInfo& info = manager.GetPane(window);
		if (info.IsOk() && info.IsDocked() && info.IsShown() && !info.IsMaximized()) {
			const wxSize size = window->GetSize();
			if (size.x > 0 && size.y > 0) {
				info.BestSize(size);
			}
		}
		return manager.SavePaneInfo(info);
	}

} // namespace

void LayoutManager::EnsurePaneOnScreen(wxAuiPaneInfo& info) {
	if (!info.IsFloating()) {
		return;
	}

	const int display_index = wxDisplay::GetFromPoint(info.floating_pos);
	if (display_index == wxNOT_FOUND) {
		info.Dock();
		return;
	}

	// AUI gives the client size precedence. Without a frame we cannot convert it to
	// frame dimensions, so keep the saved values for the post-Update pass.
	if (info.frame == nullptr && info.floating_client_size != wxDefaultSize) {
		return;
	}

	wxSize size = info.floating_size;
	if (info.frame != nullptr && info.floating_client_size != wxDefaultSize) {
		size = info.frame->ClientToWindowSize(info.floating_client_size);
	}
	if (size == wxDefaultSize) {
		size = info.best_size;
	}
	size.x = std::max(size.x, info.min_size.x);
	size.y = std::max(size.y, info.min_size.y);
	if (size.x <= 0 || size.y <= 0) {
		return;
	}

	const wxRect display = wxDisplay(display_index).GetClientArea();
	const auto fitted = rme::ui::layout::FitPaneInDisplay(
		{ info.floating_pos.x, info.floating_pos.y, size.x, size.y },
		{ display.x, display.y, display.width, display.height }
	);
	if (!fitted) {
		info.Dock();
		return;
	}

	info.FloatingPosition(wxPoint(fitted->x, fitted->y));
	info.FloatingSize(wxSize(fitted->width, fitted->height));
	info.FloatingClientSize(wxDefaultSize);

	if (info.frame) {
		info.frame->SetSize(wxRect(wxPoint(fitted->x, fitted->y), wxSize(fitted->width, fitted->height)));
	}
}

void LayoutManager::UpdatePaletteActivePage(PaletteWindow* palette, const std::string& page_name) {
	if (g_layout.IsLoadingPerspective() || !palette || !g_gui.aui_manager || page_name.empty()) {
		return;
	}
	const wxAuiPaneInfo& info = g_gui.aui_manager->GetPane(palette);
	if (!info.IsOk() || info.name.empty()) {
		return;
	}

	const std::string pane_name = nstr(info.name);
	std::string current_data = g_settings.getString(Config::PALETTE_ACTIVE_PAGE);
	std::unordered_map<std::string, std::string> pane_pages;

	if (!current_data.empty()) {
		if (current_data.find('=') != std::string::npos) {
			std::stringstream ss(current_data);
			std::string item;
			while (std::getline(ss, item, '|')) {
				auto eq = item.find('=');
				if (eq != std::string::npos) {
					pane_pages[item.substr(0, eq)] = item.substr(eq + 1);
				}
			}
		} else {
			pane_pages[pane_name] = current_data;
		}
	}

	pane_pages[pane_name] = page_name;

	std::string serialized;
	for (const auto& [pname, ppage] : pane_pages) {
		serialized += pname + "=" + ppage + "|";
	}
	g_settings.setString(Config::PALETTE_ACTIVE_PAGE, serialized);
}

LayoutManager::LayoutManager() {
}

LayoutManager::~LayoutManager() {
}

void LayoutManager::LoadPerspective() {
	struct PerspectiveGuard {
		bool& flag;
		explicit PerspectiveGuard(bool& f) : flag(f) { flag = true; }
		~PerspectiveGuard() { flag = false; }
	} guard(m_isLoadingPerspective);

	if (!g_version.IsVersionLoaded()) {
		if (g_settings.getInteger(Config::WINDOW_MAXIMIZED)) {
			g_gui.root->Maximize();
		} else {
			g_gui.root->SetSize(wxSize(
				g_settings.getInteger(Config::WINDOW_WIDTH),
				g_settings.getInteger(Config::WINDOW_HEIGHT)
			));
		}
	} else {
		std::string tmp;
		std::string layout = g_settings.getString(Config::PALETTE_LAYOUT);

		std::vector<std::string> palette_list;
		for (char c : layout) {
			if (c == '|') {
				palette_list.push_back(tmp);
				tmp.clear();
			} else {
				tmp.push_back(c);
			}
		}

		if (!tmp.empty()) {
			palette_list.push_back(tmp);
		}

		if (palette_list.empty() && g_palettes.palettes.empty()) {
			g_gui.CreatePalette();
		}

		// Restoring the perspective runs twice on a normal start (application
		// init and, later, when the client version is loaded), so the palettes
		// already registered for these saved panes are reused instead of
		// creating a second palette with the same pane name.
		for (size_t index = 0; index < palette_list.size(); ++index) {
			const std::string& name = palette_list[index];

			PaletteWindow* palette = nullptr;
			if (index < g_palettes.palettes.size()) {
				palette = *std::next(g_palettes.palettes.begin(), static_cast<std::ptrdiff_t>(index));
			} else {
				palette = g_gui.CreatePalette();
			}
			if (!palette) {
				continue;
			}

			wxAuiPaneInfo& info = g_gui.aui_manager->GetPane(palette);
			if (!info.IsOk()) {
				continue;
			}
			g_gui.aui_manager->LoadPaneInfo(wxstr(name), info);

			if (info.IsFloatable()) {
				bool offscreen = true;
				for (uint32_t index = 0; index < wxDisplay::GetCount(); ++index) {
					wxDisplay display(index);
					wxRect rect = display.GetClientArea();
					if (rect.Contains(info.floating_pos)) {
						offscreen = false;
						break;
					}
				}

				if (offscreen) {
					info.Dock();
				}
			}
		}

		if (g_settings.getInteger(Config::MINIMAP_VISIBLE)) {
			if (!g_minimap.GetWindow()) {
				wxAuiPaneInfo info = MinimapManager::DefaultPaneInfo();

				const wxString& data = wxstr(g_settings.getString(Config::MINIMAP_LAYOUT));
				if (!data.empty()) {
					g_gui.aui_manager->LoadPaneInfo(data, info);
				}
				MinimapManager::NormalizePaneInfo(info);

				g_minimap.SetWindow(new MinimapWindow(g_gui.root));
				g_gui.aui_manager->AddPane(g_minimap.GetWindow(), info);
			} else {
				wxAuiPaneInfo& info = g_gui.aui_manager->GetPane(g_minimap.GetWindow());

				const wxString& data = wxstr(g_settings.getString(Config::MINIMAP_LAYOUT));
				if (!data.empty()) {
					g_gui.aui_manager->LoadPaneInfo(data, info);
				}
				MinimapManager::NormalizePaneInfo(info);
			}

			wxAuiPaneInfo& info = g_gui.aui_manager->GetPane(g_minimap.GetWindow());
			EnsurePaneOnScreen(info);
		}
		g_status.UpdateTitle();
		g_gui.root->UpdateMenubar();
	}

	if (g_settings.getInteger(Config::INGAME_PREVIEW_VISIBLE)) {
		g_preview.Create(false);
		if (g_preview.GetWindow()) {
			wxAuiPaneInfo& info = g_gui.aui_manager->GetPane(g_preview.GetWindow());
			EnsurePaneOnScreen(info);
		}
	}

	// Initialize ToolOptionsWindow
	if (!g_gui.tool_options) {
		g_gui.tool_options = new ToolOptionsWindow(g_gui.root);

		wxAuiPaneInfo info;
		wxString layout = wxstr(g_settings.getString(Config::TOOL_OPTIONS_LAYOUT));
		if (!layout.empty()) {
			g_gui.aui_manager->LoadPaneInfo(layout, info);
		} else {
			info.Name("ToolOptions").Caption("Brush Options").Left().Layer(0).Position(0).CloseButton(true).MaximizeButton(true).BestSize(220, 330);
		}
		if (info.name.empty()) {
			info.Name("ToolOptions");
		}
		info.MinSize(g_gui.root ? g_gui.root->FromDIP(wxSize(220, 180)) : wxSize(220, 180));

		g_gui.aui_manager->AddPane(g_gui.tool_options, info);
	} else {
		wxAuiPaneInfo& info = g_gui.aui_manager->GetPane(g_gui.tool_options);
		wxString layout = wxstr(g_settings.getString(Config::TOOL_OPTIONS_LAYOUT));
		if (!layout.empty()) {
			g_gui.aui_manager->LoadPaneInfo(layout, info);
		}
		info.MinSize(g_gui.root ? g_gui.root->FromDIP(wxSize(220, 180)) : wxSize(220, 180));
	}

	if (g_gui.tool_options) {
		g_gui.tool_options->ReloadSettings();
		if (PaletteWindow* palette = g_gui.GetPalette()) {
			g_gui.ActivatePalette(palette);
		}
	}

	const std::string active_page_data = g_settings.getString(Config::PALETTE_ACTIVE_PAGE);
	if (!active_page_data.empty()) {
		if (active_page_data.find('=') != std::string::npos) {
			std::unordered_map<std::string, std::string> pane_pages;
			std::stringstream ss(active_page_data);
			std::string item;
			while (std::getline(ss, item, '|')) {
				auto eq = item.find('=');
				if (eq != std::string::npos) {
					pane_pages[item.substr(0, eq)] = item.substr(eq + 1);
				}
			}
			const std::vector<PaletteWindow*> palettes_copy(g_palettes.palettes.begin(), g_palettes.palettes.end());
			for (auto* palette : palettes_copy) {
				if (!palette || !g_gui.aui_manager) {
					continue;
				}
				const wxAuiPaneInfo& pinfo = g_gui.aui_manager->GetPane(palette);
				if (!pinfo.IsOk()) {
					continue;
				}
				auto it = pane_pages.find(nstr(pinfo.name));
				if (it != pane_pages.end()) {
					palette->SelectPage(it->second);
				}
			}
		} else {
			if (PaletteWindow* palette = g_gui.GetPalette()) {
				palette->SelectPage(active_page_data);
			}
		}
	}

	if (g_gui.tile_properties_panel) {
		wxAuiPaneInfo& info = g_gui.aui_manager->GetPane(g_gui.tile_properties_panel);
		if (info.IsOk()) {
			const wxString layout = wxstr(g_settings.getString(Config::TILE_PROPERTIES_LAYOUT));
			if (!layout.empty()) {
				g_gui.aui_manager->LoadPaneInfo(layout, info);
			}
			// Saved layouts store the minimum size from the version that wrote them.
			info.MinSize(g_gui.tile_properties_panel->GetMinSize());
			EnsurePaneOnScreen(info);
		}
	}

	if (g_gui.brush_items_window) {
		wxAuiPaneInfo& info = g_gui.aui_manager->GetPane(g_gui.brush_items_window);
		if (info.IsOk()) {
			const wxString layout = wxstr(g_settings.getString(Config::BRUSH_ITEMS_LAYOUT));
			if (!layout.empty()) {
				g_gui.aui_manager->LoadPaneInfo(layout, info);
				EnsurePaneOnScreen(info);
			}
			if (info.IsShown()) {
				g_gui.brush_items_window->SetActiveBrush(g_gui.GetCurrentBrush());
			}
		}
	}

	if (LuaScriptsWindow* scripts = LuaScriptsWindow::Get()) {
		wxAuiPaneInfo& info = g_gui.aui_manager->GetPane(scripts);
		if (info.IsOk()) {
			const wxString layout = wxstr(g_settings.getString(Config::SCRIPTS_MANAGER_LAYOUT));
			if (!layout.empty()) {
				g_gui.aui_manager->LoadPaneInfo(layout, info);
				EnsurePaneOnScreen(info);
			}
		}
	}


	g_gui.aui_manager->Update();
	for (wxAuiPaneInfo& pane : g_gui.aui_manager->GetAllPanes()) {
		EnsurePaneOnScreen(pane);
	}
	if (g_gui.root && g_gui.root->GetAuiToolBar()) {
		g_gui.root->GetAuiToolBar()->LoadPerspective();
	}
}

void LayoutManager::SavePerspective() {
	g_settings.setInteger(Config::WINDOW_MAXIMIZED, g_gui.root->IsMaximized());
	g_settings.setInteger(Config::WINDOW_WIDTH, g_gui.root->GetSize().GetWidth());
	g_settings.setInteger(Config::WINDOW_HEIGHT, g_gui.root->GetSize().GetHeight());

	g_settings.setInteger(Config::MINIMAP_VISIBLE, g_minimap.GetWindow() ? 1 : 0);

	wxString pinfo;
	wxString palette_pages_info;
	const std::vector<PaletteWindow*> palettes_copy(g_palettes.palettes.begin(), g_palettes.palettes.end());
	for (auto* palette : palettes_copy) {
		if (palette && g_gui.aui_manager && g_gui.aui_manager->GetPane(palette).IsShown()) {
			pinfo << SavePaneInfoWithCurrentSize(*g_gui.aui_manager, palette) << "|";
			const wxAuiPaneInfo& p_info = g_gui.aui_manager->GetPane(palette);
			const std::string page_name = palette->GetSelectedPageName();
			if (!page_name.empty() && !p_info.name.empty()) {
				palette_pages_info << p_info.name << "=" << wxstr(page_name) << "|";
			}
		}
	}
	g_settings.setString(Config::PALETTE_LAYOUT, nstr(pinfo));
	if (!palette_pages_info.empty()) {
		g_settings.setString(Config::PALETTE_ACTIVE_PAGE, nstr(palette_pages_info));
	}

	if (g_minimap.GetWindow()) {
		wxString s = SavePaneInfoWithCurrentSize(*g_gui.aui_manager, g_minimap.GetWindow());
		g_settings.setString(Config::MINIMAP_LAYOUT, nstr(s));
	}

	if (g_gui.tool_options) {
		wxString s = SavePaneInfoWithCurrentSize(*g_gui.aui_manager, g_gui.tool_options);
		g_settings.setString(Config::TOOL_OPTIONS_LAYOUT, nstr(s));
	}

	if (g_gui.tile_properties_panel) {
		wxString s = SavePaneInfoWithCurrentSize(*g_gui.aui_manager, g_gui.tile_properties_panel);
		g_settings.setString(Config::TILE_PROPERTIES_LAYOUT, nstr(s));
	}

	if (g_gui.brush_items_window) {
		wxString s = SavePaneInfoWithCurrentSize(*g_gui.aui_manager, g_gui.brush_items_window);
		g_settings.setString(Config::BRUSH_ITEMS_LAYOUT, nstr(s));
	}

	if (LuaScriptsWindow* scripts = LuaScriptsWindow::Get()) {
		wxString s = SavePaneInfoWithCurrentSize(*g_gui.aui_manager, scripts);
		g_settings.setString(Config::SCRIPTS_MANAGER_LAYOUT, nstr(s));
	}

	g_settings.setInteger(Config::INGAME_PREVIEW_VISIBLE, g_preview.IsVisible() ? 1 : 0);
	if (g_preview.GetWindow()) {
		wxString s = SavePaneInfoWithCurrentSize(*g_gui.aui_manager, g_preview.GetWindow());
		g_settings.setString(Config::INGAME_PREVIEW_LAYOUT, nstr(s));
	}

	if (g_gui.root && g_gui.root->GetAuiToolBar()) {
		g_gui.root->GetAuiToolBar()->SavePerspective();
	}
}
