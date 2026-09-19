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
#include "lua_api.h"

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <future>
#include <mutex>
#include <atomic>
#include <queue>
#include <condition_variable>
#include <string_view>
#include <functional>
#include <unordered_map>
#include <boost/asio.hpp>

namespace LuaAPI {

	// Constants for resource safety
	static constexpr size_t MAX_STREAM_SESSIONS = 16;
	static constexpr size_t MAX_STREAM_BUFFER_SIZE = 1024 * 1024; // 1MB
	static std::atomic<size_t> g_activeStreamCount { 0 };

	struct UrlParts {
		std::string scheme = "http";
		std::string authority;
		std::string path;
		std::string portSuffix;
	};

	static UrlParts parseUrlParts(const std::string& url) {
		UrlParts parts;

		const size_t schemePos = url.find("://");
		const size_t hostStart = (schemePos == std::string::npos) ? 0 : schemePos + 3;
		if (schemePos != std::string::npos) {
			parts.scheme = url.substr(0, schemePos);
		}

		const size_t pathStart = url.find_first_of("/?#", hostStart);
		parts.authority = (pathStart == std::string::npos) ? url.substr(hostStart) : url.substr(hostStart, pathStart - hostStart);
		parts.path = (pathStart == std::string::npos) ? std::string() : url.substr(pathStart);

		if (!parts.authority.empty() && parts.authority.front() == '[') {
			const size_t bracketEnd = parts.authority.find(']');
			if (bracketEnd != std::string::npos && bracketEnd + 1 < parts.authority.size() && parts.authority[bracketEnd + 1] == ':') {
				parts.portSuffix = parts.authority.substr(bracketEnd + 1);
			}
		} else {
			const size_t colonPos = parts.authority.rfind(':');
			if (colonPos != std::string::npos && parts.authority.find(':') == colonPos) {
				parts.portSuffix = parts.authority.substr(colonPos);
			}
		}

		return parts;
	}

	static std::string formatPinnedIp(const std::string& safeIp) {
		if (safeIp.find(':') != std::string::npos && (safeIp.empty() || safeIp.front() != '[')) {
			return "[" + safeIp + "]";
		}
		return safeIp;
	}

	static cpr::Url buildPinnedUrl(const std::string& safeIp, const UrlParts& parts) {
		return cpr::Url { parts.scheme + "://" + formatPinnedIp(safeIp) + parts.portSuffix + parts.path };
	}

	// StreamSession class for managing streaming HTTP requests
	class StreamSession {
	public:
		struct Snapshot {
			std::string data;
			bool finished;
			bool hasError;
			std::string error;
			int status;
			cpr::Header headers;
		};

		StreamSession() :
			finished_(false), hasError_(false), closed_(false), statusCode_(0), totalSize_(0) { }

		bool appendChunk(const std::string& chunk) {
			std::lock_guard<std::mutex> lock(mutex_);
			if (closed_ || totalSize_ + chunk.size() > MAX_STREAM_BUFFER_SIZE) {
				return false;
			}
			chunks_.push(chunk);
			totalSize_ += chunk.size();
			cv_.notify_one();
			return true;
		}

		Snapshot getSnapshot() {
			std::lock_guard<std::mutex> lock(mutex_);
			Snapshot s;
			while (!chunks_.empty()) {
				s.data += chunks_.front();
				chunks_.pop();
			}
			totalSize_ = 0;
			s.finished = finished_;
			s.hasError = hasError_;
			s.error = errorMessage_;
			s.status = statusCode_;
			s.headers = responseHeaders_;
			return s;
		}

		bool hasChunks() {
			std::lock_guard<std::mutex> lock(mutex_);
			return !chunks_.empty();
		}

		void setFinished() {
			std::lock_guard<std::mutex> lock(mutex_);
			finished_ = true;
			cv_.notify_all();
		}

		bool isFinished() {
			std::lock_guard<std::mutex> lock(mutex_);
			return finished_;
		}

		void setError(const std::string& error) {
			std::lock_guard<std::mutex> lock(mutex_);
			hasError_ = true;
			errorMessage_ = error;
			finished_ = true;
			cv_.notify_all();
		}

