#include "app/preferences/client_version_page.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <map>
#include <ranges>
#include <thread>

#include <wx/menu.h>
#include <wx/tokenzr.h>
#include <wx/weakref.h>

#include "app/main.h"
#include "app/settings.h"
#include "ui/theme.h"
#include "util/image_manager.h"

namespace {
constexpr char kDefaultDataDirectory[] = "1287";
constexpr std::array<std::string_view, 8> kAutoDetectedProperties = {
	"metadataFile",
	"spritesFile",
	"datSignature",
	"sprSignature",
	"transparency",
	"extended",
	"frameDurations",
	"frameGroups",
};

bool IsAutoDetectedProperty(std::string_view property_name) {
	return std::ranges::find(kAutoDetectedProperties, property_name) != kAutoDetectedProperties.end();
}

unsigned char BlendChannel(unsigned char background, unsigned char overlay, double ratio) {
	return static_cast<unsigned char>(std::lround((static_cast<double>(background) * (1.0 - ratio)) + (static_cast<double>(overlay) * ratio)));
}

wxColour BlendColour(const wxColour& background, const wxColour& overlay, double ratio) {
	return wxColour(
		BlendChannel(background.Red(), overlay.Red(), ratio),
		BlendChannel(background.Green(), overlay.Green(), ratio),
		BlendChannel(background.Blue(), overlay.Blue(), ratio));
}

std::string ConfigTypeFromSelection(long selection) {
	switch (selection) {
		case 0:
			return "dat_otb";
		case 1:
			return "dat_only";
		case 2:
			return "dat_srv";
		case 3:
			return "protobuf_otb";
		case 4:
			return "protobuf_only";
		default:
			return "dat_otb";
	}
}

int SelectionFromConfigType(const std::string& value) {
	if (value == "dat_only") {
		return 1;
	}
	if (value == "dat_srv") {
		return 2;
	}
	if (value == "protobuf" || value == "protobuf_otb") {
		return 3;
	}
	if (value == "protobuf_only") {
        return 4;
	}
	return 0;
}

std::string LowercaseCopy(std::string value) {
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
		return static_cast<char>(std::tolower(character));
	});
	return value;
}

bool MatchesClientFilter(const ClientVersion& version, const std::string& filter) {
	if (filter.empty()) {
		return true;
	}

	std::string haystack = LowercaseCopy(version.getName());
	haystack += " ";
	haystack += LowercaseCopy(version.getDescription());
	haystack += " ";
	haystack += std::to_string(version.getVersion());
	haystack += " ";
	haystack += std::to_string(version.getOtbId());
	return haystack.find(filter) != std::string::npos;
}

template <typename T>
void UpdateGridProperty(wxPropertyGrid* grid, const wxString& property_name, const T& value) {
	if (!grid) {
		return;
	}
	if (auto* property = grid->GetPropertyByName(property_name)) {
		property->SetValue(value);
	}
}
}

