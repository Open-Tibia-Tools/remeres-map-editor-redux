#include "app/preferences/general_page.h"

#include <array>

#include "app/main.h"
#include "app/preferences/preferences_layout.h"
#include "app/settings.h"
#include "ui/gui.h"

namespace {
constexpr std::array<const char*, 5> kPositionChoiceLabels = {
	"Lua table",
	"Compact JSON",
	"CSV",
	"Tuple",
	"Constructor call",
};

int ClampPositionFormatSelection(int selection) {
	return std::clamp(selection, 0, static_cast<int>(kPositionChoiceLabels.size()) - 1);
}

wxString BuildPositionPreview(int selection) {
	switch (ClampPositionFormatSelection(selection)) {
		case 0:
			return "{x = 512, y = 1024, z = 7}";
		case 1:
			return R"({"x":512,"y":1024,"z":7})";
		case 2:
			return "512, 1024, 7";
		case 3:
			return "(512, 1024, 7)";
		case 4:
		default:
			return "Position(512, 1024, 7)";
	}
}
}

GeneralPage::GeneralPage(wxWindow* parent) : ScrollablePreferencesPage(parent) {
	auto* page_sizer = GetPageSizer();

	auto* startup_section = new PreferencesSectionPanel(
		GetScrollWindow(),
		"Startup",
		"Control which general startup behaviors are enabled when the editor launches."
	);
	show_welcome_dialog_chkbox = PreferencesLayout::AddCheckBoxRow(
		startup_section,
		"Show welcome dialog on startup",
		"Open the recent maps and client selection screen every time the editor starts.",
		g_settings.getBoolean(Config::WELCOME_DIALOG)
	);
	update_check_on_startup_chkbox = PreferencesLayout::AddCheckBoxRow(
		startup_section,
		"Check for updates on startup",
		"Look for new editor releases automatically during startup.",
		g_settings.getBoolean(Config::USE_UPDATER)
	);
	only_one_instance_chkbox = PreferencesLayout::AddCheckBoxRow(
		startup_section,
		"Open maps in the same instance",
		"Maps opened from Explorer or shell integrations are routed into the already running editor window.",
		g_settings.getBoolean(Config::ONLY_ONE_INSTANCE)
	);
	page_sizer->Add(startup_section, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(10));

	auto* safety_section = new PreferencesSectionPanel(
		GetScrollWindow(),
		"Safety",
		"These options protect project data and expose optional editor tools."
	);
	always_make_backup_chkbox = PreferencesLayout::AddCheckBoxRow(
		safety_section,
		"Always make map backups",
		"Create a backup when saving maps so you have a recovery point if something goes wrong.",
		g_settings.getBoolean(Config::ALWAYS_MAKE_BACKUP)
	);
	enable_tileset_editing_chkbox = PreferencesLayout::AddCheckBoxRow(
		safety_section,
		"Enable tileset editing",
		"Show palette editing tools for customizing tileset organization.",
		g_settings.getBoolean(Config::SHOW_TILESET_EDITOR)
	);
	page_sizer->Add(safety_section, 0, wxEXPAND | wxALL, FromDIP(10));

	auto* performance_section = new PreferencesSectionPanel(
		GetScrollWindow(),
		"Performance",
		"Tune undo capacity and background work limits to match your hardware and map size."
	);
	undo_size_spin = new wxSpinCtrl(performance_section, wxID_ANY, std::to_string(g_settings.getInteger(Config::UNDO_SIZE)), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 0x10000000);
	PreferencesLayout::AddControlRow(
		performance_section,
		"Undo queue size",
		"Maximum number of editor actions kept in history. Larger values improve safety but use more memory.",
		undo_size_spin
	);
	undo_mem_size_spin = new wxSpinCtrl(performance_section, wxID_ANY, std::to_string(g_settings.getInteger(Config::UNDO_MEM_SIZE)), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 4096);
	PreferencesLayout::AddControlRow(
		performance_section,
		"Undo memory limit (MB)",
		"Approximate memory budget for the undo queue before old entries begin to drop off.",
		undo_mem_size_spin
	);
	worker_threads_spin = new wxSpinCtrl(performance_section, wxID_ANY, std::to_string(g_settings.getInteger(Config::WORKER_THREADS)), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 64);
	PreferencesLayout::AddControlRow(
		performance_section,
		"Worker threads",
		"Number of background threads used for heavier editor tasks. Match this to your logical CPU core count when possible.",
		worker_threads_spin
	);
	replace_size_spin = new wxSpinCtrl(performance_section, wxID_ANY, std::to_string(g_settings.getInteger(Config::REPLACE_SIZE)), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 100000);
	PreferencesLayout::AddControlRow(
		performance_section,
		"Replace count limit",
		"Maximum number of items the Replace Item tool can affect in one operation.",
		replace_size_spin
	);
	page_sizer->Add(performance_section, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(10));

	auto* clipboard_section = new PreferencesSectionPanel(
		GetScrollWindow(),
		"Clipboard",
		"Choose how copied map positions should be written when you paste them into scripts, notes, or chat."
	);
	position_format_choice = new wxChoice(clipboard_section, wxID_ANY);
	for (const auto* label : kPositionChoiceLabels) {
		position_format_choice->Append(wxString::FromUTF8(label));
	}
	position_format_choice->SetSelection(ClampPositionFormatSelection(g_settings.getInteger(Config::COPY_POSITION_FORMAT)));
	PreferencesLayout::AddControlRow(
		clipboard_section,
		"Copy position format",
		"Switch between scripting-friendly and plain text formats without giving this option a full screen of space.",
		position_format_choice
	);
	position_preview_label = PreferencesLayout::AddValuePreviewRow(
		clipboard_section,
		"Preview",
		"Example output shown with representative map coordinates.",
		BuildPositionPreview(position_format_choice->GetSelection())
	);
	page_sizer->Add(clipboard_section, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(10));

	position_format_choice->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
		UpdatePositionPreview();
	});

	FinishLayout();
}

