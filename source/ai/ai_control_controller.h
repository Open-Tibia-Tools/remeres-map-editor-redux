//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_AI_CONTROL_CONTROLLER_H
#define RME_AI_CONTROL_CONTROLLER_H

#include <memory>

namespace AiControl {

	class HttpServer;

	// Interactive starts (the menu) report a failure in a dialog; unattended starts (the
	// RME_AI_CONTROL_AUTOSTART environment variable) only log it and show it in the status bar.
	enum class StartMode {
		Interactive,
		Unattended,
	};

	// Owns the local AI Control API server for the main frame: starts it on the port from
	// RME_AI_CONTROL_PORT (default 17310), stops it, and reports both in the status bar.
	class Controller {
	public:
		Controller();
		~Controller();

		Controller(const Controller&) = delete;
		Controller& operator=(const Controller&) = delete;

		void Start(StartMode mode = StartMode::Interactive);
		void Stop();
		void Toggle();
		[[nodiscard]] bool IsRunning() const;

	private:
		std::unique_ptr<HttpServer> server_;
	};

} // namespace AiControl

#endif