ClientVersionPage::ClientVersionPage(wxWindow* parent) : PreferencesPage(parent) {
	auto* main_sizer = new wxBoxSizer(wxVERTICAL);

	client_splitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_LIVE_UPDATE);
	client_splitter->SetBackgroundColour(GetBackgroundColour());

	auto* left_panel = new wxPanel(client_splitter, wxID_ANY);
	left_panel->SetBackgroundColour(GetBackgroundColour());
	auto* left_sizer = new wxBoxSizer(wxVERTICAL);

	auto* defaults_section = new PreferencesSectionPanel(
		left_panel,
		"Global Client Settings",
		"Defaults that apply to client loading across the editor."
	);
	default_version_choice = new wxChoice(defaults_section, wxID_ANY);
	PreferencesLayout::AddControlRow(
		defaults_section,
		"Default client version",
		"Used as the preferred version in flows that need a fallback client.",
		default_version_choice,
		true
	);
	check_sigs_chkbox = new wxCheckBox(defaults_section, wxID_ANY, "Enabled");
	check_sigs_chkbox->SetValue(g_settings.getBoolean(Config::CHECK_SIGNATURES));
	PreferencesLayout::AddControlRow(
		defaults_section,
		"Signature validation",
		"Verify DAT and SPR signatures so mismatched client folders are caught early.",
		check_sigs_chkbox
	);
	left_sizer->Add(defaults_section, 0, wxEXPAND | wxALL, FromDIP(10));

	auto* library_section = new PreferencesSectionPanel(
		left_panel,
		"Client Versions",
		"Search, scan, and pick the client you want to edit."
	);
	client_search_ctrl = new wxSearchCtrl(library_section, wxID_ANY);
	client_search_ctrl->ShowSearchButton(true);
	client_search_ctrl->ShowCancelButton(true);
	client_search_ctrl->SetDescriptiveText("Search by name, version, or OTB id");
	library_section->GetBodySizer()->Add(client_search_ctrl, 0, wxEXPAND | wxBOTTOM, FromDIP(10));
	last_search_text = client_search_ctrl->GetValue();

	client_tree_ctrl = new wxTreeCtrl(library_section, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTR_HIDE_ROOT | wxTR_HAS_BUTTONS | wxTR_SINGLE | wxTR_LINES_AT_ROOT);
	client_tree_ctrl->SetBackgroundColour(Theme::Get(Theme::Role::Background));
	client_tree_ctrl->SetForegroundColour(Theme::Get(Theme::Role::Text));
	library_section->GetBodySizer()->Add(client_tree_ctrl, 1, wxEXPAND | wxBOTTOM, FromDIP(10));

	auto* action_sizer = new wxBoxSizer(wxHORIZONTAL);
	add_client_btn = new wxButton(library_section, wxID_ANY, "Add");
	add_client_btn->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_PLUS));
	action_sizer->Add(add_client_btn, 0, wxRIGHT, FromDIP(8));
	duplicate_client_btn = new wxButton(library_section, wxID_ANY, "Duplicate");
	duplicate_client_btn->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_COPY));
	action_sizer->Add(duplicate_client_btn, 0, wxRIGHT, FromDIP(8));
	delete_client_btn = new wxButton(library_section, wxID_ANY, "Remove");
	delete_client_btn->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_MINUS));
	action_sizer->Add(delete_client_btn, 0);
	action_sizer->AddStretchSpacer();
	library_section->GetBodySizer()->Add(action_sizer, 0, wxEXPAND);
	left_sizer->Add(library_section, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(10));
	left_panel->SetSizer(left_sizer);

	auto* right_panel = new wxPanel(client_splitter, wxID_ANY);
	right_panel->SetBackgroundColour(GetBackgroundColour());
	auto* right_sizer = new wxBoxSizer(wxVERTICAL);

	detail_book = new wxSimplebook(right_panel, wxID_ANY);
	detail_book->SetBackgroundColour(GetBackgroundColour());

	auto* empty_panel = new wxPanel(detail_book, wxID_ANY);
	empty_panel->SetBackgroundColour(GetBackgroundColour());
	auto* empty_sizer = new wxBoxSizer(wxVERTICAL);
	auto* empty_section = new PreferencesSectionPanel(
		empty_panel,
		"Choose a Client Version",
		"Select a version from the list on the left to edit identity, files, compatibility, and feature flags."
	);
	empty_section->GetBodySizer()->Add(
		PreferencesLayout::CreateBodyText(empty_section, "The selected client will appear here as one focused editing workspace.", false),
		0,
		wxEXPAND
	);
	empty_sizer->AddStretchSpacer();
	empty_sizer->Add(empty_section, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(10));
	empty_sizer->AddStretchSpacer();
	empty_panel->SetSizer(empty_sizer);
	detail_book->AddPage(empty_panel, "Empty");

	auto* editor_panel = new wxPanel(detail_book, wxID_ANY);
	editor_panel->SetBackgroundColour(GetBackgroundColour());
	auto* editor_sizer = new wxBoxSizer(wxVERTICAL);

	auto* details_section = new PreferencesSectionPanel(
		editor_panel,
		"Client Details",
		"Edit one client version at a time."
	);
	auto* summary_row = new wxBoxSizer(wxHORIZONTAL);
	summary_name_label = PreferencesLayout::CreateBodyText(details_section, "", true);
	summary_name_label->SetFont(Theme::GetFont(14, true));
	summary_row->Add(summary_name_label, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(10));
	summary_dirty_label = PreferencesLayout::CreateBodyText(details_section, "", true);
	summary_row->Add(summary_dirty_label, 0, wxALIGN_CENTER_VERTICAL);
	details_section->GetBodySizer()->Add(summary_row, 0, wxEXPAND | wxBOTTOM, FromDIP(12));

	auto* top_separator = new wxPanel(details_section, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(1)));
	top_separator->SetBackgroundColour(Theme::Get(Theme::Role::Border));
	details_section->GetBodySizer()->Add(top_separator, 0, wxEXPAND | wxBOTTOM, FromDIP(12));

	client_prop_grid = new wxPropertyGrid(
		details_section,
		wxID_ANY,
		wxDefaultPosition,
		wxDefaultSize,
		wxPG_SPLITTER_AUTO_CENTER | wxPG_BOLD_MODIFIED
	);
	client_prop_grid->SetBackgroundColour(Theme::Get(Theme::Role::Background));
	client_prop_grid->SetForegroundColour(Theme::Get(Theme::Role::Text));
	client_prop_grid->SetCaptionBackgroundColour(Theme::Get(Theme::Role::RaisedSurface));
	client_prop_grid->SetCaptionTextColour(Theme::Get(Theme::Role::Text));
	client_prop_grid->SetCellBackgroundColour(Theme::Get(Theme::Role::Background));
	client_prop_grid->SetCellTextColour(Theme::Get(Theme::Role::Text));
	client_prop_grid->SetLineColour(Theme::Get(Theme::Role::Border));
	client_prop_grid->SetMarginColour(Theme::Get(Theme::Role::RaisedSurface));
	client_prop_grid->SetSelectionBackgroundColour(Theme::Get(Theme::Role::Accent));
	client_prop_grid->SetSelectionTextColour(*wxWHITE);
	client_prop_grid->SetMinSize(wxSize(-1, FromDIP(420)));
	details_section->GetBodySizer()->Add(client_prop_grid, 1, wxEXPAND);

	editor_sizer->Add(details_section, 1, wxEXPAND | wxALL, FromDIP(10));
	editor_panel->SetSizer(editor_sizer);
	detail_book->AddPage(editor_panel, "Editor");

	right_sizer->Add(detail_book, 1, wxEXPAND);
	right_panel->SetSizer(right_sizer);

	client_splitter->SplitVertically(left_panel, right_panel, FromDIP(350));
	client_splitter->SetSashGravity(0.28);
	client_splitter->SetMinimumPaneSize(FromDIP(240));
	main_sizer->Add(client_splitter, 1, wxEXPAND | wxALL, FromDIP(5));

	SetSizer(main_sizer);

	PopulateDefaultVersionChoice();
	PopulateClientTree();
	active_client = GetSelectedClient();
	if (active_client) {
		active_client->backup();
	}
	RefreshClientEditor();

	client_tree_ctrl->Bind(wxEVT_TREE_SEL_CHANGED, &ClientVersionPage::OnClientSelected, this);
	client_tree_ctrl->Bind(wxEVT_TREE_ITEM_MENU, &ClientVersionPage::OnTreeContextMenu, this);
	client_search_ctrl->Bind(wxEVT_TEXT, &ClientVersionPage::OnSearchChanged, this);
	client_search_ctrl->Bind(wxEVT_SEARCHCTRL_CANCEL_BTN, &ClientVersionPage::OnSearchCancelled, this);
	client_prop_grid->Bind(wxEVT_PG_CHANGED, &ClientVersionPage::OnPropertyChanged, this);
	add_client_btn->Bind(wxEVT_BUTTON, &ClientVersionPage::OnAddClient, this);
	duplicate_client_btn->Bind(wxEVT_BUTTON, &ClientVersionPage::OnDuplicateClient, this);
	delete_client_btn->Bind(wxEVT_BUTTON, &ClientVersionPage::OnDeleteClient, this);
}

