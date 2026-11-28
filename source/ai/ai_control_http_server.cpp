//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "ai/ai_control_http_server.h"

#include "ui/gui.h"

#include <wx/modalhook.h>
#include <wx/socket.h>
#include <wx/window.h>

#include <spdlog/spdlog.h>

#ifdef _WIN32
	#include <winsock2.h>
#else
	#include <sys/socket.h>
#endif

#include <algorithm>
#include <array>
#include <exception>
#include <string_view>
#include <vector>

namespace AiControl {

	namespace {

		constexpr size_t MaxRequestBytes = MaxHeaderBytes + MaxBodyBytes;
		constexpr size_t ReadChunkBytes = 16 * 1024;
		constexpr size_t MaxOpenConnections = 16;
		constexpr int IdleCheckIntervalMs = 1000;

		// Marks a request as running for the busy gate and restores the previous state on every
		// exit, so a status request answered during a nested event loop leaves the flag intact.
		class InFlightRequest {
		public:
			explicit InFlightRequest(bool& flag) :
				flag_(flag),
				previous_(flag) {
				flag_ = true;
			}
			~InFlightRequest() {
				flag_ = previous_;
			}
			InFlightRequest(const InFlightRequest&) = delete;
			InFlightRequest& operator=(const InFlightRequest&) = delete;

		private:
			bool& flag_;
			bool previous_;
		};

		Response ErrorResponse(const ApiError& error) {
			return { error.status, ErrorBody(error) };
		}

		Response InternalError(std::string_view what) {
			spdlog::error("AI Control API: request failed: {}", what);
			return ErrorResponse({ 500, "internal_error", std::string(what) });
		}

		// Sends FIN but keeps the socket open for reading. wxSocketBase::ShutdownOutput() closes the
		// whole socket, which would reset a client that is still sending and lose the response.
		void HalfCloseOutput(wxSocketBase& socket) {
#ifdef _WIN32
			::shutdown(static_cast<SOCKET>(socket.GetSocket()), SD_SEND);
#else
			::shutdown(static_cast<int>(socket.GetSocket()), SHUT_WR);
#endif
		}

	} // namespace

	HttpServer::HttpServer() :
		server_event_id_(wxWindow::NewControlId()),
		client_event_id_(wxWindow::NewControlId()),
		idle_timer_id_(wxWindow::NewControlId()),
		idle_timer_(this, idle_timer_id_) {
		Bind(wxEVT_SOCKET, &HttpServer::OnServerEvent, this, server_event_id_);
		Bind(wxEVT_SOCKET, &HttpServer::OnClientEvent, this, client_event_id_);
		Bind(wxEVT_TIMER, &HttpServer::OnIdleTimer, this, idle_timer_id_);
	}

	HttpServer::~HttpServer() {
		Stop();
		Unbind(wxEVT_SOCKET, &HttpServer::OnServerEvent, this, server_event_id_);
		Unbind(wxEVT_SOCKET, &HttpServer::OnClientEvent, this, client_event_id_);
		Unbind(wxEVT_TIMER, &HttpServer::OnIdleTimer, this, idle_timer_id_);
		wxWindow::UnreserveControlId(server_event_id_);
		wxWindow::UnreserveControlId(client_event_id_);
		wxWindow::UnreserveControlId(idle_timer_id_);
	}

	bool HttpServer::Start(uint16_t port) {
		if (IsRunning()) {
			return port_ == port;
		}
		wxIPV4address address;
		address.Hostname(wxString(BindAddress.data(), BindAddress.size()));
		address.Service(port);
		auto* server = new wxSocketServer(address, wxSOCKET_NOWAIT);
		if (!server->IsOk()) {
			server->Destroy();
			return false;
		}
		server->SetEventHandler(*this, server_event_id_);
		server->SetNotify(wxSOCKET_CONNECTION_FLAG);
		server->Notify(true);
		server_ = server;
		port_ = port;
		idle_timer_.Start(IdleCheckIntervalMs);
		return true;
	}

