#ifndef RME_CLIENT_MISMATCH_DIALOG_H_
#define RME_CLIENT_MISMATCH_DIALOG_H_

#include <wx/dialog.h>

#include <cstdint>

class ClientVersion;

enum class ClientMismatchChoice {
	ForceLoadActive,
	SwitchVersion,
	Cancel
};

class ClientMismatchDialog : public wxDialog {
public:
    ClientMismatchDialog(
        wxWindow* parent, const ClientVersion& current, const ClientVersion& target, uint32_t otb_major, int real_open_map_count,
        bool target_has_valid_paths
    );
    ~ClientMismatchDialog() override = default;

    [[nodiscard]] ClientMismatchChoice GetChoice() const
    {
        return choice;
    }

private:
	void OnForceLoad(wxCommandEvent& evt);
	void OnSwitchVersion(wxCommandEvent& evt);
	void OnCancel(wxCommandEvent& evt);

	ClientMismatchChoice choice = ClientMismatchChoice::Cancel;
};

#endif // RME_CLIENT_MISMATCH_DIALOG_H_