void ClientVersionPage::PopulateDefaultVersionChoice() {
	const wxString previous_selection = default_version_choice->GetStringSelection();
	default_version_choice->Clear();
	ClientVersion* latest_version = ClientVersion::getLatestVersion();
	int latest_index = wxNOT_FOUND;
	int index = 0;
	for (auto* version : ClientVersion::getAll()) {
		if (IsPendingDeletion(*version)) {
			continue;
		}
		default_version_choice->Append(wxstr(version->getName()));
		if (version == latest_version) {
			latest_index = index;
		}
		++index;
	}

	const int previous_index = previous_selection.empty() ? wxNOT_FOUND : default_version_choice->FindString(previous_selection);
	if (previous_index != wxNOT_FOUND) {
		default_version_choice->SetSelection(previous_index);
	} else if (latest_index != wxNOT_FOUND) {
		default_version_choice->SetSelection(latest_index);
	} else if (default_version_choice->GetCount() > 0) {
		default_version_choice->SetSelection(0);
	}
}

bool ClientVersionPage::MatchesFilter(const ClientVersion& version) const {
	return !IsPendingDeletion(version) && MatchesClientFilter(version, client_filter);
}

int ClientVersionPage::GetMajorGroup(const ClientVersion& version) const {
	const int protocol = static_cast<int>(version.getVersion());
	if (protocol >= 10000) {
		return protocol / 1000;
	}
	if (protocol >= 100) {
		return protocol / 100;
	}
	return std::max(protocol / 10, 0);
}

void ClientVersionPage::PopulateClientTree() {
	const bool previous_ignore_tree_selection = ignore_tree_selection;
	ignore_tree_selection = true;

	ClientVersion* preferred_selection = active_client;
	client_tree_ctrl->DeleteAllItems();
	auto root = client_tree_ctrl->AddRoot("Clients");

	std::map<int, std::vector<ClientVersion*>> grouped_versions;
	for (auto* version : ClientVersion::getAll()) {
		if (MatchesFilter(*version)) {
			grouped_versions[GetMajorGroup(*version)].push_back(version);
		}
	}

	wxTreeItemId item_to_select;
	wxTreeItemId first_item;
	wxTreeItemId group_to_expand;

	for (auto& [major_version, versions] : grouped_versions) {
		std::sort(versions.begin(), versions.end(), [](const ClientVersion* lhs, const ClientVersion* rhs) {
			if (lhs->getVersion() != rhs->getVersion()) {
				return lhs->getVersion() < rhs->getVersion();
			}
			return lhs->getName() < rhs->getName();
		});

		auto group = client_tree_ctrl->AppendItem(root, wxString::Format("%d.x (%zu)", major_version, versions.size()));
		for (auto* version : versions) {
			auto item = client_tree_ctrl->AppendItem(group, wxstr(version->getName()), -1, -1, new TreeItemData(version));
			if (!first_item.IsOk()) {
				first_item = item;
			}
			if (version == preferred_selection) {
				item_to_select = item;
				group_to_expand = group;
			}
		}
		if (!client_filter.empty()) {
			client_tree_ctrl->Expand(group);
		} else {
			client_tree_ctrl->Collapse(group);
		}
	}

	if (!group_to_expand.IsOk()) {
		wxTreeItemIdValue cookie;
		group_to_expand = client_tree_ctrl->GetFirstChild(root, cookie);
	}
	if (group_to_expand.IsOk()) {
		client_tree_ctrl->Expand(group_to_expand);
	}

	if (item_to_select.IsOk()) {
		client_tree_ctrl->SelectItem(item_to_select);
		client_tree_ctrl->EnsureVisible(item_to_select);
	} else if (!preferred_selection && client_filter.empty() && first_item.IsOk()) {
		client_tree_ctrl->SelectItem(first_item);
		client_tree_ctrl->EnsureVisible(first_item);
	} else {
		client_tree_ctrl->UnselectAll();
	}
	ignore_tree_selection = previous_ignore_tree_selection;
}

void ClientVersionPage::SelectClient(ClientVersion* version) {
	if (!version) {
		ignore_tree_selection = true;
		client_tree_ctrl->UnselectAll();
		ignore_tree_selection = false;
		return;
	}

	auto root = client_tree_ctrl->GetRootItem();
	if (!root.IsOk()) {
		return;
	}

	wxTreeItemIdValue group_cookie;
	for (auto group = client_tree_ctrl->GetFirstChild(root, group_cookie); group.IsOk(); group = client_tree_ctrl->GetNextChild(root, group_cookie)) {
		wxTreeItemIdValue child_cookie;
		for (auto item = client_tree_ctrl->GetFirstChild(group, child_cookie); item.IsOk(); item = client_tree_ctrl->GetNextChild(group, child_cookie)) {
			auto* data = dynamic_cast<TreeItemData*>(client_tree_ctrl->GetItemData(item));
			if (data && data->cv == version) {
				ignore_tree_selection = true;
				client_tree_ctrl->Expand(group);
				client_tree_ctrl->SelectItem(item);
				client_tree_ctrl->EnsureVisible(item);
				ignore_tree_selection = false;
				return;
			}
		}
	}
}

