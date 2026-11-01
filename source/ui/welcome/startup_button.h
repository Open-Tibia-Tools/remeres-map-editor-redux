#ifndef RME_STARTUP_BUTTON_H_
#define RME_STARTUP_BUTTON_H_

#include <wx/dcbuffer.h>
#include <wx/wx.h>
#include <wx/bmpbndl.h>

enum class StartupButtonVariant {
	Primary,
	Secondary,
};

class StartupButton : public wxControl {
public:
	StartupButton(wxWindow* parent, wxWindowID id, const wxString& label, StartupButtonVariant variant, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize);

	void SetBitmap(const wxBitmapBundle& bundle);
	void SetBitmap(const wxBitmap& bitmap);
	void SetVariant(StartupButtonVariant variant);

	wxSize DoGetBestClientSize() const override;

private:
	void OnPaint(wxPaintEvent& event);
	void OnMouse(wxMouseEvent& event);
	void OnMouseCaptureLost(wxMouseCaptureLostEvent& event);

	wxBitmapBundle m_bundle;
	wxBitmap m_icon;
	StartupButtonVariant m_variant;
	bool m_hovered = false;
	bool m_pressed = false;
};

#endif
