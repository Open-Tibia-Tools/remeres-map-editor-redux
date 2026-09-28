#ifndef WELCOME_DIALOG_H
#define WELCOME_DIALOG_H

#include <optional>
#include <unordered_map>
#include <vector>

#include <wx/wx.h>

#include "io/iomap_otbm.h"
#include "ui/welcome/startup_types.h"

class StartupButton;
class StartupCardPanel;
class StartupInfoPanel;
class StartupListBox;

wxDECLARE_EVENT(WELCOME_DIALOG_ACTION, wxCommandEvent);

class WelcomeDialog : public wxDialog {
public:
	WelcomeDialog(const wxString& title_text, const wxString& version_text, const wxSize& size, const wxBitmap& rme_logo, const std::vector<wxString>& recent_files);
	~WelcomeDialog() override = default;

	std::optional<StartupLoadRequest> ConsumePendingLoadRequest();
	void RefreshConfiguredClients();
	[[nodiscard]] ClientVersion* selectedClient() const;

private:
	void BuildInterface(const wxString& title_text, const wxString& version_text, const wxBitmap& rme_logo);

	void OnPreferences(wxCommandEvent& event);
	void OnNewMap(wxCommandEvent& event);
	void OnBrowseMap(wxCommandEvent& event);
	void OnExit(wxCommandEvent& event);
	void OnLoadRequested(wxCommandEvent& event);
	void OnRecentMapSelected(wxCommandEvent& event);
	void OnRecentMapActivated(wxCommandEvent& event);
	void OnClientSelected(wxCommandEvent& event);
	void OnForceLoadChanged(wxCommandEvent& event);
	void OnSize(wxSizeEvent& event);

	void RefreshRecentMapList();
	void RefreshClientList();
	void RefreshMapInfoPanel();
	void RefreshClientInfoPanel();
	void RefreshFooterState();
	void UpdateStatusWrap();

	void SetSelectedMapIndex(int index);
	void SetSelectedClientIndex(int index, bool manual_selection);
	void AutoSelectMatchingClient();

	bool AttemptLoad(bool show_message);
	bool AddBrowsedMap(const wxString& path);

	void EnsurePeeked(const wxString& path);
	[[nodiscard]] const OTBMStartupPeekResult* GetSelectedMapInfo() const;
	[[nodiscard]] ClientVersion* GetSelectedClient() const;
	[[nodiscard]] StartupCompatibilityStatus GetCompatibilityStatus() const;
	[[nodiscard]] wxString BuildCompatibilityMessage() const;

	[[nodiscard]] int FindRecentMapIndex(const wxString& path) const;
	[[nodiscard]] static wxString FormatModifiedLabel(const wxString& path);
	[[nodiscard]] static std::string NormalizePathKey(const wxString& path);

	std::vector<StartupRecentMapEntry> m_recent_maps;
	std::vector<StartupConfiguredClientEntry> m_configured_clients;
	std::unordered_map<std::string, OTBMStartupPeekResult> m_peek_cache;
	std::optional<StartupLoadRequest> m_pending_load_request;

	StartupListBox* m_recent_list = nullptr;
	StartupListBox* m_client_list = nullptr;
	StartupInfoPanel* m_map_info_panel = nullptr;
	StartupInfoPanel* m_client_info_panel = nullptr;
	StartupButton* m_load_button = nullptr;
	StartupCardPanel* m_status_card = nullptr;
	wxCheckBox* m_force_load_checkbox = nullptr;
	wxStaticText* m_status_text = nullptr;
	int m_selected_map_index = wxNOT_FOUND;
	int m_selected_client_index = wxNOT_FOUND;
	bool m_has_manual_client_selection = false;
};

#endif