ClientVersion* ClientVersionPage::GetSelectedClient() {
	auto selection = client_tree_ctrl->GetSelection();
	if (!selection.IsOk()) {
		return nullptr;
	}
	auto* data = dynamic_cast<TreeItemData*>(client_tree_ctrl->GetItemData(selection));
	return data ? data->cv : nullptr;
}

void ClientVersionPage::RefreshSummary() {
	if (!active_client) {
		summary_name_label->SetLabel("");
		summary_dirty_label->SetLabel("");
		return;
	}

	summary_name_label->SetLabel(wxstr(active_client->getName()));
	if (active_client->isDirty()) {
		summary_dirty_label->SetLabel("Unsaved changes");
		summary_dirty_label->SetForegroundColour(Theme::Get(Theme::Role::Warning));
	} else {
		summary_dirty_label->SetLabel("Saved");
		summary_dirty_label->SetForegroundColour(Theme::Get(Theme::Role::Success));
	}
	summary_name_label->GetParent()->Layout();
}

void ClientVersionPage::RefreshClientEditor() {
	client_prop_grid->Clear();

	if (!active_client) {
		detail_book->SetSelection(0);
		RefreshSummary();
		return;
	}

	detail_book->SetSelection(1);
	RefreshSummary();

	client_prop_grid->Append(new wxPropertyCategory("Identity"));
	auto* version_property = client_prop_grid->Append(new wxIntProperty("Version ID", "Version", active_client->getVersion()));
	version_property->SetHelpString("Internal numeric client version, for example 860 or 1287.");
	auto* name_property = client_prop_grid->Append(new wxStringProperty("Display Name", "Name", wxstr(active_client->getName())));
	name_property->SetHelpString("Name shown in version selectors and compatibility prompts.");
	auto* description_property = client_prop_grid->Append(new wxStringProperty("Description", "description", wxstr(active_client->getDescription())));
	description_property->SetHelpString("Optional note describing the client, data source, or intended use.");
	wxPGChoices config_choices;
	config_choices.Add("dat_otb");
	config_choices.Add("dat_only");
	config_choices.Add("dat_srv");
	config_choices.Add("protobuf_otb");
	config_choices.Add("protobuf_only");
	auto* config_property = client_prop_grid->Append(new wxEnumProperty("Configuration Type", "configType", config_choices, SelectionFromConfigType(active_client->getConfigType())));
	config_property->SetHelpString("How item definitions are loaded for this client.");

	client_prop_grid->Append(new wxPropertyCategory("Files & Paths"));
	auto* client_path_property = client_prop_grid->Append(new wxDirProperty("Client Path", "clientPath", active_client->getClientPath().GetFullPath()));
	client_path_property->SetHelpString("Folder containing the configured client DAT and SPR files.");
	auto* data_directory_property = client_prop_grid->Append(new wxStringProperty("Data Directory", "dataDirectory", wxstr(active_client->getDataDirectory())));
	data_directory_property->SetHelpString("Editor data folder used for this client version.");
	auto* metadata_property = client_prop_grid->Append(new wxStringProperty("Metadata File (.dat)", "metadataFile", wxstr(active_client->getMetadataFile())));
	metadata_property->SetHelpString("File name of the DAT metadata file inside the client path.");
	auto* sprites_property = client_prop_grid->Append(new wxStringProperty("Sprites File (.spr)", "spritesFile", wxstr(active_client->getSpritesFile())));
	sprites_property->SetHelpString("File name of the SPR sprite archive inside the client path.");

	client_prop_grid->Append(new wxPropertyCategory("Compatibility"));
	auto* otb_id_property = client_prop_grid->Append(new wxIntProperty("OTB ID", "otbId", active_client->getOtbId()));
	otb_id_property->SetHelpString("Minor items version used when comparing maps against this client.");
	auto* otb_major_property = client_prop_grid->Append(new wxIntProperty("OTB Major Version", "otbMajor", active_client->getOtbMajor()));
	otb_major_property->SetHelpString("Major OTB format version for this client.");
	wxString otbm_versions_text;
	for (const auto version_id : active_client->getMapVersionsSupported()) {
		if (!otbm_versions_text.IsEmpty()) {
			otbm_versions_text += ", ";
		}
		otbm_versions_text += wxString::Format("%d", static_cast<int>(version_id) + 1);
	}
	auto* otbm_property = client_prop_grid->Append(new wxStringProperty("Supported OTBM Versions", "otbmVersions", otbm_versions_text));
	otbm_property->SetHelpString("Comma-separated list of supported OTBM versions, usually 1 through 4.");

	client_prop_grid->Append(new wxPropertyCategory("Signatures"));
	auto* dat_signature_property = client_prop_grid->Append(new wxStringProperty("DAT Signature", "datSignature", wxString::Format("%X", active_client->getDatSignature())));
	dat_signature_property->SetHelpString("Expected DAT signature written in hexadecimal.");
	auto* spr_signature_property = client_prop_grid->Append(new wxStringProperty("SPR Signature", "sprSignature", wxString::Format("%X", active_client->getSprSignature())));
	spr_signature_property->SetHelpString("Expected SPR signature written in hexadecimal.");

	client_prop_grid->Append(new wxPropertyCategory("Features"));
	client_prop_grid->Append(new wxBoolProperty("Transparency", "transparency", active_client->isTransparent()))->SetHelpString("Enable transparency handling for this client.");
	client_prop_grid->Append(new wxBoolProperty("Extended", "extended", active_client->isExtended()))->SetHelpString("Use the extended sprite or item feature set for this client.");
	client_prop_grid->Append(new wxBoolProperty("Frame Durations", "frameDurations", active_client->hasFrameDurations()))->SetHelpString("Treat animations as having per-frame durations.");
	client_prop_grid->Append(new wxBoolProperty("Frame Groups", "frameGroups", active_client->hasFrameGroups()))->SetHelpString("Treat animations as having separate frame groups.");

	wxPropertyGridIterator iterator = client_prop_grid->GetIterator();
	for (; !iterator.AtEnd(); ++iterator) {
		UpdatePropertyValidation(*iterator);
	}

	client_prop_grid->ExpandAll();
	client_prop_grid->Refresh();
}

