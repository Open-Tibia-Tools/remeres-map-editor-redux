#include "ui/map/export_minimap_window.h"

#include "app/application.h"
#include "editor/editor.h"
#include "ui/dialog_util.h"
#include "ui/gui.h"
#include "ui/gui_ids.h"
#include "util/image_manager.h"

#include <algorithm>
#include <array>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/dirdlg.h>
#include <wx/notebook.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace {

constexpr std::array<int, 8> kImageSizes { 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536 };
constexpr int kMaxSafeImageDimension = 16384;

[[nodiscard]] wxString defaultFileName(const Editor& editor) {
	const wxString mapName = wxString::FromUTF8(editor.map.getName().c_str());
	if (mapName.empty()) {
		return "minimap";
	}

	const wxFileName fileName(mapName);
	const wxString baseName = fileName.GetName();
	return baseName.empty() ? mapName : baseName;
}

[[nodiscard]] bool hasInvalidFileNameChars(const wxString& value) {
	const std::string fileName = value.ToStdString();
	return fileName.find_first_of("<>:\"/\\|?*") != std::string::npos;
}

[[nodiscard]] size_t formatIndex(MinimapExportFormat format) {
	return static_cast<size_t>(format);
}

[[nodiscard]] int maxImageSizeForFormat(MinimapExportFormat format) {
	switch (format) {
		case MinimapExportFormat::Webp:
			return std::min(kMaxWebpDimension, kMaxSafeImageDimension);
		case MinimapExportFormat::Jpg:
			return std::min(kMaxJpegDimension, kMaxSafeImageDimension);
		case MinimapExportFormat::Otmm:
			return 0;
	}
	return 0;
}

} // namespace

ExportMinimapWindow::ExportMinimapWindow(wxWindow* parent, Editor& editor) :
	wxDialog(parent, wxID_ANY, "Export Minimap", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
	editor_(editor) {
	auto* sizer = newd wxBoxSizer(wxVERTICAL);

	error_field_ = newd wxStaticText(this, wxID_VIEW_DETAILS, "", wxDefaultPosition, wxDefaultSize);
	error_field_->SetForegroundColour(*wxRED);
	auto* errorSizer = newd wxBoxSizer(wxHORIZONTAL);
	errorSizer->Add(error_field_, 0, wxALL, 5);
	sizer->Add(errorSizer, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 5);

	auto* contentSizer = newd wxBoxSizer(wxHORIZONTAL);
	auto* leftSizer = newd wxBoxSizer(wxVERTICAL);

	auto* folderSizer = newd wxStaticBoxSizer(wxHORIZONTAL, this, "Output Folder");
	wxWindow* folderParent = folderSizer->GetStaticBox();
	directory_text_field_ = newd wxTextCtrl(folderParent, wxID_ANY, wxString(g_settings.getString(Config::MINIMAP_EXPORT_DIR)), wxDefaultPosition, wxDefaultSize);
	directory_text_field_->Bind(wxEVT_TEXT, &ExportMinimapWindow::OnTextChanged, this);
	folderSizer->Add(directory_text_field_, 1, wxALL | wxEXPAND, 5);
	auto* browseButton = newd wxButton(folderParent, MAP_WINDOW_FILE_BUTTON, "Browse");
	browseButton->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_FOLDER_OPEN));
	folderSizer->Add(browseButton, 0, wxALL, 5);
	leftSizer->Add(folderSizer, 0, wxBOTTOM | wxEXPAND, 5);

	auto* fileSizer = newd wxStaticBoxSizer(wxHORIZONTAL, this, "File Name");
	wxWindow* fileParent = fileSizer->GetStaticBox();
	file_name_text_field_ = newd wxTextCtrl(fileParent, wxID_ANY, defaultFileName(editor_), wxDefaultPosition, wxDefaultSize);
	file_name_text_field_->Bind(wxEVT_TEXT, &ExportMinimapWindow::OnTextChanged, this);
	fileSizer->Add(file_name_text_field_, 1, wxALL | wxEXPAND, 5);
	leftSizer->Add(fileSizer, 0, wxBOTTOM | wxEXPAND, 5);

	notebook_ = newd wxNotebook(this, wxID_ANY);
	notebook_->AddPage(CreateFormatPage(notebook_, MinimapExportFormat::Jpg), "JPG", true);
	notebook_->AddPage(CreateFormatPage(notebook_, MinimapExportFormat::Webp), "WebP");
	notebook_->AddPage(CreateFormatPage(notebook_, MinimapExportFormat::Otmm), "OTMM");
	notebook_->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, [this](wxBookCtrlEvent& event) {
		UpdateControlState();
		CheckValues();
		event.Skip();
	});
	leftSizer->Add(notebook_, 1, wxEXPAND);

	contentSizer->Add(leftSizer, 1, wxALL | wxEXPAND, 5);
	contentSizer->Add(CreateAreaSizer(), 0, wxTOP | wxRIGHT | wxBOTTOM | wxEXPAND, 5);
	sizer->Add(contentSizer, 1, wxEXPAND);

	auto* buttonSizer = newd wxBoxSizer(wxHORIZONTAL);
	ok_button_ = newd wxButton(this, wxID_OK, "OK");
	ok_button_->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_CHECK));
	buttonSizer->Add(ok_button_, wxSizerFlags(1).Center());
	auto* cancelButton = newd wxButton(this, wxID_CANCEL, "Cancel");
	cancelButton->SetBitmap(IMAGE_MANAGER.GetBitmapBundle(ICON_XMARK));
	buttonSizer->Add(cancelButton, wxSizerFlags(1).Center());
	sizer->Add(buttonSizer, 0, wxCENTER, 10);

	SetSizerAndFit(sizer);
	SetMinSize(GetSize());
	Centre(wxBOTH);
	UpdateControlState();
	CheckValues();

	browseButton->Bind(wxEVT_BUTTON, &ExportMinimapWindow::OnClickBrowse, this);
	ok_button_->Bind(wxEVT_BUTTON, &ExportMinimapWindow::OnClickOK, this);
	cancelButton->Bind(wxEVT_BUTTON, &ExportMinimapWindow::OnClickCancel, this);
	SetIcons(IMAGE_MANAGER.GetIconBundle(ICON_FILE_EXPORT));
}