		bool hasError() {
			std::lock_guard<std::mutex> lock(mutex_);
			return hasError_;
		}

		std::string getError() {
			std::lock_guard<std::mutex> lock(mutex_);
			return errorMessage_;
		}

		Snapshot getStatus() {
			std::lock_guard<std::mutex> lock(mutex_);
			Snapshot s;
			s.finished = finished_;
			s.hasError = hasError_;
			s.error = errorMessage_;
			s.status = statusCode_;
			s.headers = responseHeaders_;
			return s;
		}

		void setStatusCode(int code) {
			statusCode_ = code;
		}

		int getStatusCode() {
			return statusCode_;
		}

		void setHeaders(const cpr::Header& headers) {
			std::lock_guard<std::mutex> lock(mutex_);
			responseHeaders_ = headers;
		}

		cpr::Header getHeaders() {
			std::lock_guard<std::mutex> lock(mutex_);
			return responseHeaders_;
		}

		void close() {
			std::lock_guard<std::mutex> lock(mutex_);
			closed_ = true;
			finished_ = true;
			cv_.notify_all();
		}

	private:
		std::queue<std::string> chunks_;
		std::mutex mutex_;
		std::condition_variable cv_;
		std::atomic<bool> finished_;
		std::atomic<bool> hasError_;
		std::atomic<bool> closed_;
		std::string errorMessage_;
		std::atomic<int> statusCode_;
		cpr::Header responseHeaders_;
		size_t totalSize_;
	};

	// Global map to store active stream sessions
	static std::unordered_map<uint64_t, std::shared_ptr<StreamSession>> g_streamSessions;
	static std::mutex g_sessionsMutex;
	static uint64_t g_nextSessionId = 1;

	// Security helper: Block localhost and loopback. Returns the first safe IP or empty string.
	static std::string isUrlSafe(const std::string& url_str) {
		std::string low = url_str;
		std::transform(low.begin(), low.end(), low.begin(), ::tolower);

		// Basic host extraction
		std::string host = low;
		size_t protocol_pos = host.find("://");
		if (protocol_pos != std::string::npos) {
			host = host.substr(protocol_pos + 3);
		}
		size_t path_pos = host.find_first_of("/?#");
		if (path_pos != std::string::npos) {
			host = host.substr(0, path_pos);
		}
		size_t port_pos = host.find(':');
		if (port_pos != std::string::npos) {
			host = host.substr(0, port_pos);
		}

		if (host.empty()) return "";

		// Resolve host to IP addresses
		try {
			boost::asio::io_context io_context;
			boost::asio::ip::tcp::resolver resolver(io_context);
			boost::asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(host, "");

			std::string safeIp = "";

			for (auto it = endpoints.begin(); it != endpoints.end(); ++it) {
				boost::asio::ip::address addr = it->endpoint().address();

				// Check if loopback
				if (addr.is_loopback()) return "";

				// Check if unspecified (0.0.0.0 or ::)
				if (addr.is_unspecified()) return "";

				// Check private ranges for IPv4
				if (addr.is_v4()) {
					auto v4 = addr.to_v4().to_bytes();
					if (v4[0] == 10) return "";
					if (v4[0] == 172 && (v4[1] >= 16 && v4[1] <= 31)) return "";
					if (v4[0] == 192 && v4[1] == 168) return "";
					if (v4[0] == 169 && v4[1] == 254) return "";
				} else if (addr.is_v6()) {
					auto v6 = addr.to_v6();
					if (v6.is_link_local() || v6.is_site_local()) return "";
					auto bytes = v6.to_bytes();
					if ((bytes[0] & 0xfe) == 0xfc) return "";
				}

				if (safeIp.empty()) {
					safeIp = addr.to_string();
				}
			}
			return safeIp;
		} catch (...) {
			return "";
		}
	}