bool ClientVersionPage::ResolvePendingChanges(ClientVersion* client) {
	if (!client || !client->isDirty()) {
		return true;
	}

	const int result = wxMessageBox(
		"Save changes to " + wxstr(client->getName()) + "?",
		"Unsaved Changes",
		wxYES_NO | wxCANCEL | wxICON_QUESTION,
		this
	);
	if (result == wxYES) {
		return true;
	}
	if (result == wxNO) {
		client->restore();
		client->clearDirty();
		ClearPropertyStates(client);
		return true;
	}
	return false;
}

bool ClientVersionPage::IsPendingDeletion(const ClientVersion& version) const {
	return pending_deleted_client_ids.contains(version.getName());
}

void ClientVersionPage::SetPropertyState(ClientVersion* client, std::string_view property_name, PropertyVisualState state) {
	if (!client || property_name.empty()) {
		return;
	}

	auto& states = property_states_[client];
	states[std::string { property_name }] = state;
}

ClientVersionPage::PropertyVisualState ClientVersionPage::GetPropertyState(const ClientVersion* client, std::string_view property_name) const {
	if (!client || property_name.empty()) {
		return PropertyVisualState::Default;
	}

	const auto client_it = property_states_.find(client);
	if (client_it == property_states_.end()) {
		return PropertyVisualState::Default;
	}

	const auto property_it = client_it->second.find(std::string { property_name });
	if (property_it == client_it->second.end()) {
		return PropertyVisualState::Default;
	}

	return property_it->second;
}

void ClientVersionPage::ClearPropertyStates(ClientVersion* client) {
	if (!client) {
		return;
	}

	property_states_.erase(client);
}

void ClientVersionPage::MarkSavedProperties() {
	for (auto& [client, states] : property_states_) {
		(void)client;
		for (auto& [property_name, state] : states) {
			(void)property_name;
			if (state == PropertyVisualState::Pending) {
				state = PropertyVisualState::Saved;
			}
		}
	}
}

void ClientVersionPage::ApplyDetectionResult(ClientVersion& client, const ClientAssetDetectionResult& result) {
	const auto applyPendingState = [&](std::string_view property_name) {
		SetPropertyState(&client, property_name, PropertyVisualState::Pending);
	};
	const auto applyUndetectedState = [&](std::string_view property_name) {
		SetPropertyState(&client, property_name, PropertyVisualState::Undetected);
	};

	if (result.metadata_file_name.has_value()) {
		client.setMetadataFile(*result.metadata_file_name);
		applyPendingState("metadataFile");
	} else {
		applyUndetectedState("metadataFile");
	}

	if (result.sprites_file_name.has_value()) {
		client.setSpritesFile(*result.sprites_file_name);
		applyPendingState("spritesFile");
	} else {
		applyUndetectedState("spritesFile");
	}

	if (result.dat_signature.has_value()) {
		client.setDatSignature(*result.dat_signature);
		applyPendingState("datSignature");
	} else {
		applyUndetectedState("datSignature");
	}

	if (result.spr_signature.has_value()) {
		client.setSprSignature(*result.spr_signature);
		applyPendingState("sprSignature");
	} else {
		applyUndetectedState("sprSignature");
	}

	if (result.dat_format.has_value() && result.dat_signature.has_value() && result.spr_signature.has_value()) {
		client.setClientData(*result.dat_signature, *result.spr_signature, *result.dat_format);
	} else if (result.dat_format.has_value()) {
		client.setDatFormat(*result.dat_format);
	}

	if (result.transparency.has_value()) {
		client.setTransparent(*result.transparency);
		applyPendingState("transparency");
	} else {
		applyUndetectedState("transparency");
	}

	if (result.extended.has_value()) {
		client.setExtended(*result.extended);
		applyPendingState("extended");
	} else {
		applyUndetectedState("extended");
	}

	if (result.frame_durations.has_value()) {
		client.setFrameDurations(*result.frame_durations);
		applyPendingState("frameDurations");
	} else {
		applyUndetectedState("frameDurations");
	}

	if (result.frame_groups.has_value()) {
		client.setFrameGroups(*result.frame_groups);
		applyPendingState("frameGroups");
	} else {
		applyUndetectedState("frameGroups");
	}

	for (const auto& warning : result.warnings) {
		wxLogWarning("%s", wxString::FromUTF8(warning));
	}
}

void ClientVersionPage::RequestClientAssetDetection(ClientVersion& client) {
	const auto client_id = client.getID();
	const auto requested_path = client.getClientPath().GetFullPath().ToStdString();
	const auto request_id = ++next_detection_request_id;
	auto snapshot = client.clone();
	wxWeakRef<ClientVersionPage> weak_this(this);
	pending_detection_requests_[client_id] = request_id;

	std::thread([weak_this, client_id, requested_path, request_id, snapshot = std::move(snapshot)]() mutable {
		auto detection = ClientAssetDetector::detect(*snapshot);
		if (!weak_this) {
			return;
		}

		weak_this->CallAfter([weak_this, client_id, requested_path, request_id, detection = std::move(detection)]() mutable {
			if (!weak_this) {
				return;
			}
			auto& self = *weak_this;

			const auto pending_it = self.pending_detection_requests_.find(client_id);
			if (pending_it == self.pending_detection_requests_.end() || pending_it->second != request_id) {
				return;
			}
			self.pending_detection_requests_.erase(pending_it);

			auto* client = ClientVersion::get(client_id);
			if (!client) {
				return;
			}
			if (client->getClientPath().GetFullPath().ToStdString() != requested_path) {
				return;
			}

			self.ApplyDetectionResult(*client, detection);
			if (self.active_client == client) {
				self.SyncClientPropertiesToGrid(*client);
				self.RefreshSummary();
			}
		});
	}).detach();
}

