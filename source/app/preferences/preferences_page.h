#ifndef RME_PREFERENCES_PAGE_H
#define RME_PREFERENCES_PAGE_H

#include <wx/panel.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/preferences.h>

/**
 * Adapter allowing RME PreferencesPage implementations to plug directly into
 * wxWidgets modern wxPreferencesEditor architecture.
 */
class PreferencesPageAdapter : public wxPreferencesPage {
public:
	PreferencesPageAdapter(const wxString& name, const wxBitmapBundle& icon = wxBitmapBundle())
		: m_name(name), m_icon(icon) {}

	wxString GetName() const override { return m_name; }
	wxBitmapBundle GetIcon() const override { return m_icon; }

private:
	wxString m_name;
	wxBitmapBundle m_icon;
};


#include "ui/theme.h"

class PreferencesPage : public wxPanel {
public:
	PreferencesPage(wxWindow* parent) : wxPanel(parent, wxID_ANY) {
		SetBackgroundColour(Theme::Get(Theme::Role::Surface));
	}
	virtual ~PreferencesPage() = default;

	virtual void Apply() = 0;
	virtual void RestoreDefaults() { };
};

class ScrollablePreferencesPage : public PreferencesPage {
public:
	explicit ScrollablePreferencesPage(wxWindow* parent) : PreferencesPage(parent) {
		auto* root_sizer = new wxBoxSizer(wxVERTICAL);

		m_scrolled_window = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL);
		m_scrolled_window->SetBackgroundColour(GetBackgroundColour());
		m_scrolled_window->SetScrollRate(0, this->FromDIP(18));
		m_scrolled_window->SetMinSize(wxSize(-1, this->FromDIP(320)));

		m_page_sizer = new wxBoxSizer(wxVERTICAL);
		m_scrolled_window->SetSizer(m_page_sizer);

		root_sizer->Add(m_scrolled_window, wxSizerFlags(1).Expand());
		SetSizer(root_sizer);
	}

protected:
	wxScrolledWindow* GetScrollWindow() const {
		return m_scrolled_window;
	}

	wxBoxSizer* GetPageSizer() const {
		return m_page_sizer;
	}

	void FinishLayout() {
		m_scrolled_window->FitInside();
		Layout();
	}

private:
	wxScrolledWindow* m_scrolled_window = nullptr;
	wxBoxSizer* m_page_sizer = nullptr;
};

#endif


