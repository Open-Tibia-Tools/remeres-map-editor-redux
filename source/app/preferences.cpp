//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#include "app/preferences.h"

#include "app/client_version.h"
#include "app/main.h"
#include "app/settings.h"
#include "ui/gui.h"
#include "util/image_manager.h"

PreferencesWindow::PreferencesWindow(wxWindow* parent, bool clientVersionSelected) :
	wxDialog(parent, wxID_ANY, "Preferences", wxDefaultPosition, wxSize(980, 720), wxCAPTION | wxCLOSE_BOX | wxRESIZE_BORDER) {
	SetBackgroundColour(Theme::Get(Theme::Role::Surface));
	SetMinSize(wxSize(FromDIP(860), FromDIP(640)));

	auto* sizer = new wxBoxSizer(wxVERTICAL);

	book = new wxListbook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBK_LEFT);
	book->SetBackgroundColour(Theme::Get(Theme::Role::PanelBackground));

	auto* image_list = new wxImageList(18, 18);
	image_list->Add(IMAGE_MANAGER.GetBitmap(ICON_GEAR, wxSize(18, 18)));
	image_list->Add(IMAGE_MANAGER.GetBitmap(ICON_PEN_TO_SQUARE, wxSize(18, 18)));
	image_list->Add(IMAGE_MANAGER.GetBitmap(ICON_IMAGE, wxSize(18, 18)));
	image_list->Add(IMAGE_MANAGER.GetBitmap(ICON_WINDOW_MAXIMIZE, wxSize(18, 18)));
	image_list->Add(IMAGE_MANAGER.GetBitmap(ICON_GAMEPAD, wxSize(18, 18)));
	book->AssignImageList(image_list);

	general_page = new GeneralPage(book);
	editor_page = new EditorPage(book);
	graphics_page = new GraphicsPage(book);
	interface_page = new InterfacePage(book);
	client_version_page = new ClientVersionPage(book);

	book->AddPage(general_page, "General", false, 0);
	book->AddPage(editor_page, "Editor", false, 1);
	book->AddPage(graphics_page, "Graphics", false, 2);
	book->AddPage(interface_page, "Interface", false, 3);
	book->AddPage(client_version_page, "Client Version", false, 4);
	book->SetSelection(clientVersionSelected ? 4 : 0);

	if (auto* list_view = book->GetListView()) {
		list_view->SetMinSize(wxSize(FromDIP(170), -1));
		list_view->SetBackgroundColour(Theme::Get(Theme::Role::RaisedSurface));
		list_view->SetTextColour(Theme::Get(Theme::Role::Text));
		list_view->SetColumnWidth(0, FromDIP(150));
	}

	sizer->Add(book, 1, wxEXPAND | wxALL, FromDIP(10));

	auto* footer_sizer = new wxBoxSizer(wxHORIZONTAL);
	footer_sizer->AddStretchSpacer();

	auto* ok_button = new wxButton(this, wxID_OK, "OK");
	ok_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_CHECK, wxSize(16, 16)));
	footer_sizer->Add(ok_button, 0, wxRIGHT, FromDIP(8));

	auto* cancel_button = new wxButton(this, wxID_CANCEL, "Cancel");
	cancel_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_XMARK, wxSize(16, 16)));
	footer_sizer->Add(cancel_button, 0, wxRIGHT, FromDIP(8));

	auto* apply_button = new wxButton(this, wxID_APPLY, "Apply");
	apply_button->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_SYNC, wxSize(16, 16)));
	footer_sizer->Add(apply_button, 0);

	sizer->Add(footer_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(10));
	SetSizer(sizer);

	const int width = g_settings.getInteger(Config::PREFERENCES_WINDOW_WIDTH);
	const int height = g_settings.getInteger(Config::PREFERENCES_WINDOW_HEIGHT);
	if (width > 0 && height > 0) {
		SetSize(width, height);
	}

	Centre(wxBOTH);

	Bind(wxEVT_BUTTON, &PreferencesWindow::OnClickOK, this, wxID_OK);
	Bind(wxEVT_BUTTON, &PreferencesWindow::OnClickCancel, this, wxID_CANCEL);
	Bind(wxEVT_BUTTON, &PreferencesWindow::OnClickApply, this, wxID_APPLY);
	Bind(wxEVT_CLOSE_WINDOW, &PreferencesWindow::OnClose, this);

	wxIcon icon;
	icon.CopyFromBitmap(IMAGE_MANAGER.GetBitmap(ICON_GEAR, wxSize(32, 32)));
	SetIcon(icon);
}

PreferencesWindow::~PreferencesWindow() {
	int width = 0;
	int height = 0;
	GetSize(&width, &height);
	g_settings.setInteger(Config::PREFERENCES_WINDOW_WIDTH, width);
	g_settings.setInteger(Config::PREFERENCES_WINDOW_HEIGHT, height);
}

void PreferencesWindow::OnClickOK(wxCommandEvent& WXUNUSED(event)) {
	if (!client_version_page->ValidateData()) {
		return;
	}
	Apply();
	EndModal(wxID_OK);
}

void PreferencesWindow::OnClickCancel(wxCommandEvent& WXUNUSED(event)) {
	client_version_page->DiscardPendingChanges();
	EndModal(wxID_CANCEL);
}

void PreferencesWindow::OnClickApply(wxCommandEvent& WXUNUSED(event)) {
	if (!client_version_page->ValidateData()) {
		return;
	}
	Apply();
}

void PreferencesWindow::Apply() {
	general_page->Apply();
	editor_page->Apply();
	graphics_page->Apply();
	interface_page->Apply();
	client_version_page->Apply();

	g_settings.save();
}

void PreferencesWindow::OnClose(wxCloseEvent& event) {
	if (!IsModal()) {
		event.Skip();
		return;
	}

	client_version_page->DiscardPendingChanges();
	EndModal(wxID_CANCEL);
}