void ClientVersionPage::SyncClientPropertiesToGrid(const ClientVersion& client) {
	if (!client_prop_grid) {
		return;
	}

	suppress_property_events = true;
	UpdateGridProperty(client_prop_grid, "Version", static_cast<long>(client.getVersion()));
	UpdateGridProperty(client_prop_grid, "Name", wxstr(client.getName()));
	UpdateGridProperty(client_prop_grid, "description", wxstr(client.getDescription()));
	UpdateGridProperty(client_prop_grid, "configType", SelectionFromConfigType(client.getConfigType()));
	UpdateGridProperty(client_prop_grid, "clientPath", client.getClientPath().GetFullPath());
	UpdateGridProperty(client_prop_grid, "dataDirectory", wxstr(client.getDataDirectory()));
	UpdateGridProperty(client_prop_grid, "metadataFile", wxstr(client.getMetadataFile()));
	UpdateGridProperty(client_prop_grid, "spritesFile", wxstr(client.getSpritesFile()));
	UpdateGridProperty(client_prop_grid, "otbId", static_cast<long>(client.getOtbId()));
	UpdateGridProperty(client_prop_grid, "otbMajor", static_cast<long>(client.getOtbMajor()));

	wxString otbm_versions_text;
	for (const auto version_id : client.getMapVersionsSupported()) {
		if (!otbm_versions_text.IsEmpty()) {
			otbm_versions_text += ", ";
		}
		otbm_versions_text += wxString::Format("%d", static_cast<int>(version_id) + 1);
	}
	UpdateGridProperty(client_prop_grid, "otbmVersions", otbm_versions_text);
	UpdateGridProperty(client_prop_grid, "datSignature", wxString::Format("%X", client.getDatSignature()));
	UpdateGridProperty(client_prop_grid, "sprSignature", wxString::Format("%X", client.getSprSignature()));
	UpdateGridProperty(client_prop_grid, "transparency", client.isTransparent());
	UpdateGridProperty(client_prop_grid, "extended", client.isExtended());
	UpdateGridProperty(client_prop_grid, "frameDurations", client.hasFrameDurations());
	UpdateGridProperty(client_prop_grid, "frameGroups", client.hasFrameGroups());
	suppress_property_events = false;

	for (wxPropertyGridIterator iterator = client_prop_grid->GetIterator(); !iterator.AtEnd(); ++iterator) {
		UpdatePropertyValidation(*iterator);
	}
	client_prop_grid->Refresh();
}

void ClientVersionPage::UpdatePropertyValidation(wxPGProperty* prop) {
	if (!prop) {
		return;
	}

	bool invalid = false;
	if (prop->GetName() == "Name") {
		const auto name = nstr(prop->GetValueAsString());
		invalid = name.empty();
		if (!invalid && active_client) {
			for (auto* other : ClientVersion::getAll()) {
				if (other != active_client && other->getName() == name) {
					invalid = true;
					break;
				}
			}
		}
	}

	const wxColour background = Theme::Get(Theme::Role::Background);
	if (invalid) {
		prop->SetBackgroundColour(BlendColour(background, Theme::Get(Theme::Role::Error), 0.28));
		return;
	}

	switch (GetPropertyState(active_client, nstr(prop->GetName()))) {
		case PropertyVisualState::Pending:
			prop->SetBackgroundColour(BlendColour(background, Theme::Get(Theme::Role::Warning), 0.30));
			break;
		case PropertyVisualState::Undetected:
			prop->SetBackgroundColour(BlendColour(background, Theme::Get(Theme::Role::Error), 0.30));
			break;
		case PropertyVisualState::Saved:
			prop->SetBackgroundColour(BlendColour(background, Theme::Get(Theme::Role::Success), 0.30));
			break;
		case PropertyVisualState::Default:
		default:
			prop->SetBackgroundColour(background);
			break;
	}
}

void ClientVersionPage::OnClientSelected(wxTreeEvent& event) {
	if (ignore_tree_selection) {
		return;
	}

	// Ignore selection events queued for items removed during a tree rebuild.
	if (!event.GetItem().IsOk() || event.GetItem() != client_tree_ctrl->GetSelection()) {
		return;
	}

	ClientVersion* requested_client = GetSelectedClient();
	if (requested_client == active_client) {
		RefreshClientEditor();
		return;
	}

	const bool had_dirty_changes = active_client && active_client->isDirty();
	ClientVersion* previous_client = active_client;
	if (!ResolvePendingChanges(previous_client)) {
		ignore_tree_selection = true;
		SelectClient(previous_client);
		ignore_tree_selection = false;
		return;
	}

	if (had_dirty_changes && !previous_client->isDirty()) {
		PopulateDefaultVersionChoice();
		PopulateClientTree();
		ignore_tree_selection = true;
		SelectClient(requested_client);
		ignore_tree_selection = false;
		requested_client = GetSelectedClient();
	}

	active_client = requested_client;
	if (active_client) {
		active_client->backup();
	}
	RefreshClientEditor();
}

