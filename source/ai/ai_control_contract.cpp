//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "ai/ai_control_contract.h"

#include "editor/connected_positions.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <filesystem>
#include <format>
#include <limits>
#include <set>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace AiControl {

	namespace {

		constexpr int MaxJsonDepth = 64;

		ApiError BadRequest(std::string message) {
			return ApiError { 400, "bad_request", std::move(message) };
		}

		std::string_view TrimView(std::string_view text) {
			while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
				text.remove_prefix(1);
			}
			while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) {
				text.remove_suffix(1);
			}
			return text;
		}

		std::optional<size_t> ParseDecimal(std::string_view text) {
			if (text.empty()) {
				return std::nullopt;
			}
			size_t value = 0;
			const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
			if (ec != std::errc() || end != text.data() + text.size()) {
				return std::nullopt;
			}
			return value;
		}

		// Nesting depth of a JSON text, ignoring brackets inside strings. Copying or dumping a very
		// deep document recurses, so depth is bounded before the text is parsed at all.
		int JsonNestingDepth(std::string_view text) {
			int depth = 0;
			int deepest = 0;
			bool in_string = false;
			bool escaped = false;
			for (const char character : text) {
				if (in_string) {
					if (escaped) {
						escaped = false;
					} else if (character == '\\') {
						escaped = true;
					} else if (character == '"') {
						in_string = false;
					}
					continue;
				}
				if (character == '"') {
					in_string = true;
				} else if (character == '[' || character == '{') {
					deepest = std::max(deepest, ++depth);
				} else if (character == ']' || character == '}') {
					--depth;
				}
			}
			return deepest;
		}

		bool IsAbsoluteUtf8Path(std::string_view text) {
			try {
				return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(text.data()), text.size())).is_absolute();
			} catch (const std::exception&) {
				return false;
			}
		}

		// UNC ("\\server\share", "//server/share") and device ("\\?\...", "\\.\...") paths all
		// start with two separators.
		bool IsNetworkOrDevicePath(std::string_view text) {
			const auto separator = [](char character) {
				return character == '\\' || character == '/';
			};
			return text.size() >= 2 && separator(text[0]) && separator(text[1]);
		}

		int EditDistance(std::string_view left, std::string_view right) {
			std::vector<int> previous(right.size() + 1);
			std::vector<int> current(right.size() + 1);
			for (size_t column = 0; column <= right.size(); ++column) {
				previous[column] = static_cast<int>(column);
			}
			for (size_t row = 1; row <= left.size(); ++row) {
				current[0] = static_cast<int>(row);
				for (size_t column = 1; column <= right.size(); ++column) {
					const int substitution = previous[column - 1] + (left[row - 1] == right[column - 1] ? 0 : 1);
					current[column] = std::min({ previous[column] + 1, current[column - 1] + 1, substitution });
				}
				std::swap(previous, current);
			}
			return previous[right.size()];
		}

		// Reads an optional boolean field into `value`; `value` keeps its default when the field is absent.
		std::optional<ApiError> ReadBool(const nlohmann::json& body, std::string_view key, bool& value) {
			const auto found = body.find(key);
			if (found == body.end()) {
				return std::nullopt;
			}
			if (!found->is_boolean()) {
				return BadRequest(std::format("{} must be a boolean.", key));
			}
			value = found->get<bool>();
			return std::nullopt;
		}

		std::optional<int> IntegerValue(const nlohmann::json& value) {
			if (!value.is_number_integer()) {
				return std::nullopt;
			}
			if (value.is_number_unsigned()) {
				const auto number = value.get<uint64_t>();
				if (number > static_cast<uint64_t>(std::numeric_limits<int>::max())) {
					return std::nullopt;
				}
				return static_cast<int>(number);
			}
			const auto number = value.get<int64_t>();
			if (number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max()) {
				return std::nullopt;
			}
			return static_cast<int>(number);
		}

		std::optional<uint16_t> ItemIdValue(const nlohmann::json& value) {
			const auto number = IntegerValue(value);
			if (!number || *number < 1 || *number > std::numeric_limits<uint16_t>::max()) {
				return std::nullopt;
			}
			return static_cast<uint16_t>(*number);
		}

		std::optional<ApiError> ValidateFloor(const std::optional<int>& z) {
			if (!z || *z < 0 || *z > MAP_MAX_LAYER) {
				return BadRequest(std::format("Expected integer floor z from 0 to {}.", MAP_MAX_LAYER));
			}
			return std::nullopt;
		}

		constexpr std::array<RouteSpec, 16> RouteTable { {
			{ RouteId::Health, HttpMethod::Get, "/health", RouteKind::Status, false, "Liveness, API version and whether a map is open.", "none" },
			{ RouteId::Capabilities, HttpMethod::Get, "/capabilities", RouteKind::Status, false, "This route table, limits and rules.", "none" },
			{ RouteId::Snapshot, HttpMethod::Get, "/snapshot", RouteKind::Query, true, "Open map, selection, active tool state and the brush catalog.", "none" },
			{ RouteId::Brushes, HttpMethod::Post, "/brushes", RouteKind::Query, true, "Search brushes by name and type.", "{q?: substring, type?: brush type, limit?: 1-500}" },
			{ RouteId::InspectTile, HttpMethod::Post, "/inspect_tile", RouteKind::Query, true, "Every item of one tile.", "{x, y, z}" },
			{ RouteId::InspectArea, HttpMethod::Post, "/inspect_area", RouteKind::Query, true, "Item histogram and sample tiles of an area (up to 20000 tiles).", "{x, y, z, width, height, sampleLimit?: 0-512}" },
			{ RouteId::TerrainGrid, HttpMethod::Post, "/terrain_grid", RouteKind::Query, true, "Ground ids of an area as character rows with a legend (up to 50000 tiles).", "{x, y, z, width, height}" },
			{ RouteId::Paint, HttpMethod::Post, "/paint", RouteKind::Mutation, true, "Draw a ground, wall, door, doodad, carpet, table or RAW brush on tiles, with auto-borders.", "{brush, type?, z, area?: {x, y, width, height}, tiles?: [[x, y], ...], automagic?: bool}" },
			{ RouteId::ClearArea, HttpMethod::Post, "/clear_area", RouteKind::Mutation, true, "Empty an area (up to 50000 tiles) in one undo step.", "{x, y, z, width, height, keepGround?: bool, keepFlags?: bool, label?: string}" },
			{ RouteId::PreviewPatch, HttpMethod::Post, "/preview_patch", RouteKind::Query, true, "Before/after of an item-level patch and the token apply_patch needs.", "{label?, tiles: [{x, y, z, groundId?, replaceItems?, addItemIds?}]}" },
			{ RouteId::ApplyPatch, HttpMethod::Post, "/apply_patch", RouteKind::Mutation, true, "Apply a previewed patch in one undo step; stale tokens are rejected.", "{label?, previewToken, tiles: same as preview_patch}" },
			{ RouteId::Undo, HttpMethod::Post, "/undo", RouteKind::Mutation, true, "Undo editor history steps.", "{steps?: 1-1000}" },
			{ RouteId::Redo, HttpMethod::Post, "/redo", RouteKind::Mutation, true, "Redo editor history steps.", "{steps?: 1-1000}" },
			{ RouteId::History, HttpMethod::Get, "/history", RouteKind::Query, true, "Undo index, size and the last 100 entries.", "none" },
			{ RouteId::Batch, HttpMethod::Post, "/batch", RouteKind::Mutation, false, "Run up to 200 ops in order, stopping at the first failure.", "{ops: [{op: route name, ...route body}], rollbackOnError?: bool}" },
			{ RouteId::ExportRegion, HttpMethod::Post, "/export_region", RouteKind::Query, true, "Render a map region at 100% zoom into a plain PNG file.", "{minX, minY, maxX, maxY, z, path: absolute local .png path, overwrite?: bool}" },
		} };

		std::string_view RouteKindName(RouteKind kind) {
			switch (kind) {
				case RouteKind::Status:
					return "status";
				case RouteKind::Query:
					return "query";
				case RouteKind::Mutation:
					return "mutation";
			}
			return "query";
		}

		const RouteSpec* FindRouteByPath(std::string_view path) {
			const auto found = std::ranges::find(RouteTable, path, &RouteSpec::path);
			return found == RouteTable.end() ? nullptr : &*found;
		}

	} // namespace

	nlohmann::json ErrorBody(const ApiError& error) {
		return {
			{ "ok", false },
			{ "error", error.code },
			{ "message", error.message },
		};
	}

	std::string SerializeJson(const nlohmann::json& value) {
		return value.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
	}

	std::string ToLower(std::string_view text) {
		std::string lowered(text);
		std::ranges::transform(lowered, lowered.begin(), [](unsigned char character) {
			return static_cast<char>(std::tolower(character));
		});
		return lowered;
	}

	std::string_view HttpMethodName(HttpMethod method) {
		return method == HttpMethod::Post ? "POST" : "GET";
	}

	HttpParseResult ParseHttpRequest(std::string_view received) {
		HttpParseResult result;
		const auto reject = [&result](int status, std::string code, std::string message) {
			result.state = HttpParseState::Rejected;
			result.rejection = ApiError { status, std::move(code), std::move(message) };
			return result;
		};

		const size_t header_end = received.find("\r\n\r\n");
		if (header_end == std::string_view::npos || header_end + 4 > MaxHeaderBytes) {
			if (received.size() > MaxHeaderBytes) {
				return reject(431, "headers_too_large", "HTTP headers exceed 64 KiB.");
			}
			return result;
		}

		const std::string_view head = received.substr(0, header_end);
		const size_t line_end = head.find("\r\n");
		const std::string_view request_line = head.substr(0, line_end);
		const size_t first_space = request_line.find(' ');
		const size_t second_space = first_space == std::string_view::npos ? std::string_view::npos : request_line.find(' ', first_space + 1);
		if (second_space == std::string_view::npos || request_line.find(' ', second_space + 1) != std::string_view::npos) {
			return reject(400, "bad_request", "Malformed HTTP request line.");
		}
		const std::string_view method = request_line.substr(0, first_space);
		const std::string_view target = request_line.substr(first_space + 1, second_space - first_space - 1);
		const std::string_view version = request_line.substr(second_space + 1);
		if (version != "HTTP/1.1" && version != "HTTP/1.0") {
			return reject(400, "bad_request", "Only HTTP/1.0 and HTTP/1.1 are supported.");
		}
		if (method == "GET") {
			result.request.method = HttpMethod::Get;
		} else if (method == "POST") {
			result.request.method = HttpMethod::Post;
		} else {
			return reject(405, "method_not_allowed", "Only GET and POST are supported.");
		}
		if (target.empty() || target.front() != '/') {
			return reject(400, "bad_request", "The request target must be an absolute path.");
		}
		result.request.path = std::string(target.substr(0, target.find_first_of("?#")));

		std::optional<size_t> content_length;
		size_t cursor = line_end == std::string_view::npos ? head.size() : line_end + 2;
		while (cursor < head.size()) {
			const size_t next = head.find("\r\n", cursor);
			const std::string_view line = head.substr(cursor, next == std::string_view::npos ? std::string_view::npos : next - cursor);
			cursor = next == std::string_view::npos ? head.size() : next + 2;

			if (line.empty() || line.front() == ' ' || line.front() == '\t') {
				return reject(400, "bad_request", "Folded or empty header lines are not supported.");
			}
			const size_t colon = line.find(':');
			if (colon == std::string_view::npos || colon == 0 || line.substr(0, colon).find_first_of(" \t") != std::string_view::npos) {
				return reject(400, "bad_request", "Malformed HTTP header line.");
			}
			const std::string name = ToLower(line.substr(0, colon));
			const std::string_view value = TrimView(line.substr(colon + 1));
			if (name == "host") {
				if (result.request.host) {
					return reject(400, "bad_request", "Duplicate Host header.");
				}
				result.request.host = std::string(value);
			} else if (name == "origin") {
				result.request.hasOrigin = true;
			} else if (name == "content-type") {
				if (result.request.contentType) {
					return reject(400, "bad_request", "Duplicate Content-Type header.");
				}
				result.request.contentType = std::string(value);
			} else if (name == "content-length") {
				if (content_length) {
					return reject(400, "bad_request", "Duplicate Content-Length header.");
				}
				content_length = ParseDecimal(value);
				if (!content_length) {
					return reject(400, "bad_request", "Invalid Content-Length header.");
				}
				if (*content_length > MaxBodyBytes) {
					return reject(413, "body_too_large", "HTTP body exceeds 1 MiB.");
				}
			} else if (name == "transfer-encoding") {
				return reject(400, "unsupported_transfer_encoding", "Transfer-Encoding is not supported; send Content-Length.");
			}
		}

		const size_t body_start = header_end + 4;
		const size_t body_length = content_length.value_or(0);
		if (received.size() - body_start < body_length) {
			return result;
		}
		result.request.body = std::string(received.substr(body_start, body_length));
		result.state = HttpParseState::Complete;
		return result;
	}

	std::optional<ApiError> CheckRequestPolicy(const HttpRequest& request, uint16_t port) {
		if (request.hasOrigin) {
			return ApiError { 403, "origin_forbidden", "Requests with an Origin header are rejected: the API serves local tools, not web pages." };
		}
		const std::string host = request.host ? ToLower(*request.host) : std::string();
		if (host != std::format("127.0.0.1:{}", port) && host != std::format("localhost:{}", port)) {
			return ApiError { 403, "host_forbidden", std::format("Host must be 127.0.0.1:{} or localhost:{}.", port, port) };
		}
		if (request.method == HttpMethod::Post) {
			const std::string_view content_type = request.contentType ? std::string_view(*request.contentType) : std::string_view();
			if (ToLower(TrimView(content_type.substr(0, content_type.find(';')))) != "application/json") {
				return ApiError { 415, "unsupported_media_type", "POST requests must send Content-Type: application/json." };
			}
		}
		return std::nullopt;
	}

	JsonBodyResult ParseJsonBody(std::string_view text) {
		JsonBodyResult result;
		if (TrimView(text).empty()) {
			return result;
		}
		if (JsonNestingDepth(text) > MaxJsonDepth) {
			result.error = ApiError { 400, "json_too_deep", std::format("JSON nesting exceeds {} levels.", MaxJsonDepth) };
			return result;
		}
		nlohmann::json parsed = nlohmann::json::parse(text, nullptr, false);
		if (parsed.is_discarded()) {
			result.error = ApiError { 400, "invalid_json", "The request body is not valid JSON." };
			return result;
		}
		if (!parsed.is_object()) {
			result.error = BadRequest("Expected a JSON object body.");
			return result;
		}
		result.body = std::move(parsed);
		return result;
	}

	std::string_view ReasonPhrase(int status) {
		switch (status) {
			case 200:
				return "OK";
			case 400:
				return "Bad Request";
			case 403:
				return "Forbidden";
			case 404:
				return "Not Found";
			case 405:
				return "Method Not Allowed";
			case 413:
				return "Payload Too Large";
			case 415:
				return "Unsupported Media Type";
			case 431:
				return "Request Header Fields Too Large";
			case 503:
				return "Service Unavailable";
			default:
				return "Internal Server Error";
		}
	}

	std::string BuildHttpResponse(int status, std::string_view payload) {
		std::string response = std::format(
			"HTTP/1.1 {} {}\r\n"
			"Content-Type: application/json; charset=utf-8\r\n"
			"Content-Length: {}\r\n"
			"Connection: close\r\n"
			"Cache-Control: no-store\r\n"
			"X-Content-Type-Options: nosniff\r\n"
			"\r\n",
			status, ReasonPhrase(status), payload.size()
		);
		response.append(payload);
		return response;
	}

	std::optional<uint16_t> ParsePort(std::string_view text) {
		const auto value = ParseDecimal(TrimView(text));
		if (!value || *value < 1024 || *value > 65535) {
			return std::nullopt;
		}
		return static_cast<uint16_t>(*value);
	}

	std::span<const RouteSpec> Routes() {
		return RouteTable;
	}

	const RouteSpec& RouteFor(RouteId id) {
		return *std::ranges::find(RouteTable, id, &RouteSpec::id);
	}

	std::string_view NormalizeRoutePath(std::string_view path) {
		if (path.starts_with("/ai/")) {
			path.remove_prefix(3);
		}
		return path;
	}

	RouteMatch LookupRoute(HttpMethod method, std::string_view path) {
		const RouteSpec* route = FindRouteByPath(NormalizeRoutePath(path));
		if (!route) {
			return { nullptr, ApiError { 404, "not_found", "Unknown AI Control API endpoint; GET /capabilities lists them." } };
		}
		if (route->method != method) {
			return { nullptr, ApiError { 405, "method_not_allowed", std::format("{} expects {}.", route->path, HttpMethodName(route->method)) } };
		}
		return { route, std::nullopt };
	}

	nlohmann::json CapabilitiesJson() {
		nlohmann::json endpoints = nlohmann::json::array();
		for (const RouteSpec& route : RouteTable) {
			endpoints.push_back({
				{ "method", HttpMethodName(route.method) },
				{ "path", route.path },
				{ "kind", RouteKindName(route.kind) },
				{ "batchable", route.batchable },
				{ "summary", route.summary },
				{ "body", route.body },
			});
		}
		return {
			{ "ok", true },
			{ "name", ServiceName },
			{ "version", ApiVersion },
			{ "bindAddress", BindAddress },
			{ "pathAlias", "/ai/<route> is accepted for every /<route>" },
			{ "limits", {
							{ "headerBytes", MaxHeaderBytes },
							{ "bodyBytes", MaxBodyBytes },
							{ "inspectAreaTiles", InspectAreaTileLimit },
							{ "inspectAreaSamples", InspectAreaSampleLimit },
							{ "terrainGridTiles", TerrainGridTileLimit },
							{ "clearAreaTiles", ClearAreaTileLimit },
							{ "paintTiles", PaintTileLimit },
							{ "patchTiles", PatchTileLimit },
							{ "patchAddedItems", PatchAddedItemLimit },
							{ "batchOps", BatchOpLimit },
							{ "historySteps", HistoryStepLimit },
							{ "exportSidePixels", ExportSideLimitPixels },
							{ "exportPixels", ExportPixelLimit },
						} },
			{ "rules", {
						   "Every POST needs Content-Type: application/json; requests with an Origin header or another Host than 127.0.0.1:<port> / localhost:<port> are rejected.",
						   "Everything except health and capabilities answers 503 busy while the editor loads, a modal dialog is open or another request is running.",
						   "Mutations are undo steps of the open map and never merge with the user's own actions; a Live client is read-only.",
						   "paint draws through the editor's own brushes on single tiles with automagic on unless \"automagic\": false, in one undo step; the user's brush, brush size, automagic, editor mode and selection are never touched. Doors follow the editor's locked-door toggle (snapshot tools.doorsLocked). \"changed\": false means the brush placed nothing (e.g. a doodad on tiles without ground).",
						   "undo and redo act like the editor's own Undo/Redo, including switching to selection mode when they leave a selection.",
						   "apply_patch never computes borders; replaceItems removes stacked items only.",
						   "clear_area removes items, creatures, spawns, house membership, zones and, unless keepFlags, tile flags in one undo step.",
						   "batch checks op names and the fields of every paint, clear_area, apply_patch, undo, redo and export_region before anything runs, then stops at the first failing op; rollbackOnError (default true) undoes exactly the history entries the batch added. rolledBack is false when that cannot restore the start: an entry was merged or dropped from full history, or an undo/redo op ran in the batch (then nothing is undone).",
						   "Mutations and export_region answer 503 busy while the user is drawing, dragging or pasting.",
						   "export_region renders the region at 100% zoom on its floor into a plain PNG; the view and minimap are restored afterwards. The editor is blocked while it renders, about one redraw per viewport-sized tile, so the largest regions take seconds. An existing file is replaced only with \"overwrite\": true.",
					   } },
			{ "endpoints", std::move(endpoints) },
		};
	}

	std::optional<ApiError> CheckBusyGate(const RouteSpec& route, const EditorActivity& activity) {
		if (route.kind == RouteKind::Status) {
			return std::nullopt;
		}
		if (activity.requestInProgress) {
			return ApiError { 503, "busy", "Another AI Control API request is still running." };
		}
		if (activity.loading) {
			return ApiError { 503, "busy", "The editor is loading." };
		}
		if (activity.renderingDisabled) {
			return ApiError { 503, "busy", "The editor is busy with a long operation." };
		}
		if (activity.modalDialogOpen) {
			return ApiError { 503, "busy", "A modal dialog is open in the editor." };
		}
		if (activity.userInteracting && (route.kind == RouteKind::Mutation || route.id == RouteId::ExportRegion)) {
			return ApiError { 503, "busy", "The user is drawing, dragging or pasting in the editor." };
		}
		return std::nullopt;
	}

	int PhaseTimeoutSeconds(ConnectionPhase phase) {
		switch (phase) {
			case ConnectionPhase::Reading:
				return 10;
			case ConnectionPhase::Writing:
				return 5;
			case ConnectionPhase::Draining:
				return 2;
		}
		return 2;
	}

	bool IsConnectionExpired(ConnectionPhase phase, std::chrono::steady_clock::duration elapsed) {
		return elapsed > std::chrono::seconds(PhaseTimeoutSeconds(phase));
	}

	bool CountsTowardConnectionLimit(ConnectionPhase phase) {
		return phase != ConnectionPhase::Draining;
	}

	std::optional<int> Integer(const nlohmann::json& body, std::string_view key) {
		const auto found = body.find(key);
		if (found == body.end()) {
			return std::nullopt;
		}
		return IntegerValue(*found);
	}

	std::optional<ApiError> RejectUnknownFields(const nlohmann::json& body, std::span<const std::string_view> allowed, std::string_view context) {
		if (!body.is_object()) {
			return BadRequest(std::format("Expected a JSON object for {}.", context));
		}
		for (const auto& [key, value] : body.items()) {
			if (std::ranges::find(allowed, std::string_view(key)) == allowed.end()) {
				return BadRequest(std::format("Unknown {} field: {}", context, key));
			}
		}
		return std::nullopt;
	}

	std::optional<Area> ParseArea(const nlohmann::json& body) {
		const auto x = Integer(body, "x");
		const auto y = Integer(body, "y");
		const auto z = Integer(body, "z");
		const auto width = Integer(body, "width");
		const auto height = Integer(body, "height");
		if (!x || !y || !z || !width || !height) {
			return std::nullopt;
		}
		return Area { *x, *y, *z, *width, *height };
	}

	std::optional<ApiError> ValidateArea(const Area& area, int mapWidth, int mapHeight, int64_t tileLimit) {
		if (area.x < 0 || area.y < 0 || area.z < 0 || area.z > MAP_MAX_LAYER || area.width <= 0 || area.height <= 0) {
			return BadRequest(std::format("Expected non-negative x and y, z from 0 to {} and positive width and height.", MAP_MAX_LAYER));
		}
		const int64_t end_x = static_cast<int64_t>(area.x) + area.width;
		const int64_t end_y = static_cast<int64_t>(area.y) + area.height;
		if (end_x > mapWidth || end_y > mapHeight) {
			return ApiError { 400, "area_outside_map", "Area is outside the map bounds." };
		}
		if (static_cast<int64_t>(area.width) * area.height > tileLimit) {
			return ApiError { 400, "area_too_large", std::format("Area exceeds the endpoint limit of {} tiles.", tileLimit) };
		}
		return std::nullopt;
	}

	nlohmann::json AreaJson(const Area& area) {
		return { { "x", area.x }, { "y", area.y }, { "z", area.z }, { "width", area.width }, { "height", area.height } };
	}

	TerrainGrid EncodeTerrainGrid(int width, int height, std::span<const std::optional<uint16_t>> itemIds) {
		TerrainGrid result;
		if (width <= 0 || height <= 0 || static_cast<int64_t>(width) * height != static_cast<int64_t>(itemIds.size())) {
			return result;
		}

		std::unordered_map<uint16_t, int> counts;
		for (const auto& item_id : itemIds) {
			if (item_id) {
				++counts[*item_id];
				++result.foundTiles;
			} else {
				++result.emptyTiles;
			}
		}

		std::vector<std::pair<uint16_t, int>> ranked(counts.begin(), counts.end());
		std::ranges::sort(ranked, [](const auto& left, const auto& right) {
			return left.second != right.second ? left.second > right.second : left.first < right.first;
		});

		static constexpr std::string_view Symbols = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
		std::unordered_map<uint16_t, char> symbol_by_id;
		for (size_t index = 0; index < ranked.size(); ++index) {
			const auto [item_id, count] = ranked[index];
			if (index < Symbols.size()) {
				symbol_by_id.emplace(item_id, Symbols[index]);
				result.legend.push_back({ Symbols[index], item_id, count });
			} else {
				result.overflow.push_back({ result.overflowChar, item_id, count });
			}
		}

		result.rows.reserve(static_cast<size_t>(height));
		for (int y = 0; y < height; ++y) {
			std::string row;
			row.reserve(static_cast<size_t>(width));
			for (int x = 0; x < width; ++x) {
				const auto& item_id = itemIds[static_cast<size_t>(y) * width + x];
				if (!item_id) {
					row.push_back(result.emptyChar);
					continue;
				}
				const auto found = symbol_by_id.find(*item_id);
				row.push_back(found == symbol_by_id.end() ? result.overflowChar : found->second);
			}
			result.rows.push_back(std::move(row));
		}
		return result;
	}

	PatchParseResult ParsePatchRequest(const nlohmann::json& body, PatchMode mode, int mapWidth, int mapHeight) {
		const auto fail = [](ApiError error) {
			return PatchParseResult { std::move(error), {} };
		};
		static constexpr std::array<std::string_view, 3> PatchFields { "label", "tiles", "previewToken" };
		if (auto error = RejectUnknownFields(body, PatchFields, "patch")) {
			return fail(std::move(*error));
		}

		PatchRequest request;
		if (const auto found = body.find("label"); found != body.end()) {
			if (!found->is_string()) {
				return fail(BadRequest("label must be a string."));
			}
			request.label = found->get<std::string>();
			if (request.label.size() > PatchLabelLimit) {
				return fail(BadRequest(std::format("label must not exceed {} bytes.", PatchLabelLimit)));
			}
		}
		if (const auto found = body.find("previewToken"); found != body.end()) {
			if (!found->is_string()) {
				return fail(BadRequest("previewToken must be a string."));
			}
			request.previewToken = found->get<std::string>();
		}
		if (mode == PatchMode::Apply) {
			const bool well_formed = request.previewToken.size() == 16 && std::ranges::all_of(request.previewToken, [](unsigned char character) {
										 return std::isdigit(character) || (character >= 'a' && character <= 'f');
									 });
			if (!well_formed) {
				return fail(BadRequest("previewToken must be the 16-character lowercase hexadecimal token from preview_patch."));
			}
		} else if (!request.previewToken.empty()) {
			return fail(BadRequest("previewToken is only accepted by apply_patch."));
		}

		const auto tiles = body.find("tiles");
		if (tiles == body.end() || !tiles->is_array() || tiles->empty()) {
			return fail(BadRequest("tiles must be a non-empty array."));
		}
		if (tiles->size() > PatchTileLimit) {
			return fail(ApiError { 400, "patch_too_large", std::format("A patch changes at most {} tiles.", PatchTileLimit) });
		}

		static constexpr std::array<std::string_view, 6> TileFields { "x", "y", "z", "groundId", "replaceItems", "addItemIds" };
		std::set<std::tuple<int, int, int>> seen;
		for (const auto& tile : *tiles) {
			if (auto error = RejectUnknownFields(tile, TileFields, "tile patch")) {
				return fail(std::move(*error));
			}
			const auto x = Integer(tile, "x");
			const auto y = Integer(tile, "y");
			const auto z = Integer(tile, "z");
			if (!x || !y || !z) {
				return fail(BadRequest("Every tile patch needs integer x, y and z."));
			}
			if (auto error = ValidateArea({ *x, *y, *z, 1, 1 }, mapWidth, mapHeight, 1)) {
				return fail(std::move(*error));
			}
			if (!seen.emplace(*x, *y, *z).second) {
				return fail(ApiError { 400, "duplicate_tile", "A patch may change each position only once." });
			}

			PatchTileRequest entry;
			entry.position = Position(*x, *y, *z);
			if (const auto found = tile.find("groundId"); found != tile.end()) {
				entry.groundId = ItemIdValue(*found);
				if (!entry.groundId) {
					return fail(BadRequest("groundId must be an integer from 1 to 65535."));
				}
			}
			if (const auto found = tile.find("replaceItems"); found != tile.end()) {
				if (!found->is_boolean()) {
					return fail(BadRequest("replaceItems must be a boolean."));
				}
				entry.replaceItems = found->get<bool>();
			}
			if (const auto found = tile.find("addItemIds"); found != tile.end()) {
				if (!found->is_array()) {
					return fail(BadRequest("addItemIds must be an array."));
				}
				for (const auto& item : *found) {
					const auto item_id = ItemIdValue(item);
					if (!item_id) {
						return fail(BadRequest("Every addItemIds value must be an integer from 1 to 65535."));
					}
					entry.addItemIds.push_back(*item_id);
					if (++request.addedItemCount > PatchAddedItemLimit) {
						return fail(ApiError { 400, "patch_too_large", std::format("A patch adds at most {} items.", PatchAddedItemLimit) });
					}
				}
			}
			if (!entry.groundId && !entry.replaceItems && entry.addItemIds.empty()) {
				return fail(BadRequest("Every tile patch must set groundId, replaceItems or at least one addItemIds value."));
			}
			request.tiles.push_back(std::move(entry));
		}
		return { std::nullopt, std::move(request) };
	}

	std::string StableToken(std::string_view value) {
		uint64_t hash = 14695981039346656037ULL;
		for (const unsigned char byte : value) {
			hash ^= byte;
			hash *= 1099511628211ULL;
		}
		return std::format("{:016x}", hash);
	}

	std::string_view BrushKindName(BrushKind kind) {
		switch (kind) {
			case BrushKind::Ground:
				return "ground";
			case BrushKind::Wall:
				return "wall";
			case BrushKind::WallDecoration:
				return "wall_decoration";
			case BrushKind::Door:
				return "door";
			case BrushKind::Doodad:
				return "doodad";
			case BrushKind::Carpet:
				return "carpet";
			case BrushKind::Table:
				return "table";
			case BrushKind::Raw:
				return "raw";
			case BrushKind::Other:
				return "other";
		}
		return "other";
	}

	std::optional<BrushKind> ParseBrushKind(std::string_view name) {
		static constexpr std::array<BrushKind, 9> Kinds {
			BrushKind::Ground,
			BrushKind::Wall,
			BrushKind::WallDecoration,
			BrushKind::Door,
			BrushKind::Doodad,
			BrushKind::Carpet,
			BrushKind::Table,
			BrushKind::Raw,
			BrushKind::Other,
		};
		const auto found = std::ranges::find(Kinds, name, BrushKindName);
		if (found == Kinds.end()) {
			return std::nullopt;
		}
		return *found;
	}

	bool IsPaintableBrushKind(BrushKind kind) {
		return kind != BrushKind::Other;
	}

	PaintParseResult ParsePaintRequest(const nlohmann::json& body, int mapWidth, int mapHeight) {
		const auto fail = [](ApiError error) {
			return PaintParseResult { std::move(error), {} };
		};
		static constexpr std::array<std::string_view, 6> PaintFields { "brush", "type", "z", "area", "tiles", "automagic" };
		if (auto error = RejectUnknownFields(body, PaintFields, "paint")) {
			return fail(std::move(*error));
		}

		PaintRequest request;
		const auto brush = body.find("brush");
		if (brush == body.end() || !brush->is_string() || brush->get<std::string>().empty()) {
			return fail(BadRequest("Expected brush: the name of a brush."));
		}
		request.brushName = brush->get<std::string>();
		if (const auto type = body.find("type"); type != body.end()) {
			const auto kind = type->is_string() ? ParseBrushKind(type->get<std::string>()) : std::nullopt;
			if (!kind || !IsPaintableBrushKind(*kind)) {
				return fail(BadRequest("type must be ground, wall, wall_decoration, door, doodad, carpet, table or raw."));
			}
			request.kind = kind;
		}
		const auto z = Integer(body, "z");
		if (auto error = ValidateFloor(z)) {
			return fail(std::move(*error));
		}
		request.z = *z;
		if (auto error = ReadBool(body, "automagic", request.automagic)) {
			return fail(std::move(*error));
		}

		std::unordered_set<uint64_t> seen;
		const auto add = [&](const Position& position) {
			if (seen.insert(ConnectedPositions::planarKey(position)).second) {
				request.positions.push_back(position);
			}
		};

		if (const auto area_field = body.find("area"); area_field != body.end()) {
			static constexpr std::array<std::string_view, 4> AreaFields { "x", "y", "width", "height" };
			if (auto error = RejectUnknownFields(*area_field, AreaFields, "paint area")) {
				return fail(std::move(*error));
			}
			const auto x = Integer(*area_field, "x");
			const auto y = Integer(*area_field, "y");
			const auto width = Integer(*area_field, "width");
			const auto height = Integer(*area_field, "height");
			if (!x || !y || !width || !height) {
				return fail(BadRequest("area needs integer x, y, width and height."));
			}
			const Area area { *x, *y, request.z, *width, *height };
			if (auto error = ValidateArea(area, mapWidth, mapHeight, static_cast<int64_t>(PaintTileLimit))) {
				return fail(std::move(*error));
			}
			for (int dy = 0; dy < area.height; ++dy) {
				for (int dx = 0; dx < area.width; ++dx) {
					add(Position(area.x + dx, area.y + dy, area.z));
				}
			}
		}

		if (const auto tiles = body.find("tiles"); tiles != body.end()) {
			if (!tiles->is_array()) {
				return fail(BadRequest("tiles must be an array of [x, y] pairs."));
			}
			if (tiles->size() > PaintTileLimit) {
				return fail(ApiError { 400, "paint_too_large", std::format("Paint at most {} tiles per call.", PaintTileLimit) });
			}
			for (const auto& tile : *tiles) {
				const auto x = tile.is_array() && tile.size() == 2 ? IntegerValue(tile[0]) : std::nullopt;
				const auto y = tile.is_array() && tile.size() == 2 ? IntegerValue(tile[1]) : std::nullopt;
				if (!x || !y) {
					return fail(BadRequest("Every tiles entry must be [x, y] with integers."));
				}
				if (*x < 0 || *y < 0 || *x >= mapWidth || *y >= mapHeight) {
					return fail(ApiError { 400, "area_outside_map", std::format("Tile [{}, {}] is outside the map.", *x, *y) });
				}
				add(Position(*x, *y, request.z));
			}
		}

		if (request.positions.empty()) {
			return fail(BadRequest("Paint needs area and/or tiles with at least one position."));
		}
		if (request.positions.size() > PaintTileLimit) {
			return fail(ApiError { 400, "paint_too_large", std::format("Paint at most {} tiles per call.", PaintTileLimit) });
		}
		return { std::nullopt, std::move(request) };
	}

	ClearAreaParseResult ParseClearAreaRequest(const nlohmann::json& body, int mapWidth, int mapHeight) {
		const auto fail = [](ApiError error) {
			return ClearAreaParseResult { std::move(error), {} };
		};
		static constexpr std::array<std::string_view, 8> ClearFields { "x", "y", "z", "width", "height", "keepGround", "keepFlags", "label" };
		if (auto error = RejectUnknownFields(body, ClearFields, "clear_area")) {
			return fail(std::move(*error));
		}
		ClearAreaRequest request;
		const auto area = ParseArea(body);
		if (!area) {
			return fail(BadRequest("Expected integer x, y, z, width and height."));
		}
		if (auto error = ValidateArea(*area, mapWidth, mapHeight, ClearAreaTileLimit)) {
			return fail(std::move(*error));
		}
		request.area = *area;
		if (auto error = ReadBool(body, "keepGround", request.keepGround)) {
			return fail(std::move(*error));
		}
		if (auto error = ReadBool(body, "keepFlags", request.keepFlags)) {
			return fail(std::move(*error));
		}
		request.label = "AI clear area";
		if (const auto label = body.find("label"); label != body.end()) {
			if (!label->is_string() || label->get<std::string>().empty() || label->get<std::string>().size() > PatchLabelLimit) {
				return fail(BadRequest(std::format("label must be a string of 1 to {} bytes.", PatchLabelLimit)));
			}
			request.label = label->get<std::string>();
		}
		return { std::nullopt, std::move(request) };
	}

	StepsParseResult ParseHistorySteps(const nlohmann::json& body) {
		static constexpr std::array<std::string_view, 1> StepFields { "steps" };
		if (auto error = RejectUnknownFields(body, StepFields, "history step")) {
			return { std::move(error), 1 };
		}
		if (!body.contains("steps")) {
			return { std::nullopt, 1 };
		}
		const auto steps = Integer(body, "steps");
		if (!steps || *steps < 1 || *steps > HistoryStepLimit) {
			return { BadRequest(std::format("steps must be an integer from 1 to {}.", HistoryStepLimit)), 1 };
		}
		return { std::nullopt, *steps };
	}

	BrushQueryParseResult ParseBrushQuery(const nlohmann::json& body) {
		const auto fail = [](ApiError error) {
			return BrushQueryParseResult { std::move(error), {} };
		};
		static constexpr std::array<std::string_view, 3> QueryFields { "q", "type", "limit" };
		if (auto error = RejectUnknownFields(body, QueryFields, "brushes")) {
			return fail(std::move(*error));
		}
		BrushQuery query;
		if (const auto needle = body.find("q"); needle != body.end()) {
			if (!needle->is_string()) {
				return fail(BadRequest("q must be a string."));
			}
			query.needle = ToLower(needle->get<std::string>());
		}
		if (const auto type = body.find("type"); type != body.end()) {
			query.kind = type->is_string() ? ParseBrushKind(type->get<std::string>()) : std::nullopt;
			if (!query.kind) {
				return fail(BadRequest("type must be ground, wall, wall_decoration, door, doodad, carpet, table, raw or other."));
			}
		}
		if (body.contains("limit")) {
			const auto limit = Integer(body, "limit");
			if (!limit || *limit < 1 || *limit > BrushListLimit) {
				return fail(BadRequest(std::format("limit must be an integer from 1 to {}.", BrushListLimit)));
			}
			query.limit = static_cast<size_t>(*limit);
		}
		return { std::nullopt, std::move(query) };
	}

	bool MatchesBrushQuery(const BrushQuery& query, std::string_view name, BrushKind kind) {
		if (query.kind && *query.kind != kind) {
			return false;
		}
		return query.needle.empty() || ToLower(name).find(query.needle) != std::string::npos;
	}

	std::vector<std::string> SuggestBrushNames(std::string_view wanted, std::span<const std::string> names, size_t limit) {
		const std::string needle = ToLower(wanted);
		const std::set<std::string> unique(names.begin(), names.end());
		std::vector<std::tuple<int, size_t, std::string>> scored;
		for (const std::string& name : unique) {
			const std::string lowered = ToLower(name);
			int score = 0;
			if (lowered == needle) {
				score = -2;
			} else if (lowered.find(needle) != std::string::npos || needle.find(lowered) != std::string::npos) {
				score = -1;
			} else {
				score = EditDistance(lowered, needle);
			}
			if (score <= 3) {
				scored.emplace_back(score, name.size(), name);
			}
		}
		std::ranges::sort(scored);
		std::vector<std::string> result;
		for (auto& entry : scored) {
			if (result.size() >= limit) {
				break;
			}
			result.push_back(std::move(std::get<2>(entry)));
		}
		return result;
	}

	ExportRegionParseResult ParseExportRegionRequest(const nlohmann::json& body, int mapWidth, int mapHeight) {
		const auto fail = [](ApiError error) {
			return ExportRegionParseResult { std::move(error), {} };
		};
		static constexpr std::array<std::string_view, 7> ExportFields { "minX", "minY", "maxX", "maxY", "z", "path", "overwrite" };
		if (auto error = RejectUnknownFields(body, ExportFields, "export_region")) {
			return fail(std::move(*error));
		}
		const auto min_x = Integer(body, "minX");
		const auto min_y = Integer(body, "minY");
		const auto max_x = Integer(body, "maxX");
		const auto max_y = Integer(body, "maxY");
		if (!min_x || !min_y || !max_x || !max_y) {
			return fail(BadRequest("Expected integer minX, minY, maxX and maxY."));
		}
		const auto z = Integer(body, "z");
		if (auto error = ValidateFloor(z)) {
			return fail(std::move(*error));
		}
		if (*min_x < 0 || *min_y < 0 || *min_x > *max_x || *min_y > *max_y) {
			return fail(BadRequest("Expected 0 <= minX <= maxX and 0 <= minY <= maxY."));
		}
		if (*max_x >= mapWidth || *max_y >= mapHeight) {
			return fail(ApiError { 400, "area_outside_map", "Region is outside the map bounds." });
		}
		const int64_t width = (static_cast<int64_t>(*max_x) - *min_x + 1) * TILE_SIZE;
		const int64_t height = (static_cast<int64_t>(*max_y) - *min_y + 1) * TILE_SIZE;
		if (width > ExportSideLimitPixels || height > ExportSideLimitPixels || width * height > ExportPixelLimit) {
			return fail(ApiError { 400, "region_too_large", std::format("The PNG may be at most {} pixels per side and {} pixels in total.", ExportSideLimitPixels, ExportPixelLimit) });
		}

		const auto path = body.find("path");
		if (path == body.end() || !path->is_string()) {
			return fail(BadRequest("Expected path: an absolute .png file path."));
		}
		const std::string text = path->get<std::string>();
		if (IsNetworkOrDevicePath(text)) {
			return fail(BadRequest("path must be a local file path; network (UNC) and device paths are not allowed."));
		}
		if (!IsAbsoluteUtf8Path(text) || !ToLower(text).ends_with(".png")) {
			return fail(BadRequest("path must be an absolute file path ending in .png."));
		}
		bool overwrite = false;
		if (const auto found = body.find("overwrite"); found != body.end()) {
			if (!found->is_boolean()) {
				return fail(BadRequest("overwrite must be a boolean."));
			}
			overwrite = found->get<bool>();
		}

		ExportRegionRequest request;
		request.region = PositionRect { *min_x, *min_y, *max_x, *max_y };
		request.z = *z;
		request.path = text;
		request.overwrite = overwrite;
		return { std::nullopt, std::move(request) };
	}

	std::optional<ApiError> PrevalidateBatch(const BatchPlan& plan, int mapWidth, int mapHeight) {
		for (size_t index = 0; index < plan.ops.size(); ++index) {
			const BatchOp& op = plan.ops[index];
			std::optional<ApiError> error;
			switch (op.route->id) {
				case RouteId::Paint:
					error = ParsePaintRequest(op.args, mapWidth, mapHeight).error;
					break;
				case RouteId::ClearArea:
					error = ParseClearAreaRequest(op.args, mapWidth, mapHeight).error;
					break;
				case RouteId::ApplyPatch:
					error = ParsePatchRequest(op.args, PatchMode::Apply, mapWidth, mapHeight).error;
					break;
				case RouteId::Undo:
				case RouteId::Redo:
					error = ParseHistorySteps(op.args).error;
					break;
				case RouteId::ExportRegion:
					error = ParseExportRegionRequest(op.args, mapWidth, mapHeight).error;
					break;
				default:
					break;
			}
			if (error) {
				error->message = std::format("Operation {} ({}): {}", index + 1, op.route->path.substr(1), error->message);
				return error;
			}
		}
		return std::nullopt;
	}

	RollbackPlan PlanBatchRollback(const HistoryCounters& before, const HistoryCounters& after, size_t currentIndex, bool historyMoved) {
		if (historyMoved) {
			return {};
		}
		const uint64_t pushed = after.pushed >= before.pushed ? after.pushed - before.pushed : 0;
		const bool merged = after.merged != before.merged;
		const size_t steps = static_cast<size_t>(std::min<uint64_t>(pushed, currentIndex));
		return { steps, !merged && pushed <= currentIndex };
	}

	BatchParseResult ParseBatchRequest(const nlohmann::json& body) {
		const auto fail = [](ApiError error) {
			return BatchParseResult { std::move(error), {} };
		};
		static constexpr std::array<std::string_view, 2> BatchFields { "ops", "rollbackOnError" };
		if (auto error = RejectUnknownFields(body, BatchFields, "batch")) {
			return fail(std::move(*error));
		}
		BatchPlan plan;
		if (auto error = ReadBool(body, "rollbackOnError", plan.rollbackOnError)) {
			return fail(std::move(*error));
		}
		const auto ops = body.find("ops");
		if (ops == body.end() || !ops->is_array() || ops->empty()) {
			return fail(BadRequest("ops must be a non-empty array of {\"op\": \"<route>\", ...} objects."));
		}
		if (ops->size() > BatchOpLimit) {
			return fail(ApiError { 400, "batch_too_large", std::format("A batch runs at most {} ops.", BatchOpLimit) });
		}

		plan.ops.reserve(ops->size());
		size_t number = 0;
		for (const auto& op : *ops) {
			++number;
			const auto name = op.is_object() ? op.find("op") : op.end();
			if (!op.is_object() || name == op.end() || !name->is_string() || name->get<std::string>().empty()) {
				return fail(ApiError { 400, "invalid_op", std::format("Operation {} needs a string op naming a route.", number) });
			}
			std::string path = name->get<std::string>();
			if (!path.starts_with('/')) {
				path.insert(path.begin(), '/');
			}
			const RouteSpec* route = FindRouteByPath(NormalizeRoutePath(path));
			if (!route) {
				return fail(ApiError { 400, "unknown_op", std::format("Operation {}: unknown op '{}'.", number, name->get<std::string>()) });
			}
			if (!route->batchable) {
				return fail(ApiError { 400, "invalid_op", std::format("Operation {}: '{}' cannot run inside a batch.", number, route->path.substr(1)) });
			}
			nlohmann::json args = op;
			args.erase("op");
			plan.ops.push_back({ route, std::move(args) });
		}
		return { std::nullopt, std::move(plan) };
	}

} // namespace AiControl
