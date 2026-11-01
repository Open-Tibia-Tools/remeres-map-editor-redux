//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "app/application.h"

#ifdef _USE_UPDATER_

	#include <wx/url.h>

	#include "util/json.h"

	#include "app/updater.h"
	#include "ui/main_frame.h"
	#include <thread>
	#include <atomic>
	#include <memory>



UpdateChecker::UpdateChecker() :
	m_alive(std::make_shared<std::atomic<bool>>(true)) {
	////
}

UpdateChecker::~UpdateChecker() {
	if (m_alive) {
		*m_alive = false;
	}
}

void UpdateChecker::connect(MainFrame* receiver) {
	wxString address = "http://www.remeresmapeditor.com/update.php";
	address << "?os="
			<<
	#ifdef __WINDOWS__
		"windows";
	#elif __LINUX__
		"linux";
	#else
		"unknown";
	#endif
	address << "&verid=" << __RME_VERSION_ID__;
	#ifdef __EXPERIMENTAL__
	address << "&beta";
	#endif
	wxURL* url = new wxURL(address);

	m_thread = std::jthread([receiver, url, alive = m_alive](std::stop_token stop_token) {
		wxInputStream* input = url->GetInputStream();
		if (!input) {
			delete url;
			return;
		}

		std::string data;
		while (!input->Eof()) {
			if (stop_token.stop_requested()) {
				delete input;
				delete url;
				return;
			}
			data += input->GetC();
		}

		delete input;
		delete url;

		// Direct safe thread dispatch via wxTheApp->CallAfter with value semantics
		wxTheApp->CallAfter([receiver, data = std::move(data), alive]() {
			if (*alive && receiver) {
				receiver->OnUpdateDataReceived(data);
			}
		});
	});
}

#endif