void ClientVersionPage::OnSearchChanged(wxCommandEvent& WXUNUSED(event)) {
	const wxString new_search_text = client_search_ctrl->GetValue();
	const std::string new_filter = LowercaseCopy(nstr(new_search_text));
	ClientVersion* previous_client = active_client;
	if (previous_client && previous_client->isDirty() && !MatchesClientFilter(*previous_client, new_filter)) {
		if (!ResolvePendingChanges(previous_client)) {
			client_search_ctrl->ChangeValue(last_search_text);
			client_search_ctrl->SetFocus();
			client_search_ctrl->SetInsertionPointEnd();
			return;
		}
	}

	client_filter = new_filter;
	last_search_text = new_search_text;
	PopulateClientTree();
	active_client = GetSelectedClient();
	if (!active_client && previous_client && MatchesFilter(*previous_client)) {
		active_client = previous_client;
		SelectClient(active_client);
	}
	if (active_client && active_client != previous_client) {
		active_client->backup();
	}
	RefreshClientEditor();
	client_search_ctrl->SetFocus();
	client_search_ctrl->SetInsertionPointEnd();
}

void ClientVersionPage::OnSearchCancelled(wxCommandEvent& WXUNUSED(event)) {
	client_search_ctrl->ChangeValue("");
	last_search_text.clear();
	client_filter.clear();
	PopulateClientTree();
	active_client = GetSelectedClient();
	RefreshClientEditor();
	client_search_ctrl->SetFocus();
}

void ClientVersionPage::OnTreeContextMenu(wxTreeEvent& event) {
	auto* data = dynamic_cast<TreeItemData*>(client_tree_ctrl->GetItemData(event.GetItem()));
	if (!data || !data->cv) {
		return;
	}

	if (client_tree_ctrl->GetSelection() != event.GetItem()) {
		client_tree_ctrl->SelectItem(event.GetItem());
		if (client_tree_ctrl->GetSelection() != event.GetItem()) {
			return;
		}
	}

	wxMenu menu;
	menu.Append(wxID_COPY, "Duplicate")->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_COPY));
	menu.Bind(wxEVT_MENU, &ClientVersionPage::OnDuplicateClient, this, wxID_COPY);
	PopupMenu(&menu);
}

void ClientVersionPage::OnDuplicateClient(wxCommandEvent& WXUNUSED(event)) {
	ClientVersion* current = active_client ? active_client : GetSelectedClient();
	if (!current) {
		return;
	}

	auto duplicate = current->clone();
	duplicate->setName(current->getName() + " (Copy)");
	duplicate->markDirty();

	ClientVersion* duplicate_ptr = duplicate.get();
	ClientVersion::addVersion(std::move(duplicate));
	PopulateDefaultVersionChoice();
	PopulateClientTree();
	active_client = duplicate_ptr;
	active_client->backup();
	active_client->markDirty();
	SelectClient(active_client);
	RefreshClientEditor();
}

void ClientVersionPage::OnPropertyChanged(wxPropertyGridEvent& event) {
	if (suppress_property_events) {
		return;
	}

	ClientVersion* client = active_client;
	if (!client) {
		return;
	}

	auto* prop = event.GetProperty();
	const auto prop_name = prop->GetName();
	const wxAny value = prop->GetValue();

	if (prop_name == "Version") {
		client->setVersion(value.As<int>());
	} else if (prop_name == "Name") {
		client->setName(nstr(value.As<wxString>()));
	} else if (prop_name == "description") {
		client->setDescription(nstr(value.As<wxString>()));
	} else if (prop_name == "configType") {
		client->setConfigType(ConfigTypeFromSelection(prop->GetValue().GetLong()));
	} else if (prop_name == "clientPath") {
		client->setClientPath(FileName(value.As<wxString>()));
		RequestClientAssetDetection(*client);
	} else if (prop_name == "dataDirectory") {
		client->setDataDirectory(nstr(value.As<wxString>()));
	} else if (prop_name == "metadataFile") {
		client->setMetadataFile(nstr(value.As<wxString>()));
	} else if (prop_name == "spritesFile") {
		client->setSpritesFile(nstr(value.As<wxString>()));
	} else if (prop_name == "otbId") {
		client->setOtbId(value.As<int>());
	} else if (prop_name == "otbMajor") {
		client->setOtbMajor(value.As<int>());
	} else if (prop_name == "otbmVersions") {
		client->getMapVersionsSupported().clear();
		wxStringTokenizer tokenizer(value.As<wxString>(), ", ");
		while (tokenizer.HasMoreTokens()) {
			long version = 0;
			if (tokenizer.GetNextToken().ToLong(&version) && version >= 1 && version <= 5) {
				client->getMapVersionsSupported().push_back(static_cast<MapVersionID>(version - 1));
			}
		}
	} else if (prop_name == "datSignature") {
		uint32_t signature = 0;
		auto text = nstr(value.As<wxString>());
		const auto end = text.data() + text.size();
		const auto result = std::from_chars(text.data(), end, signature, 16);
		if (result.ec == std::errc() && result.ptr == end) {
			client->setDatSignature(signature);
		} else {
			prop->SetValue(wxString::Format("%X", client->getDatSignature()));
		}
	} else if (prop_name == "sprSignature") {
		uint32_t signature = 0;
		auto text = nstr(value.As<wxString>());
		const auto end = text.data() + text.size();
		const auto result = std::from_chars(text.data(), end, signature, 16);
		if (result.ec == std::errc() && result.ptr == end) {
			client->setSprSignature(signature);
		} else {
			prop->SetValue(wxString::Format("%X", client->getSprSignature()));
		}
	} else if (prop_name == "transparency") {
		client->setTransparent(value.As<bool>());
	} else if (prop_name == "extended") {
		client->setExtended(value.As<bool>());
	} else if (prop_name == "frameDurations") {
		client->setFrameDurations(value.As<bool>());
	} else if (prop_name == "frameGroups") {
		client->setFrameGroups(value.As<bool>());
	}

	if (IsAutoDetectedProperty(nstr(prop_name))) {
		SetPropertyState(client, nstr(prop_name), PropertyVisualState::Pending);
	}

	client->markDirty();
	PopulateDefaultVersionChoice();
	if (prop_name == "Name" || prop_name == "Version") {
		PopulateClientTree();
		SelectClient(client);
	}

	UpdatePropertyValidation(prop);
	client_prop_grid->Refresh();
	RefreshSummary();
}

