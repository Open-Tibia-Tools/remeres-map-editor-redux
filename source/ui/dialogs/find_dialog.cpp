#include "ui/dialogs/find_dialog.h"

#include "brushes/brush.h"
#include "item_definitions/core/item_definition_store.h"
#include "ui/gui.h"
#include "ui/theme.h"
#include "brushes/raw/raw_brush.h"
#include "rendering/indicators/indicator_drawing_utils.h"
#include "util/image_manager.h"
#include <glad/glad.h>
#include <nanovg.h>
#include <algorithm>

static bool caseInsensitiveContains(std::string_view haystack, std::string_view needle_lower) {
	auto it = std::search(
		haystack.begin(), haystack.end(),
		needle_lower.begin(), needle_lower.end(),
		[](char ch1, char ch2) {
			return std::tolower(static_cast<unsigned char>(ch1)) == std::tolower(static_cast<unsigned char>(ch2));
		}
	);
	return it != haystack.end();
}

// ============================================================================
// Numkey forwarding text control

void KeyForwardingTextCtrl::OnKeyDown(wxKeyEvent& event) {
	if (event.GetKeyCode() == WXK_UP || event.GetKeyCode() == WXK_DOWN || event.GetKeyCode() == WXK_PAGEDOWN || event.GetKeyCode() == WXK_PAGEUP) {
		GetParent()->GetEventHandler()->AddPendingEvent(event);
	} else {
		event.Skip();
	}
}

// ============================================================================
// Find Item Dialog (Jump to item)

FindDialog::FindDialog(wxWindow* parent, wxString title) :
	wxDialog(g_gui.root, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, wxRESIZE_BORDER | wxCAPTION | wxCLOSE_BOX),
	idle_input_timer(this),
	result_brush(nullptr),
	result_id(0) {
	wxSizer* sizer = newd wxBoxSizer(wxVERTICAL);

	search_field = newd KeyForwardingTextCtrl(this, JUMP_DIALOG_TEXT, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
	search_field->SetHint("Type to search...");
	search_field->SetToolTip("Type at least 2 characters to search for brushes or items.");
	search_field->SetFocus();
	sizer->Add(search_field, 0, wxEXPAND);

	item_list = newd FindDialogListBox(this, JUMP_DIALOG_LIST);
	item_list->SetMinSize(FROM_DIP(item_list, wxSize(470, 400)));
	item_list->SetToolTip("Double click to select.");
	sizer->Add(item_list, wxSizerFlags(1).Expand().Border());

	wxSizer* stdsizer = newd wxBoxSizer(wxHORIZONTAL);
	wxButton* okBtn = newd wxButton(this, wxID_OK, "OK");
	okBtn->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_CHECK, wxSize(16, 16)));
	okBtn->SetToolTip("Jump to selected item/brush");
	stdsizer->Add(okBtn, wxSizerFlags(1).Center());
	wxButton* cancelBtn = newd wxButton(this, wxID_CANCEL, "Cancel");
	cancelBtn->SetBitmap(IMAGE_MANAGER.GetBitmap(ICON_XMARK, wxSize(16, 16)));
	cancelBtn->SetToolTip("Close this window");
	stdsizer->Add(cancelBtn, wxSizerFlags(1).Center());
	sizer->Add(stdsizer, wxSizerFlags(0).Center().Border());

	SetSizerAndFit(sizer);
	Centre(wxBOTH);

	Bind(wxEVT_TIMER, &FindDialog::OnTextIdle, this, wxID_ANY);
	Bind(wxEVT_TEXT, &FindDialog::OnTextChange, this, JUMP_DIALOG_TEXT);
	Bind(wxEVT_KEY_DOWN, &FindDialog::OnKeyDown, this);
	Bind(wxEVT_TEXT_ENTER, &FindDialog::OnClickOK, this, JUMP_DIALOG_TEXT);
	Bind(wxEVT_LISTBOX_DCLICK, &FindDialog::OnClickList, this, JUMP_DIALOG_LIST);
	Bind(wxEVT_BUTTON, &FindDialog::OnClickOK, this, wxID_OK);
	Bind(wxEVT_BUTTON, &FindDialog::OnClickCancel, this, wxID_CANCEL);

	// We can't call it here since it calls an abstract function, call in child constructors instead.
	// RefreshContents();

	wxIcon icon;
	icon.CopyFromBitmap(IMAGE_MANAGER.GetBitmap(ICON_SEARCH, wxSize(32, 32)));
	SetIcon(icon);
}

FindDialog::~FindDialog() = default;