	// HTTP GET request
	static sol::table httpGet(sol::this_state ts, const std::string& url, sol::optional<sol::table> optHeaders) {
		sol::state_view lua(ts);
		sol::table result = lua.create_table();

		std::string safeIp = isUrlSafe(url);
		if (safeIp.empty()) {
			result["error"] = "Security: URL blocked (Localhost access denied or resolve failed)";
			result["ok"] = false;
			result["headers"] = lua.create_table();
			return result;
		}

		cpr::Header headers;
		if (optHeaders) {
			sol::table headersTable = *optHeaders;
			for (auto& pair : headersTable) {
				if (pair.first.is<std::string>() && pair.second.is<std::string>()) {
					headers[pair.first.as<std::string>()] = pair.second.as<std::string>();
				}
			}
		}

		const UrlParts parts = parseUrlParts(url);

		cpr::Response response = cpr::Get(
			buildPinnedUrl(safeIp, parts),
			headers,
			cpr::Header { { "Host", parts.authority } },
			cpr::Timeout { 10000 }
		);

		result["status"] = static_cast<int>(response.status_code);
		result["body"] = response.text;
		result["error"] = response.error.message;
		result["ok"] = response.status_code >= 200 && response.status_code < 300;

		sol::table respHeaders = lua.create_table();
		for (const auto& h : response.header) {
			respHeaders[h.first] = h.second;
		}
		result["headers"] = respHeaders;
		return result;
	}

	// HTTP POST request
	static sol::table httpPost(sol::this_state ts, const std::string& url, const std::string& body, sol::optional<sol::table> optHeaders) {
		sol::state_view lua(ts);
		sol::table result = lua.create_table();

		std::string safeIp = isUrlSafe(url);
		if (safeIp.empty()) {
			result["error"] = "Security: URL blocked (Localhost access denied or resolve failed)";
			result["ok"] = false;
			result["headers"] = lua.create_table();
			return result;
		}

		cpr::Header headers;
		if (optHeaders) {
			sol::table headersTable = *optHeaders;
			for (auto& pair : headersTable) {
				if (pair.first.is<std::string>() && pair.second.is<std::string>()) {
					headers[pair.first.as<std::string>()] = pair.second.as<std::string>();
				}
			}
		}

		const UrlParts parts = parseUrlParts(url);

		cpr::Response response = cpr::Post(
			buildPinnedUrl(safeIp, parts),
			cpr::Body { body },
			headers,
			cpr::Header { { "Host", parts.authority } },
			cpr::Timeout { 10000 }
		);

		result["status"] = static_cast<int>(response.status_code);
		result["body"] = response.text;
		result["error"] = response.error.message;
		result["ok"] = response.status_code >= 200 && response.status_code < 300;

		sol::table respHeaders = lua.create_table();
		for (const auto& h : response.header) {
			respHeaders[h.first] = h.second;
		}
		result["headers"] = respHeaders;
		return result;
	}

	// Helper to convert Lua table to JSON with cycle detection
	struct LuaToJson {
		std::set<const void*> visited;
		nlohmann::json convert(sol::object obj) {
			if (obj.is<bool>()) return obj.as<bool>();
			if (obj.is<int>()) return obj.as<int>();
			if (obj.is<double>()) return obj.as<double>();
			if (obj.is<std::string>()) return obj.as<std::string>();
			if (obj.is<sol::nil_t>()) return nullptr;

			if (obj.is<sol::table>()) {
				sol::table tbl = obj.as<sol::table>();
				const void* ptr = tbl.pointer();
				if (visited.count(ptr)) return nullptr;
				visited.insert(ptr);

				nlohmann::json result;
				bool isArray = true;
				size_t maxKey = 0, count = 0;

				for (auto& pair : tbl) {
					if (pair.first.is<size_t>()) {
						size_t k = pair.first.as<size_t>();
						if (k > maxKey) maxKey = k;
						count++;
					} else {
						isArray = false; break;
					}
				}

				if (isArray && maxKey == count && maxKey > 0) {
					result = nlohmann::json::array();
					for (size_t i = 1; i <= maxKey; ++i) result.push_back(convert(tbl[i]));
				} else {
					result = nlohmann::json::object();
					for (auto& pair : tbl) {
						std::string key;
						if (pair.first.is<std::string>()) key = pair.first.as<std::string>();
						else if (pair.first.is<int>()) key = std::to_string(pair.first.as<int>());
						else continue;
						result[key] = convert(pair.second);
					}
				}
				visited.erase(ptr);
				return result;
			}
			return nullptr;
		}
	};

