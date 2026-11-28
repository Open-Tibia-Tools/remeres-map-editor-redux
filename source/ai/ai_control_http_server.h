//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_AI_CONTROL_HTTP_SERVER_H
#define RME_AI_CONTROL_HTTP_SERVER_H

#include "ai/ai_control_contract.h"
#include "ai/ai_control_service.h"

#include <wx/event.h>
#include <wx/timer.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>

class wxSocketBase;
class wxSocketEvent;
class wxSocketServer;

namespace AiControl {

	// Minimal HTTP/1.1 server on 127.0.0.1 driven by wxSocket events, so every request runs on
	// the UI thread like a menu command. One request per connection (Connection: close).
	class HttpServer final : public wxEvtHandler {
	public:
		HttpServer();
		~HttpServer() override;

		HttpServer(const HttpServer&) = delete;
		HttpServer& operator=(const HttpServer&) = delete;

		[[nodiscard]] bool Start(uint16_t port);
		void Stop();
		[[nodiscard]] bool IsRunning() const;
		[[nodiscard]] uint16_t Port() const;

	private:
		void OnServerEvent(wxSocketEvent& event);
		void OnClientEvent(wxSocketEvent& event);
		void ReadRequest(wxSocketBase& socket);
		// Policy, routing, busy gate and dispatch; never lets an exception escape.
		[[nodiscard]] Response Respond(const HttpRequest& request);
		// Queues the response and writes it without blocking: the rest goes out on wxSOCKET_OUTPUT
		// events, so a slow reader never stalls the UI thread.
		void BeginSend(wxSocketBase& socket, const Response& response);
		void ContinueSend(wxSocketBase& socket);
		void DiscardInput(wxSocketBase& socket);
		void CloseClient(wxSocketBase& socket);
		// Closes connections that overran their phase's deadline (PhaseTimeout).
		void OnIdleTimer(wxTimerEvent& event);

		struct Connection {
			ConnectionPhase phase = ConnectionPhase::Reading;
			std::string received;
			std::string outgoing;
			size_t sent = 0;
			// Reading: last byte received. Writing and draining: when the phase began.
			std::chrono::steady_clock::time_point since;
		};

		int server_event_id_;
		int client_event_id_;
		int idle_timer_id_;
		// wxWidgets sockets are released with Destroy(), which defers deletion until no event for
		// them is pending; they cannot be owned by a smart pointer.
		wxSocketServer* server_ = nullptr;
		std::unordered_map<wxSocketBase*, Connection> buffers_;
		wxTimer idle_timer_;
		uint16_t port_ = 0;
		bool handling_request_ = false;
	};

} // namespace AiControl

#endif
