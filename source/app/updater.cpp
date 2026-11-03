//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "app/application.h"

#ifdef _USE_UPDATER_

	#include "app/updater.h"
	#include "ui/main_frame.h"

UpdateChecker::UpdateChecker() = default;

UpdateChecker::~UpdateChecker() {
	if (m_request.IsOk()) {
		m_request.Cancel();
	}
}

void UpdateChecker::connect(MainFrame* receiver) {
	m_receiver = receiver;

	wxString address = "https://www.remeresmapeditor.com/update.php";
	address << "?os="
	#ifdef __WINDOWS__
		<< "windows";
	#elif __LINUX__
		<< "linux";
	#else
		<< "unknown";
	#endif
	address << "&verid=" << __RME_VERSION_ID__;
	#ifdef __EXPERIMENTAL__
	address << "&beta";
	#endif

#if wxUSE_WEBREQUEST
	auto& session = wxWebSession::GetDefault();
	m_request = session.CreateRequest(this, address);
	if (!m_request.IsOk()) {
		return;
	}

	Bind(wxEVT_WEBREQUEST_STATE, [this](wxWebRequestEvent& evt) {
		if (evt.GetState() == wxWebRequest::State_Completed) {
			wxWebResponse resp = evt.GetResponse();
			if (resp.IsOk() && resp.GetStatus() == 200 && m_receiver) {
				m_receiver->OnUpdateDataReceived(resp.AsString().ToStdString());
			}
		}
	});

	m_request.Start();
#endif
}

#endif
