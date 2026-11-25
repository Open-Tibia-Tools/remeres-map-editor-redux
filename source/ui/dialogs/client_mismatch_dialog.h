#ifndef RME_CLIENT_MISMATCH_DIALOG_H_
#define RME_CLIENT_MISMATCH_DIALOG_H_

#include <wx/dialog.h>

#include <string_view>

class ClientVersion;
enum class SwitchPreconditionStatus;
struct MapVersion;

enum class ClientMismatchChoice {
	ForceLoadActive,
	SwitchVersion,
	Cancel
};

class ClientMismatchDialog : public wxDialog {
public:
    ClientMismatchDialog(
        wxWindow* parent, const ClientVersion& current, const ClientVersion& target, const MapVersion& map_ver,
        std::string_view map_filename, int real_open_map_count, SwitchPreconditionStatus switch_status
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