	void HttpServer::Stop() {
		idle_timer_.Stop();
		for (const auto& [socket, connection] : buffers_) {
			socket->Notify(false);
			socket->Destroy();
		}
		buffers_.clear();
		if (server_) {
			server_->Notify(false);
			server_->Destroy();
			server_ = nullptr;
		}
		port_ = 0;
	}

	bool HttpServer::IsRunning() const {
		return server_ != nullptr && server_->IsOk();
	}

	uint16_t HttpServer::Port() const {
		return port_;
	}

	void HttpServer::OnServerEvent(wxSocketEvent& event) {
		if (!server_ || event.GetSocketEvent() != wxSOCKET_CONNECTION) {
			return;
		}
		wxSocketBase* socket = server_->Accept(false);
		if (!socket) {
			return;
		}
		const auto open_connections = std::ranges::count_if(buffers_, [](const auto& entry) {
			return CountsTowardConnectionLimit(entry.second.phase);
		});
		if (static_cast<size_t>(open_connections) >= MaxOpenConnections) {
			socket->Destroy();
			return;
		}
		// Never wait inside Read or Write: waiting would block or yield the UI thread.
		socket->SetFlags(wxSOCKET_NOWAIT);
		socket->SetEventHandler(*this, client_event_id_);
		socket->SetNotify(wxSOCKET_INPUT_FLAG | wxSOCKET_LOST_FLAG);
		socket->Notify(true);
		buffers_.emplace(socket, Connection { .since = std::chrono::steady_clock::now() });
	}

	void HttpServer::OnClientEvent(wxSocketEvent& event) {
		wxSocketBase* socket = event.GetSocket();
		if (!socket || !buffers_.contains(socket)) {
			return;
		}
		try {
			const ConnectionPhase phase = buffers_.at(socket).phase;
			switch (event.GetSocketEvent()) {
				case wxSOCKET_INPUT:
					if (phase == ConnectionPhase::Reading) {
						ReadRequest(*socket);
					} else {
						DiscardInput(*socket);
					}
					break;
				case wxSOCKET_OUTPUT:
					if (phase == ConnectionPhase::Writing) {
						ContinueSend(*socket);
					}
					break;
				case wxSOCKET_LOST:
					CloseClient(*socket);
					break;
				default:
					break;
			}
		} catch (const std::exception& error) {
			spdlog::error("AI Control API: connection dropped: {}", error.what());
			if (buffers_.contains(socket)) {
				CloseClient(*socket);
			}
		}
	}

	void HttpServer::ReadRequest(wxSocketBase& socket) {
		const auto found = buffers_.find(&socket);
		if (found == buffers_.end()) {
			return;
		}
		std::string& received = found->second.received;
		found->second.since = std::chrono::steady_clock::now();
		std::array<char, ReadChunkBytes> chunk {};
		while (true) {
			socket.Read(chunk.data(), static_cast<wxUint32>(chunk.size()));
			const size_t count = socket.LastCount();
			if (count == 0) {
				break;
			}
			received.append(chunk.data(), count);
			if (received.size() > MaxRequestBytes + 4) {
				BeginSend(socket, ErrorResponse({ 413, "request_too_large", "The HTTP request exceeds the size limit." }));
				return;
			}
			if (count < chunk.size()) {
				break;
			}
		}

		const HttpParseResult parsed = ParseHttpRequest(received);
		if (parsed.state == HttpParseState::Incomplete) {
			return;
		}
		// The request is complete: no further event of this socket may run while it is handled, and
		// Stop() during a nested event loop must not destroy it under the running handler.
		socket.Notify(false);
		buffers_.erase(found);
		if (parsed.state == HttpParseState::Rejected) {
			BeginSend(socket, ErrorResponse(*parsed.rejection));
			return;
		}
		const Response response = Respond(parsed.request);
		if (!IsRunning()) {
			// Stopped while the request ran: nobody tracks this socket any more.
			socket.Destroy();
			return;
		}
		BeginSend(socket, response);
	}