void FindDialog::OnKeyDown(wxKeyEvent& event) {
	int w, h;
	item_list->GetSize(&w, &h);
	size_t amount = 1;

	switch (event.GetKeyCode()) {
		case WXK_PAGEUP:
			amount = h / 32 + 1;
			[[fallthrough]];
		case WXK_UP: {
			if (item_list->GetItemCount() > 0) {
				ssize_t n = item_list->GetSelection();
				if (n == wxNOT_FOUND) {
					n = 0;
				} else if (static_cast<size_t>(n) >= amount) {
					n -= amount;
				} else {
					n = 0;
				}
				item_list->SetSelection(n);
			}
			break;
		}

		case WXK_PAGEDOWN:
			amount = h / 32 + 1;
			[[fallthrough]];
		case WXK_DOWN: {
			if (item_list->GetItemCount() > 0) {
				ssize_t n = item_list->GetSelection();
				size_t itemcount = item_list->GetItemCount();
				if (n == wxNOT_FOUND) {
					n = 0;
				} else if (static_cast<uint32_t>(n) < itemcount - amount && itemcount - amount < itemcount) {
					n += amount;
				} else {
					n = item_list->GetItemCount() - 1;
				}

				item_list->SetSelection(n);
			}
			break;
		}
		default:
			event.Skip();
			break;
	}
}

void FindDialog::OnTextIdle(wxTimerEvent& WXUNUSED(event)) {
	RefreshContents();
}

void FindDialog::OnTextChange(wxCommandEvent& WXUNUSED(event)) {
	idle_input_timer.Start(800, true);
}

void FindDialog::OnClickList(wxCommandEvent& event) {
	OnClickListInternal(event);
}

void FindDialog::OnClickOK(wxCommandEvent& WXUNUSED(event)) {
	// This is to get virtual callback
	OnClickOKInternal();
}

void FindDialog::OnClickCancel(wxCommandEvent& WXUNUSED(event)) {
	EndModal(0);
}

void FindDialog::RefreshContents() {
	// This is to get virtual callback
	RefreshContentsInternal();
}

// ============================================================================
// Find Brush Dialog (Jump to brush)

FindBrushDialog::FindBrushDialog(wxWindow* parent, wxString title) :
	FindDialog(parent, title) {
	RefreshContents();
}

FindBrushDialog::~FindBrushDialog() = default;

void FindBrushDialog::OnClickListInternal(wxCommandEvent& event) {
	Brush* brush = item_list->GetSelectedBrush();
	if (brush) {
		result_brush = brush;
		EndModal(1);
	}
}

void FindBrushDialog::OnClickOKInternal() {
	// This is kind of stupid as it would fail unless the "Please enter a search string" wasn't there
	if (item_list->GetItemCount() > 0) {
		if (item_list->GetSelection() == wxNOT_FOUND) {
			item_list->SetSelection(0);
		}
		Brush* brush = item_list->GetSelectedBrush();
		if (!brush) {
			// It's either "Please enter a search string" or "No matches"
			// Perhaps we can refresh now?
			std::string search_string = as_lower_str(nstr(search_field->GetValue()));
			bool do_search = (search_string.size() >= 2);

			if (do_search) {
				const BrushMap& map = g_brushes.getMap();
				for (const auto& [name, brush_ptr] : map) {
					const Brush* brush = brush_ptr.get();
					if (!caseInsensitiveContains(brush->getName(), search_string)) {
						continue;
					}

					// Don't match RAWs now.
					if (brush->is<RAWBrush>()) {
						continue;
					}

					// Found one!
					result_brush = brush;
					break;
				}

				// Did we not find a matching brush?
				if (!result_brush) {
					// Then let's search the RAWs
					for (ServerItemId id : g_item_definitions.allIds()) {
						RAWBrush* raw_brush = g_item_definitions.editorData(id).raw_brush;
						if (!raw_brush) {
							continue;
						}

						if (!caseInsensitiveContains(raw_brush->getName(), search_string)) {
							continue;
						}

						// Found one!
						result_brush = raw_brush;
						break;
					}
				}
				// Done!
			}
		} else {
			result_brush = brush;
		}
	}
	EndModal(1);
}

void FindBrushDialog::RefreshContentsInternal() {
	item_list->Clear();

	std::string search_string = as_lower_str(nstr(search_field->GetValue()));
	bool do_search = (search_string.size() >= 2);

	if (do_search) {

		bool found_search_results = false;

		const BrushMap& brushes_map = g_brushes.getMap();

		for (const auto& [name, brush_ptr] : brushes_map) {
			const Brush* brush = brush_ptr.get();

			if (!caseInsensitiveContains(brush->getName(), search_string)) {
				continue;
			}

			if (brush->is<RAWBrush>()) {
				continue;
			}

			found_search_results = true;
			item_list->AddBrush(const_cast<Brush*>(brush));
		}

		for (ServerItemId id : g_item_definitions.allIds()) {
			RAWBrush* raw_brush = g_item_definitions.editorData(id).raw_brush;
			if (!raw_brush) {
				continue;
			}

			if (!caseInsensitiveContains(raw_brush->getName(), search_string)) {
				continue;
			}

			found_search_results = true;
			item_list->AddBrush(raw_brush);
		}

		if (found_search_results) {
			item_list->SetSelection(0);
		} else {
			item_list->SetNoMatches();
		}
	}
	item_list->CommitUpdates();
}