ExportMinimapWindow::~ExportMinimapWindow() = default;

void ExportMinimapWindow::OnClickBrowse(wxCommandEvent& WXUNUSED(event)) {
	wxDirDialog dialog(this, "Select the output folder", directory_text_field_->GetValue(), wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
	if (dialog.ShowModal() == wxID_OK) {
		directory_text_field_->ChangeValue(dialog.GetPath());
	}
	CheckValues();
}

void ExportMinimapWindow::OnTextChanged(wxCommandEvent& event) {
	CheckValues();
	event.Skip();
}

void ExportMinimapWindow::OnChoiceChanged(wxCommandEvent& event) {
	UpdateControlState();
	CheckValues();
	event.Skip();
}

void ExportMinimapWindow::OnClickOK(wxCommandEvent& WXUNUSED(event)) {
	const wxFileName directory = wxFileName::DirName(directory_text_field_->GetValue());
	g_settings.setString(Config::MINIMAP_EXPORT_DIR, directory.GetFullPath().ToStdString());

	MinimapExportOptions options {
		.outputDirectory = directory,
		.fileBaseName = FileBaseName(),
		.format = SelectedFormat(),
		.selectedFloors = SelectedFloors(),
		.imageSize = SelectedImageSize(),
		.showAllFloors = CurrentControls().show_all_floors_checkbox ? CurrentControls().show_all_floors_checkbox->GetValue() : false,
		.applyShadeToAdjacentFloors = CurrentControls().shade_adjacent_floors_checkbox ? CurrentControls().shade_adjacent_floors_checkbox->GetValue() : false,
	};

	g_gui.CreateLoadBar("Exporting Minimap");
	const MinimapExportResult result = MinimapExporter::Export(editor_, options, [](uint64_t completed, uint64_t total) {
		if (total > 0) {
			g_gui.SetLoadDone(static_cast<int32_t>((completed * 100) / total));
		}
	});
	g_gui.DestroyLoadBar();

	if (!result.ok) {
		DialogUtil::PopupDialog(this, "Error", wxString::FromUTF8(result.error.c_str()), wxOK);
		return;
	}

	if (result.filesWritten == 0) {
		DialogUtil::PopupDialog(this, "Export Minimap", "No minimap tiles found to export.", wxOK);
	} else {
		DialogUtil::PopupDialog(this, "Export Minimap", wxString::Format("Exported %llu minimap file(s).", static_cast<unsigned long long>(result.filesWritten)), wxOK);
	}
	EndModal(wxID_OK);
}

void ExportMinimapWindow::OnClickCancel(wxCommandEvent& WXUNUSED(event)) {
	EndModal(wxID_CANCEL);
}

void ExportMinimapWindow::CheckValues() {
	if (directory_text_field_->IsEmpty()) {
		error_field_->SetLabel("Type or select an output folder.");
		ok_button_->Enable(false);
		return;
	}
	if (file_name_text_field_->IsEmpty()) {
		error_field_->SetLabel("Type a name for the file.");
		ok_button_->Enable(false);
		return;
	}
	if (hasInvalidFileNameChars(file_name_text_field_->GetValue())) {
		error_field_->SetLabel("File name contains invalid characters.");
		ok_button_->Enable(false);
		return;
	}

	const wxFileName directory = wxFileName::DirName(directory_text_field_->GetValue());
	if (!directory.DirExists()) {
		error_field_->SetLabel("Output folder not found.");
		ok_button_->Enable(false);
		return;
	}
	if (!directory.IsDirWritable()) {
		error_field_->SetLabel("Output folder is not writable.");
		ok_button_->Enable(false);
		return;
	}
	if (std::ranges::none_of(SelectedFloors(), [](bool selected) { return selected; })) {
		error_field_->SetLabel("Select at least one floor.");
		ok_button_->Enable(false);
		return;
	}

	error_field_->SetLabel(wxEmptyString);
	ok_button_->Enable(true);
}

void ExportMinimapWindow::UpdateControlState() {
	for (FormatControls& controls : format_controls_) {
		if (!controls.shade_adjacent_floors_checkbox) {
			continue;
		}

		const bool canShade = controls.show_all_floors_checkbox && controls.show_all_floors_checkbox->GetValue();
		controls.shade_adjacent_floors_checkbox->Enable(canShade);
	}
}

MinimapExportFormat ExportMinimapWindow::SelectedFormat() const {
	if (!notebook_) {
		return MinimapExportFormat::Jpg;
	}

	switch (notebook_->GetSelection()) {
		case 1:
			return MinimapExportFormat::Webp;
		case 2:
			return MinimapExportFormat::Otmm;
		default:
			return MinimapExportFormat::Jpg;
	}
}

int ExportMinimapWindow::SelectedImageSize() const {
	const wxChoice* choice = CurrentControls().image_size_choice;
	const std::vector<int>& imageSizes = CurrentControls().image_sizes;
	if (!choice || imageSizes.empty()) {
		return kImageSizes[1];
	}

	const int index = std::clamp(choice->GetSelection(), 0, static_cast<int>(imageSizes.size() - 1));
	return imageSizes[static_cast<size_t>(index)];
}

MinimapExportFloorMask ExportMinimapWindow::SelectedFloors() const {
	MinimapExportFloorMask floors {};
	for (int floor = 0; floor < MAP_LAYERS; ++floor) {
		const wxCheckBox* checkbox = floor_checkboxes_[static_cast<size_t>(floor)];
		floors[static_cast<size_t>(floor)] = checkbox && checkbox->GetValue();
	}
	return floors;
}

std::string ExportMinimapWindow::FileBaseName() const {
	const wxString text = file_name_text_field_->GetValue();
	const wxFileName fileName(text);
	const wxString baseName = fileName.GetName();
	return (baseName.empty() ? text : baseName).ToStdString();
}

wxPanel* ExportMinimapWindow::CreateFormatPage(wxNotebook* notebook, MinimapExportFormat format) {
	auto* panel = newd wxPanel(notebook, wxID_ANY);
	auto* sizer = newd wxBoxSizer(wxVERTICAL);
	FormatControls& controls = format_controls_[formatIndex(format)];

	if (format != MinimapExportFormat::Otmm) {
		auto* imageSizeSizer = newd wxStaticBoxSizer(wxHORIZONTAL, panel, "Image Size");
		wxWindow* imageSizeParent = imageSizeSizer->GetStaticBox();
		controls.image_size_choice = newd wxChoice(imageSizeParent, wxID_ANY);
		const int maxImageSize = maxImageSizeForFormat(format);
		for (const int size : kImageSizes) {
			if (size > maxImageSize) {
				continue;
			}
			controls.image_sizes.push_back(size);
			controls.image_size_choice->Append(wxString::Format("%d x %d", size, size));
		}
		controls.image_size_choice->SetSelection(1);
		controls.image_size_choice->Bind(wxEVT_CHOICE, &ExportMinimapWindow::OnChoiceChanged, this);
		imageSizeSizer->Add(controls.image_size_choice, 1, wxALL | wxEXPAND, 5);
		sizer->Add(imageSizeSizer, 0, wxLEFT | wxRIGHT | wxTOP | wxEXPAND, 14);
	}

	if (format != MinimapExportFormat::Otmm) {
		controls.show_all_floors_checkbox = newd wxCheckBox(panel, wxID_ANY, "Enable show all floors during export");
		controls.show_all_floors_checkbox->SetValue(false);
		controls.show_all_floors_checkbox->Bind(wxEVT_CHECKBOX, &ExportMinimapWindow::OnChoiceChanged, this);
		sizer->Add(controls.show_all_floors_checkbox, 0, wxLEFT | wxRIGHT | wxTOP, 20);

		controls.shade_adjacent_floors_checkbox = newd wxCheckBox(panel, wxID_ANY, "Apply shade to adjacent floors");
		controls.shade_adjacent_floors_checkbox->SetValue(false);
		controls.shade_adjacent_floors_checkbox->Bind(wxEVT_CHECKBOX, &ExportMinimapWindow::OnChoiceChanged, this);
		sizer->Add(controls.shade_adjacent_floors_checkbox, 0, wxLEFT | wxRIGHT | wxTOP, 20);
	}

	sizer->AddStretchSpacer(1);
	panel->SetSizer(sizer);
	return panel;
}

wxSizer* ExportMinimapWindow::CreateAreaSizer() {
	auto* areaSizer = newd wxStaticBoxSizer(wxHORIZONTAL, this, "Area");
	wxWindow* areaParent = areaSizer->GetStaticBox();
	auto* surfaceSizer = newd wxBoxSizer(wxVERTICAL);
	auto* undergroundSizer = newd wxBoxSizer(wxVERTICAL);
	surfaceSizer->Add(newd wxStaticText(areaParent, wxID_ANY, "Surface floors"), 0, wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 4);
	undergroundSizer->Add(newd wxStaticText(areaParent, wxID_ANY, "Underground floors"), 0, wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 4);

	for (int floor = 0; floor < MAP_LAYERS; ++floor) {
		auto* checkbox = newd wxCheckBox(areaParent, wxID_ANY, wxString::Format("Floor %d", floor));
		checkbox->SetValue(true);
		checkbox->Bind(wxEVT_CHECKBOX, &ExportMinimapWindow::OnChoiceChanged, this);
		floor_checkboxes_[static_cast<size_t>(floor)] = checkbox;
		if (floor <= GROUND_LAYER) {
			surfaceSizer->Add(checkbox, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);
		} else {
			undergroundSizer->Add(checkbox, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);
		}
	}

	areaSizer->Add(surfaceSizer, 1, wxALL | wxEXPAND, 5);
	areaSizer->Add(undergroundSizer, 1, wxALL | wxEXPAND, 5);
	return areaSizer;
}

ExportMinimapWindow::FormatControls& ExportMinimapWindow::CurrentControls() {
	return format_controls_[formatIndex(SelectedFormat())];
}

const ExportMinimapWindow::FormatControls& ExportMinimapWindow::CurrentControls() const {
	return format_controls_[formatIndex(SelectedFormat())];
}