void GeneralPage::UpdatePositionPreview() {
	if (position_preview_label && position_format_choice) {
		position_preview_label->SetLabelText(BuildPositionPreview(position_format_choice->GetSelection()));
		position_preview_label->GetParent()->Layout();
	}
}

void GeneralPage::Apply() {
	g_settings.setInteger(Config::WELCOME_DIALOG, show_welcome_dialog_chkbox->GetValue());
	g_settings.setInteger(Config::ALWAYS_MAKE_BACKUP, always_make_backup_chkbox->GetValue());
	g_settings.setInteger(Config::USE_UPDATER, update_check_on_startup_chkbox->GetValue());
	g_settings.setInteger(Config::ONLY_ONE_INSTANCE, only_one_instance_chkbox->GetValue());
	g_settings.setInteger(Config::UNDO_SIZE, undo_size_spin->GetValue());
	g_settings.setInteger(Config::UNDO_MEM_SIZE, undo_mem_size_spin->GetValue());
	g_settings.setInteger(Config::WORKER_THREADS, worker_threads_spin->GetValue());
	g_settings.setInteger(Config::REPLACE_SIZE, replace_size_spin->GetValue());
	const int selected_format = position_format_choice->GetSelection() == wxNOT_FOUND ? 0 : ClampPositionFormatSelection(position_format_choice->GetSelection());
	g_settings.setInteger(Config::COPY_POSITION_FORMAT, selected_format);

	if (g_settings.getBoolean(Config::SHOW_TILESET_EDITOR) != enable_tileset_editing_chkbox->GetValue()) {
		g_gui.RebuildPalettes();
	}
	g_settings.setInteger(Config::SHOW_TILESET_EDITOR, enable_tileset_editing_chkbox->GetValue());
}