// ============================================================================
// Listbox in find item / brush stuff

FindDialogListBox::FindDialogListBox(wxWindow* parent, wxWindowID id) :
	NanoVGListBox(parent, id, wxLB_SINGLE),
	cleared(false),
	no_matches(false) {
	Clear();
}

FindDialogListBox::~FindDialogListBox() {
	////
}

void FindDialogListBox::Clear() {
	cleared = true;
	no_matches = false;
	brushlist.clear();
	SetItemCount(1);
}

void FindDialogListBox::SetNoMatches() {
	cleared = false;
	no_matches = true;
	brushlist.clear();
	SetItemCount(1);
}

void FindDialogListBox::AddBrush(Brush* brush) {
	cleared = false;
	no_matches = false;
	brushlist.push_back(brush);
}

void FindDialogListBox::CommitUpdates() {
	if (cleared || no_matches) {
		SetItemCount(1);
	} else {
		SetItemCount(brushlist.size());
	}
	Refresh();
}

Brush* FindDialogListBox::GetSelectedBrush() {
	int n = GetSelection();
	if (n == -1 || no_matches || cleared) {
		return nullptr;
	}
	return brushlist[n];
}

void FindDialogListBox::OnDrawItem(NVGcontext* vg, const wxRect& rect, size_t n) {
	wxColour textColour = Theme::Get(Theme::Role::Text);
	if (no_matches) {
		nvgFontSize(vg, 12.0f);
		nvgFontFace(vg, "sans");
		nvgFillColor(vg, nvgRGBA(textColour.Red(), textColour.Green(), textColour.Blue(), 255));
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgText(vg, rect.x + 40, rect.y + rect.height / 2.0f, "No matches for your search.", nullptr);
	} else if (cleared) {
		nvgFontSize(vg, 12.0f);
		nvgFontFace(vg, "sans");
		nvgFillColor(vg, nvgRGBA(textColour.Red(), textColour.Green(), textColour.Blue(), 255));
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgText(vg, rect.x + 40, rect.y + rect.height / 2.0f, "Please enter your search string.", nullptr);
	} else {
		ASSERT(n < brushlist.size());
		const Brush* brush = brushlist[n];
		auto tech = rme::rendering::TechnicalItemRegistry::GetBrushIndicatorType(brush);
		if (tech == rme::rendering::TileIndicatorType::None && brush && brush->is<RAWBrush>()) {
			const auto* raw = brush->as<RAWBrush>();
			const uint16_t sid = raw->getItemID();
			const auto def = g_item_definitions.get(sid);
			const uint16_t cid = def ? def.clientId() : 0;
			tech = rme::rendering::TechnicalItemRegistry::Classify(sid, cid);
		}
		if (tech != rme::rendering::TileIndicatorType::None) {
			const int icon_size = rect.height;
			rme::rendering::DrawNanoVGIndicatorBadge(
				vg,
				tech,
				static_cast<float>(rect.x),
				static_cast<float>(rect.y),
				static_cast<float>(icon_size)
			);
		} else {
			Sprite* spr = g_gui.gfx.getSprite(brush->getLookID());
			if (spr) {
				int tex = GetOrCreateSpriteTexture(vg, spr);
				if (tex > 0) {
					int icon_size = rect.height;
					NVGpaint imgPaint = nvgImagePattern(vg, rect.x, rect.y, icon_size, icon_size, 0, tex, 1.0f);
					nvgBeginPath(vg);
					nvgRect(vg, rect.x, rect.y, icon_size, icon_size);
					nvgFillPaint(vg, imgPaint);
					nvgFill(vg);
				}
			}
		}

		if (IsSelected(n)) {
			textColour = Theme::Get(Theme::Role::TextOnAccent);
			nvgFillColor(vg, nvgRGBA(textColour.Red(), textColour.Green(), textColour.Blue(), 255));
		} else {
			textColour = Theme::Get(Theme::Role::Text);
			nvgFillColor(vg, nvgRGBA(textColour.Red(), textColour.Green(), textColour.Blue(), 255));
		}

		nvgFontSize(vg, 12.0f);
		nvgFontFace(vg, "sans");
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		wxString name = wxstr(brushlist[n]->getName());
		nvgText(vg, rect.x + rect.height + 8, rect.y + rect.height / 2.0f, name.ToUTF8().data(), nullptr);
	}
}

int FindDialogListBox::OnMeasureItem(size_t n) const {
	return FromDIP(32);
}