	// HTTP POST with JSON body
	static sol::table httpPostJson(sol::this_state ts, const std::string& url, sol::table jsonBody, sol::optional<sol::table> optHeaders) {
		sol::state_view lua(ts);

		LuaToJson converter;
		std::string jsonStr = converter.convert(jsonBody).dump();

		// Add Content-Type header if not present
		sol::table headers = lua.create_table();
		headers["Content-Type"] = "application/json";

		if (optHeaders) {
			for (auto& pair : *optHeaders) {
				if (pair.first.is<std::string>() && pair.second.is<std::string>()) {
					headers[pair.first.as<std::string>()] = pair.second.as<std::string>();
				}
			}
		}

		return httpPost(ts, url, jsonStr, headers);
	}

	// Start a streaming POST request - returns session ID
	static sol::table httpPostStream(sol::this_state ts, const std::string& url, const std::string& body, sol::optional<sol::table> optHeaders) {
		sol::state_view lua(ts);
		sol::table result = lua.create_table();

		std::string safeIp = isUrlSafe(url);
		if (safeIp.empty()) {
			result["error"] = "Security: URL blocked (Localhost access denied or resolve failed)";
			result["ok"] = false;
			result["headers"] = lua.create_table();
			return result;
		}

		auto session = std::make_shared<StreamSession>();
		uint64_t sessionId;

		{
			std::lock_guard<std::mutex> lock(g_sessionsMutex);
			if (g_streamSessions.size() >= MAX_STREAM_SESSIONS || g_activeStreamCount.load(std::memory_order_relaxed) >= MAX_STREAM_SESSIONS) {
				result["error"] = "Resource limit: Too many active stream sessions";
				result["ok"] = false;
				return result;
			}
			sessionId = g_nextSessionId++;
			while (g_streamSessions.find(sessionId) != g_streamSessions.end()) {
				sessionId = g_nextSessionId++;
			}
			g_streamSessions[sessionId] = session;
		}
		g_activeStreamCount.fetch_add(1, std::memory_order_relaxed);

		cpr::Header headers;
		if (optHeaders) {
			sol::table headersTable = *optHeaders;
			for (auto& pair : headersTable) {
				if (pair.first.is<std::string>() && pair.second.is<std::string>()) {
					headers[pair.first.as<std::string>()] = pair.second.as<std::string>();
				}
			}
		}

		UrlParts parts = parseUrlParts(url);

		try {
			std::thread([session, sessionId, parts, body, headers, safeIp]() {
			struct StreamCleanup {
				uint64_t sessionId;
				std::shared_ptr<StreamSession> session;
				~StreamCleanup() {
					g_activeStreamCount.fetch_sub(1, std::memory_order_relaxed);
					std::lock_guard<std::mutex> lock(g_sessionsMutex);
					auto it = g_streamSessions.find(sessionId);
					if (it != g_streamSessions.end() && it->second == session) {
						if (session->isFinished() && !session->hasChunks()) {
							g_streamSessions.erase(it);
						}
					}
				}
			} cleanup { sessionId, session };

			try {
				std::function<bool(std::string_view, intptr_t)> writeCallback = [session](std::string_view data, intptr_t /*userdata*/) -> bool {
					return session->appendChunk(std::string(data));
				};

				cpr::Response response = cpr::Post(
					buildPinnedUrl(safeIp, parts),
					cpr::Body { body },
					headers,
					cpr::Header { { "Host", parts.authority } },
					cpr::WriteCallback { writeCallback, 0 },
					cpr::Timeout { 30000 }
				);

				session->setStatusCode(static_cast<int>(response.status_code));
				session->setHeaders(response.header);

				if (response.error) {
					session->setError(response.error.message);
				} else {
					session->setFinished();
				}
			} catch (const std::exception& e) {
				session->setError(e.what());
			} catch (...) {
				session->setError("Unknown streaming HTTP error");
			}
			}).detach();
		} catch (const std::exception& e) {
			g_activeStreamCount.fetch_sub(1, std::memory_order_relaxed);
			std::lock_guard<std::mutex> lock(g_sessionsMutex);
			g_streamSessions.erase(sessionId);
			result["error"] = std::string("Failed to start streaming thread: ") + e.what();
			result["ok"] = false;
			return result;
		}

		result["sessionId"] = sessionId;
		result["ok"] = true;

		return result;
	}