	Response HttpServer::Respond(const HttpRequest& request) {
		try {
			if (auto error = CheckRequestPolicy(request, port_)) {
				return ErrorResponse(*error);
			}
			const RouteMatch match = LookupRoute(request.method, request.path);
			if (!match.route) {
				return ErrorResponse(*match.error);
			}
			const EditorActivity activity {
				.loading = g_gui.IsLoading(),
				.renderingDisabled = !g_gui.IsRenderingEnabled(),
				.modalDialogOpen = wxModalDialogHook::GetOpenCount() > 0,
				.requestInProgress = handling_request_,
				.userInteracting = IsUserInteracting(),
			};
			if (auto error = CheckBusyGate(*match.route, activity)) {
				return ErrorResponse(*error);
			}
			const JsonBodyResult body = request.method == HttpMethod::Post ? ParseJsonBody(request.body) : JsonBodyResult {};
			if (body.error) {
				return ErrorResponse(*body.error);
			}
			const InFlightRequest in_flight(handling_request_);
			return Dispatch(*match.route, body.body);
		} catch (const std::exception& error) {
			return InternalError(error.what());
		} catch (...) {
			return InternalError("unknown exception");
		}
	}

	void HttpServer::BeginSend(wxSocketBase& socket, const Response& response) {
		Connection& connection = buffers_[&socket];
		connection.phase = ConnectionPhase::Writing;
		connection.received.clear();
		connection.outgoing = BuildHttpResponse(response.status, SerializeJson(response.body));
		connection.sent = 0;
		connection.since = std::chrono::steady_clock::now();
		socket.SetFlags(wxSOCKET_NOWAIT);
		socket.SetNotify(wxSOCKET_INPUT_FLAG | wxSOCKET_OUTPUT_FLAG | wxSOCKET_LOST_FLAG);
		socket.Notify(true);
		ContinueSend(socket);
	}

	void HttpServer::ContinueSend(wxSocketBase& socket) {
		const auto found = buffers_.find(&socket);
		if (found == buffers_.end()) {
			return;
		}
		Connection& connection = found->second;
		while (connection.sent < connection.outgoing.size()) {
			const size_t remaining = connection.outgoing.size() - connection.sent;
			socket.Write(connection.outgoing.data() + connection.sent, static_cast<wxUint32>(remaining));
			const size_t count = socket.LastCount();
			if (count == 0) {
				if (socket.Error() && socket.LastError() != wxSOCKET_WOULDBLOCK) {
					CloseClient(socket);
				}
				// Otherwise the socket buffer is full; wxSOCKET_OUTPUT resumes the write.
				return;
			}
			connection.sent += count;
		}
		// Everything is queued. Send FIN, then read and drop whatever the client still sends until it
		// closes: closing with unread input would reset the connection and could lose the response.
		connection.outgoing.clear();
		connection.phase = ConnectionPhase::Draining;
		connection.since = std::chrono::steady_clock::now();
		HalfCloseOutput(socket);
		DiscardInput(socket);
	}

	void HttpServer::DiscardInput(wxSocketBase& socket) {
		std::array<char, ReadChunkBytes> chunk {};
		do {
			socket.Read(chunk.data(), static_cast<wxUint32>(chunk.size()));
		} while (socket.LastCount() == chunk.size());
	}

	void HttpServer::CloseClient(wxSocketBase& socket) {
		buffers_.erase(&socket);
		socket.Notify(false);
		socket.Destroy();
	}

	void HttpServer::OnIdleTimer(wxTimerEvent&) {
		const auto now = std::chrono::steady_clock::now();
		std::vector<wxSocketBase*> expired;
		for (const auto& [socket, connection] : buffers_) {
			if (IsConnectionExpired(connection.phase, now - connection.since)) {
				expired.push_back(socket);
			}
		}
		for (wxSocketBase* socket : expired) {
			CloseClient(*socket);
		}
	}

} // namespace AiControl
