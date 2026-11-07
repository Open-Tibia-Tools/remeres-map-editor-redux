//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifdef _USE_UPDATER_

	#ifndef RME_UPDATER_H_
		#define RME_UPDATER_H_

		#include <wx/event.h>
		#include <wx/webrequest.h>

class MainFrame;

class UpdateChecker : public wxEvtHandler {
public:
	UpdateChecker();
	~UpdateChecker() override;

	void connect(MainFrame* receiver);

private:
	wxWebRequest m_request;
	MainFrame* m_receiver = nullptr;
};

	#endif

#endif