	// Start a streaming POST request with JSON body - returns session ID
	static sol::table httpPostJsonStream(sol::this_state ts, const std::string& url, sol::table jsonBody, sol::optional<sol::table> optHeaders) {
		sol::state_view lua(ts);

		LuaToJson converter;
		std::string jsonStr = converter.convert(jsonBody).dump();

		// Add Content-Type header if not present
		sol::table headers = lua.create_table();
		headers["Content-Type"] = "application/json";

		if (optHeaders) {
			for (auto& pair : *optHeaders) {
				if (pair.first.is<std::string>() && pair.second.is<std::string>()) {
					headers[pair.first.as<std::string>()] = pair.second.as<std::string>();
				}
			}
		}

		return httpPostStream(ts, url, jsonStr, headers);
	}

	// Read available chunks from a stream session
	static sol::table httpStreamRead(sol::this_state ts, uint64_t sessionId) {
		sol::state_view lua(ts);
		sol::table result = lua.create_table();

		std::shared_ptr<StreamSession> session;
		{
			std::lock_guard<std::mutex> lock(g_sessionsMutex);
			auto it = g_streamSessions.find(sessionId);
			if (it == g_streamSessions.end()) {
				result["ok"] = false;
				result["error"] = "Invalid session ID";
				result["finished"] = true;
				return result;
			}
			session = it->second;
		}

		// Use atomic snapshot to avoid race conditions
		StreamSession::Snapshot s = session->getSnapshot();

		result["data"] = s.data;
		result["finished"] = s.finished;
		result["hasError"] = s.hasError;

		if (s.hasError) {
			result["error"] = s.error;
			result["ok"] = false;
		} else {
			result["ok"] = true;
		}

		if (s.finished) {
			result["status"] = s.status;
			sol::table respHeaders = lua.create_table();
			for (const auto& h : s.headers) {
				respHeaders[h.first] = h.second;
			}
			result["headers"] = respHeaders;

			// Cleanup finished session if all data drained
			if (s.data.empty()) {
				std::lock_guard<std::mutex> lock(g_sessionsMutex);
				g_streamSessions.erase(sessionId);
			}
		}

		return result;
	}

	// Close and cleanup a stream session
	static bool httpStreamClose(uint64_t sessionId) {
		std::lock_guard<std::mutex> lock(g_sessionsMutex);
		auto it = g_streamSessions.find(sessionId);
		if (it != g_streamSessions.end()) {
			it->second->close();
			return true;
		}
		return false;
	}

	// Check if a stream session is finished
	static sol::table httpStreamStatus(sol::this_state ts, uint64_t sessionId) {
		sol::state_view lua(ts);
		sol::table result = lua.create_table();

		std::shared_ptr<StreamSession> session;
		{
			std::lock_guard<std::mutex> lock(g_sessionsMutex);
			auto it = g_streamSessions.find(sessionId);
			if (it == g_streamSessions.end()) {
				result["valid"] = false;
				result["finished"] = true;
				return result;
			}
			session = it->second;
		}

		// Atomic status check via getStatus (doesn't drain data)
		StreamSession::Snapshot s = session->getStatus();

		result["valid"] = true;
		result["finished"] = s.finished;
		result["hasError"] = s.hasError;
		result["hasData"] = session->hasChunks();

		if (s.hasError) {
			result["error"] = s.error;
		}

		if (s.finished) {
			result["status"] = s.status;
		}
		
		return result;
	}

	void registerHttp(sol::state& lua) {
		sol::table http = lua.create_named_table("http");

		// Basic HTTP methods
		http["get"] = httpGet;
		http["post"] = httpPost;
		http["postJson"] = httpPostJson;

		// Streaming HTTP methods
		http["postStream"] = httpPostStream;
		http["postJsonStream"] = httpPostJsonStream;
		http["streamRead"] = httpStreamRead;
		http["streamClose"] = httpStreamClose;
		http["streamStatus"] = httpStreamStatus;
	}

} // namespace LuaAPI