void ClientVersionPage::OnAddClient(wxCommandEvent& WXUNUSED(event)) {
	std::string new_name = "New Client";
	int counter = 1;
	while (ClientVersion::get(new_name)) {
		new_name = "New Client " + std::to_string(counter++);
	}

	OtbVersion otb;
	otb.name = new_name;
	otb.id = PROTOCOL_VERSION_NONE;
	otb.format_version = OTB_VERSION_1;

	auto client = std::make_unique<ClientVersion>(otb, new_name, kDefaultDataDirectory);
	client->setMetadataFile("Tibia.dat");
	client->setSpritesFile("Tibia.spr");
	client->markDirty();

	ClientVersion* client_ptr = client.get();
	ClientVersion::addVersion(std::move(client));
	PopulateDefaultVersionChoice();
	PopulateClientTree();
	active_client = client_ptr;
	active_client->backup();
	active_client->markDirty();
	SelectClient(active_client);
	RefreshClientEditor();
}

void ClientVersionPage::OnDeleteClient(wxCommandEvent& WXUNUSED(event)) {
	ClientVersion* client = active_client ? active_client : GetSelectedClient();
	if (!client) {
		return;
	}

	if (wxMessageBox("Are you sure you want to delete " + wxstr(client->getName()) + "?", "Confirm Delete", wxYES_NO | wxICON_WARNING, this) != wxYES) {
		return;
	}

	pending_deleted_client_ids.insert(client->getName());
	ClearPropertyStates(client);

	PopulateDefaultVersionChoice();
	active_client = nullptr;
	PopulateClientTree();
	active_client = GetSelectedClient();
	if (active_client) {
		active_client->backup();
	}
	RefreshClientEditor();
}

bool ClientVersionPage::ValidateData() {
	std::unordered_set<std::string> names;
	for (auto* client : ClientVersion::getAll()) {
		if (IsPendingDeletion(*client)) {
			continue;
		}
		if (!client->isValid()) {
			wxMessageBox("Client '" + wxstr(client->getName()) + "' has invalid data. The name cannot be empty.", "Invalid Client Data", wxOK | wxICON_ERROR, this);
			SelectClient(client);
			active_client = client;
			RefreshClientEditor();
			return false;
		}

		if (const auto [_, inserted] = names.insert(client->getName()); !inserted) {
			wxMessageBox("Client '" + wxstr(client->getName()) + "' has a duplicate name. Rename one of them before saving.", "Duplicate Client Name", wxOK | wxICON_ERROR, this);
			SelectClient(client);
			active_client = client;
			RefreshClientEditor();
			return false;
		}
	}
	return true;
}

void ClientVersionPage::Apply() {
	g_settings.setInteger(Config::CHECK_SIGNATURES, check_sigs_chkbox->GetValue());

	if (default_version_choice->GetSelection() != wxNOT_FOUND) {
		const auto default_name = nstr(default_version_choice->GetStringSelection());
		if (auto* default_client = ClientVersion::get(default_name)) {
			ClientVersion::setLatestVersion(default_client);
			g_settings.setInteger(Config::DEFAULT_CLIENT_VERSION, default_client->getProtocolID());
		}
	} else {
		ClientVersion::setLatestVersion(nullptr);
		g_settings.setInteger(Config::DEFAULT_CLIENT_VERSION, 0);
	}

	bool any_dirty = !pending_deleted_client_ids.empty();
	for (auto* client : ClientVersion::getAll()) {
		if (IsPendingDeletion(*client)) {
			continue;
		}
		if (client->isDirty()) {
			any_dirty = true;
			break;
		}
	}
	if (!any_dirty) {
		return;
	}

	std::vector<std::unique_ptr<ClientVersion>> deleted_backups;
	deleted_backups.reserve(pending_deleted_client_ids.size());
	for (const auto& id : pending_deleted_client_ids) {
		if (auto* client = ClientVersion::get(id)) {
			deleted_backups.push_back(client->clone());
		}
	}

	const std::string previous_latest_name = ClientVersion::getLatestVersion() ? ClientVersion::getLatestVersion()->getName() : std::string {};
	for (const auto& id : pending_deleted_client_ids) {
		ClientVersion::removeVersion(id);
	}

	if (ClientVersion::saveVersions()) {
		for (auto* client : ClientVersion::getAll()) {
			client->clearDirty();
			client->backup();
		}
		MarkSavedProperties();
		pending_deleted_client_ids.clear();
		PopulateDefaultVersionChoice();
		PopulateClientTree();
		active_client = GetSelectedClient();
		if (active_client) {
			active_client->backup();
		}
		RefreshClientEditor();
		RefreshSummary();
	} else {
		for (auto& client : deleted_backups) {
			ClientVersion::addVersion(std::move(client));
		}
		if (!previous_latest_name.empty()) {
			ClientVersion::setLatestVersion(ClientVersion::get(previous_latest_name));
		}
		PopulateDefaultVersionChoice();
		PopulateClientTree();
		active_client = GetSelectedClient();
		RefreshClientEditor();
		wxMessageBox("Failed to save client versions. Check the log for details.", "Save Error", wxOK | wxICON_ERROR, this);
	}
}

void ClientVersionPage::DiscardPendingChanges() {
	for (auto* client : ClientVersion::getAll()) {
		if (client->isDirty()) {
			client->restore();
			client->clearDirty();
		}
	}

	property_states_.clear();
	pending_deleted_client_ids.clear();
	client_filter.clear();
	last_search_text.clear();
	if (client_search_ctrl) {
		client_search_ctrl->ChangeValue("");
	}

	PopulateDefaultVersionChoice();
	PopulateClientTree();
	active_client = GetSelectedClient();
	RefreshClientEditor();
}

