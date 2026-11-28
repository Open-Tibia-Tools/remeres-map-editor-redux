//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "ai/ai_control_controller.h"

#include "ai/ai_control_contract.h"
#include "ai/ai_control_http_server.h"
#include "ui/dialog_util.h"
#include "ui/gui.h"

#include <wx/utils.h>

#include <spdlog/spdlog.h>

#include <format>
#include <optional>

namespace AiControl {

	namespace {

		constexpr std::string_view PortVariable = "RME_AI_CONTROL_PORT";

		// The port from RME_AI_CONTROL_PORT, the default when it is unset, nullopt when it is invalid.
		std::optional<uint16_t> ConfiguredPort() {
			wxString text;
			if (!wxGetEnv(wxString(PortVariable.data(), PortVariable.size()), &text)) {
				return DefaultPort;
			}
			return ParsePort(text.utf8_string());
		}

		void ReportStartFailure(StartMode mode, const std::string& message) {
			if (mode == StartMode::Interactive) {
				DialogUtil::PopupDialog("AI Control API", wxstr(message), wxOK | wxICON_ERROR);
				return;
			}
			spdlog::error("AI Control API: {}", message);
			g_gui.SetStatusText(wxstr(std::format("AI Control API not started: {}", message)));
		}

	} // namespace

	Controller::Controller() :
		server_(std::make_unique<HttpServer>()) {
	}

	Controller::~Controller() = default;

	void Controller::Start(StartMode mode) {
		if (server_->IsRunning()) {
			return;
		}
		const std::optional<uint16_t> port = ConfiguredPort();
		if (!port) {
			ReportStartFailure(mode, std::format("{} must be a port from 1024 to 65535.", PortVariable));
			return;
		}
		if (!server_->Start(*port)) {
			ReportStartFailure(mode, std::format("Could not listen on {}:{}. Is the port in use?", BindAddress, *port));
			return;
		}
		g_gui.SetStatusText(wxstr(std::format("AI Control API listening on http://{}:{}", BindAddress, *port)));
	}

	void Controller::Stop() {
		if (!server_->IsRunning()) {
			return;
		}
		server_->Stop();
		g_gui.SetStatusText("AI Control API stopped");
	}

	void Controller::Toggle() {
		if (server_->IsRunning()) {
			Stop();
		} else {
			Start();
		}
	}

	bool Controller::IsRunning() const {
		return server_->IsRunning();
	}

} // namespace AiControl
