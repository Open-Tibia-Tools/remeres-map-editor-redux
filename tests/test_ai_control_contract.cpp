#include "ai/ai_control_contract.h"
#include "editor/operations/brush_fill_operations.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

// The pure contract of the local AI Control API: HTTP framing and policy, routes, the busy
// gate and request parsing, plus the border targets of the paint helper. No editor, map or
// wxWidgets is needed for any of these cases.

using namespace AiControl;

namespace {

	constexpr uint16_t TestPort = 17310;

	std::string PostRequest(std::string_view path, std::string_view body, std::string_view extra_headers = "") {
		return std::string("POST ") + std::string(path) + " HTTP/1.1\r\nHost: 127.0.0.1:17310\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) + "\r\n" + std::string(extra_headers) + "\r\n" + std::string(body);
	}

	HttpRequest PolicyRequest(HttpMethod method, std::optional<std::string> host, std::optional<std::string> content_type = std::nullopt) {
		HttpRequest request;
		request.method = method;
		request.path = "/health";
		request.host = std::move(host);
		request.contentType = std::move(content_type);
		return request;
	}

	std::set<std::tuple<int, int, int>> KeysOf(const PositionVector& positions) {
		std::set<std::tuple<int, int, int>> keys;
		for (const Position& position : positions) {
			keys.emplace(position.x, position.y, position.z);
		}
		return keys;
	}

#ifdef _WIN32
	constexpr std::string_view AbsolutePng = "C:/exports/region.png";
#else
	constexpr std::string_view AbsolutePng = "/tmp/exports/region.png";
#endif

} // namespace

TEST_CASE("http parser frames a complete POST and strips the query", "[ai_control][http]") {
	const auto result = ParseHttpRequest(PostRequest("/ai/paint?debug=1", R"({"z":7})"));
	REQUIRE(result.state == HttpParseState::Complete);
	CHECK(result.request.method == HttpMethod::Post);
	CHECK(result.request.path == "/ai/paint");
	CHECK(result.request.host == "127.0.0.1:17310");
	CHECK(result.request.contentType == "application/json");
	CHECK_FALSE(result.request.hasOrigin);
	CHECK(result.request.body == R"({"z":7})");
}

TEST_CASE("http parser waits for missing headers and body bytes", "[ai_control][http]") {
	CHECK(ParseHttpRequest("GET /health HTTP/1.1\r\nHost: 127.0.0.1:17310\r\n").state == HttpParseState::Incomplete);
	const std::string full = PostRequest("/paint", R"({"brush":"grass"})");
	CHECK(ParseHttpRequest(std::string_view(full).substr(0, full.size() - 3)).state == HttpParseState::Incomplete);
	CHECK(ParseHttpRequest(full).state == HttpParseState::Complete);
}

TEST_CASE("http parser enforces header and body limits", "[ai_control][http]") {
	const std::string endless_headers = "GET /health HTTP/1.1\r\nX-Fill: " + std::string(MaxHeaderBytes, 'a');
	const auto headers = ParseHttpRequest(endless_headers);
	REQUIRE(headers.state == HttpParseState::Rejected);
	CHECK(headers.rejection->status == 431);

	const auto body = ParseHttpRequest("POST /paint HTTP/1.1\r\nHost: 127.0.0.1:17310\r\nContent-Length: " + std::to_string(MaxBodyBytes + 1) + "\r\n\r\n");
	REQUIRE(body.state == HttpParseState::Rejected);
	CHECK(body.rejection->status == 413);
}

TEST_CASE("http parser rejects unsupported methods, versions and framing", "[ai_control][http]") {
	const auto status_of = [](std::string_view raw) {
		const auto result = ParseHttpRequest(raw);
		REQUIRE(result.state == HttpParseState::Rejected);
		return result.rejection->status;
	};
	CHECK(status_of("DELETE /health HTTP/1.1\r\nHost: 127.0.0.1:17310\r\n\r\n") == 405);
	CHECK(status_of("GET /health HTTP/2.0\r\nHost: 127.0.0.1:17310\r\n\r\n") == 400);
	CHECK(status_of("GET http://127.0.0.1:17310/health HTTP/1.1\r\n\r\n") == 400);
	CHECK(status_of("GET  /health HTTP/1.1\r\n\r\n") == 400);
	CHECK(status_of("POST /paint HTTP/1.1\r\nContent-Length: 2\r\nContent-Length: 2\r\n\r\n{}") == 400);
	CHECK(status_of("POST /paint HTTP/1.1\r\nContent-Length: 2x\r\n\r\n{}") == 400);
	CHECK(status_of("POST /paint HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n") == 400);
	CHECK(status_of("GET /health HTTP/1.1\r\nHost: a\r\nHost: b\r\n\r\n") == 400);
	CHECK(status_of("GET /health HTTP/1.1\r\nBroken header\r\n\r\n") == 400);
	CHECK(status_of("GET /health HTTP/1.1\r\nHost: a\r\n folded\r\n\r\n") == 400);
}

TEST_CASE("http parser notices an Origin header", "[ai_control][http]") {
	const auto result = ParseHttpRequest("GET /health HTTP/1.1\r\nHost: 127.0.0.1:17310\r\norigin: http://evil.example\r\n\r\n");
	REQUIRE(result.state == HttpParseState::Complete);
	CHECK(result.request.hasOrigin);
}

TEST_CASE("request policy admits only local JSON requests without Origin", "[ai_control][policy]") {
	CHECK_FALSE(CheckRequestPolicy(PolicyRequest(HttpMethod::Get, "127.0.0.1:17310"), TestPort));
	CHECK_FALSE(CheckRequestPolicy(PolicyRequest(HttpMethod::Get, "LOCALHOST:17310"), TestPort));
	CHECK_FALSE(CheckRequestPolicy(PolicyRequest(HttpMethod::Post, "localhost:17310", "application/json; charset=utf-8"), TestPort));

	CHECK(CheckRequestPolicy(PolicyRequest(HttpMethod::Get, std::nullopt), TestPort)->status == 403);
	CHECK(CheckRequestPolicy(PolicyRequest(HttpMethod::Get, "127.0.0.1"), TestPort)->status == 403);
	CHECK(CheckRequestPolicy(PolicyRequest(HttpMethod::Get, "127.0.0.1:17311"), TestPort)->status == 403);
	CHECK(CheckRequestPolicy(PolicyRequest(HttpMethod::Get, "attacker.example:17310"), TestPort)->status == 403);

	auto with_origin = PolicyRequest(HttpMethod::Get, "127.0.0.1:17310");
	with_origin.hasOrigin = true;
	CHECK(CheckRequestPolicy(with_origin, TestPort)->code == "origin_forbidden");

	CHECK(CheckRequestPolicy(PolicyRequest(HttpMethod::Post, "127.0.0.1:17310"), TestPort)->status == 415);
	CHECK(CheckRequestPolicy(PolicyRequest(HttpMethod::Post, "127.0.0.1:17310", "text/plain"), TestPort)->status == 415);
	CHECK(CheckRequestPolicy(PolicyRequest(HttpMethod::Post, "127.0.0.1:17310", "application/x-www-form-urlencoded"), TestPort)->status == 415);
}

TEST_CASE("json body parsing accepts objects only and bounds nesting", "[ai_control][http]") {
	CHECK(ParseJsonBody("").body == nlohmann::json::object());
	CHECK(ParseJsonBody("  ").body == nlohmann::json::object());
	CHECK(ParseJsonBody(R"({"x":1})").body.at("x") == 1);
	CHECK(ParseJsonBody("[1,2]").error->code == "bad_request");
	CHECK(ParseJsonBody("{\"x\":").error->code == "invalid_json");
	CHECK(ParseJsonBody(std::string(65, '[') + std::string(65, ']')).error->code == "json_too_deep");
	CHECK_FALSE(ParseJsonBody(R"({"text":")" + std::string(100, '[') + R"("})").error);
}

TEST_CASE("responses are framed with Content-Length and Connection close", "[ai_control][http]") {
	const std::string response = BuildHttpResponse(503, R"({"ok":false})");
	CHECK(response.starts_with("HTTP/1.1 503 Service Unavailable\r\n"));
	CHECK(response.find("Content-Length: 12\r\n") != std::string::npos);
	CHECK(response.find("Connection: close\r\n") != std::string::npos);
	CHECK(response.ends_with("\r\n\r\n{\"ok\":false}"));
	CHECK(ReasonPhrase(431) == "Request Header Fields Too Large");
}

TEST_CASE("json serialization survives invalid UTF-8 names", "[ai_control][http]") {
	const nlohmann::json value { { "name", std::string("caf\xE9") } };
	std::string text;
	REQUIRE_NOTHROW(text = SerializeJson(value));
	CHECK(text.find("caf") != std::string::npos);
}

TEST_CASE("port setting accepts 1024 to 65535", "[ai_control]") {
	CHECK(ParsePort("17310") == 17310);
	CHECK(ParsePort(" 1024 ") == 1024);
	CHECK(ParsePort("65535") == 65535);
	CHECK_FALSE(ParsePort("1023"));
	CHECK_FALSE(ParsePort("65536"));
	CHECK_FALSE(ParsePort("80a"));
	CHECK_FALSE(ParsePort(""));
}

TEST_CASE("routes resolve with the /ai alias, 404 and 405", "[ai_control][routes]") {
	const auto health = LookupRoute(HttpMethod::Get, "/health");
	REQUIRE(health.route);
	CHECK(health.route->id == RouteId::Health);

	const auto alias = LookupRoute(HttpMethod::Post, "/ai/paint");
	REQUIRE(alias.route);
	CHECK(alias.route->id == RouteId::Paint);

	CHECK(LookupRoute(HttpMethod::Get, "/set_ground_zorder").error->status == 404);
	CHECK(LookupRoute(HttpMethod::Get, "/ai").error->status == 404);
	CHECK(LookupRoute(HttpMethod::Get, "/paint").error->status == 405);
	CHECK(LookupRoute(HttpMethod::Post, "/history").error->status == 405);
}

TEST_CASE("route table is unique and capabilities list every route", "[ai_control][routes]") {
	std::set<std::string_view> paths;
	for (const RouteSpec& route : Routes()) {
		CHECK(paths.insert(route.path).second);
		CHECK(&RouteFor(route.id) == &route);
	}
	CHECK(paths.size() == 16);

	const nlohmann::json capabilities = CapabilitiesJson();
	REQUIRE(capabilities.at("endpoints").size() == Routes().size());
	for (const auto& endpoint : capabilities.at("endpoints")) {
		const auto match = LookupRoute(endpoint.at("method") == "POST" ? HttpMethod::Post : HttpMethod::Get, endpoint.at("path").get<std::string>());
		CHECK(match.route);
	}
	CHECK(capabilities.at("limits").at("batchOps") == BatchOpLimit);
	CHECK_FALSE(RouteFor(RouteId::Batch).batchable);
	CHECK(RouteFor(RouteId::Paint).kind == RouteKind::Mutation);
	CHECK(RouteFor(RouteId::Health).kind == RouteKind::Status);
}

TEST_CASE("busy gate blocks everything but status routes", "[ai_control][gate]") {
	const RouteSpec& health = RouteFor(RouteId::Health);
	const RouteSpec& capabilities = RouteFor(RouteId::Capabilities);
	const RouteSpec& snapshot = RouteFor(RouteId::Snapshot);
	const RouteSpec& paint = RouteFor(RouteId::Paint);

	const EditorActivity idle;
	CHECK_FALSE(CheckBusyGate(snapshot, idle));
	CHECK_FALSE(CheckBusyGate(paint, idle));

	for (const EditorActivity busy : { EditorActivity { .loading = true }, EditorActivity { .renderingDisabled = true }, EditorActivity { .modalDialogOpen = true }, EditorActivity { .requestInProgress = true } }) {
		CHECK_FALSE(CheckBusyGate(health, busy));
		CHECK_FALSE(CheckBusyGate(capabilities, busy));
		REQUIRE(CheckBusyGate(snapshot, busy));
		CHECK(CheckBusyGate(snapshot, busy)->status == 503);
		CHECK(CheckBusyGate(paint, busy)->code == "busy");
	}
}

TEST_CASE("busy gate holds mutations and export back while the user interacts", "[ai_control][gate]") {
	const EditorActivity interacting { .userInteracting = true };
	CHECK_FALSE(CheckBusyGate(RouteFor(RouteId::Health), interacting));
	CHECK_FALSE(CheckBusyGate(RouteFor(RouteId::Snapshot), interacting));
	CHECK_FALSE(CheckBusyGate(RouteFor(RouteId::InspectTile), interacting));
	CHECK_FALSE(CheckBusyGate(RouteFor(RouteId::PreviewPatch), interacting));
	for (const RouteId id : { RouteId::Paint, RouteId::ClearArea, RouteId::ApplyPatch, RouteId::Undo, RouteId::Redo, RouteId::Batch, RouteId::ExportRegion }) {
		const auto error = CheckBusyGate(RouteFor(id), interacting);
		REQUIRE(error);
		CHECK(error->status == 503);
		CHECK(error->code == "busy");
	}
}

TEST_CASE("areas are parsed and validated against the map and the limit", "[ai_control][area]") {
	const auto area = ParseArea({ { "x", 10 }, { "y", 20 }, { "z", 7 }, { "width", 3 }, { "height", 2 } });
	REQUIRE(area);
	CHECK_FALSE(ValidateArea(*area, 100, 100, 100));
	CHECK(ValidateArea({ -1, 20, 7, 3, 2 }, 100, 100, 100)->code == "bad_request");
	CHECK(ValidateArea({ 1, 1, 16, 1, 1 }, 100, 100, 100)->code == "bad_request");
	CHECK(ValidateArea({ 99, 99, 7, 2, 2 }, 100, 100, 100)->code == "area_outside_map");
	CHECK(ValidateArea({ 0, 0, 7, 11, 10 }, 100, 100, 100)->code == "area_too_large");
	CHECK_FALSE(ParseArea({ { "x", 1 }, { "y", 1 }, { "z", 7 }, { "width", 1 } }));
	CHECK_FALSE(Integer({ { "x", 4294967296LL } }, "x"));
	CHECK_FALSE(Integer({ { "x", 1.5 } }, "x"));
	CHECK(Integer({ { "x", -3 } }, "x") == -3);
}

TEST_CASE("unknown fields are rejected by name", "[ai_control][area]") {
	static constexpr std::array<std::string_view, 2> Allowed { "x", "y" };
	CHECK_FALSE(RejectUnknownFields({ { "x", 1 } }, Allowed, "test"));
	const auto error = RejectUnknownFields({ { "x", 1 }, { "zOrder", "higher" } }, Allowed, "test");
	REQUIRE(error);
	CHECK(error->message.find("zOrder") != std::string::npos);
	CHECK(RejectUnknownFields(nlohmann::json::array(), Allowed, "test"));
}

TEST_CASE("terrain grid encodes deterministic symbols and rows", "[ai_control][terrain]") {
	const std::vector<std::optional<uint16_t>> cells = { 10, 10, 20, std::nullopt, 20, 30 };
	const TerrainGrid grid = EncodeTerrainGrid(3, 2, cells);
	CHECK(grid.foundTiles == 5);
	CHECK(grid.emptyTiles == 1);
	REQUIRE(grid.legend.size() == 3);
	CHECK(grid.legend[0].itemId == 10);
	CHECK(grid.legend[0].symbol == 'a');
	CHECK(grid.legend[1].itemId == 20);
	CHECK(grid.legend[1].symbol == 'b');
	CHECK(grid.rows == std::vector<std::string> { "aab", ".bc" });

	std::vector<std::optional<uint16_t>> many;
	for (uint16_t id = 1; id <= 63; ++id) {
		many.emplace_back(id);
	}
	const TerrainGrid overflow = EncodeTerrainGrid(63, 1, many);
	CHECK(overflow.legend.size() == 62);
	CHECK(overflow.overflow.size() == 1);
	CHECK(overflow.rows.front().back() == '#');
	CHECK(EncodeTerrainGrid(2, 2, cells).rows.empty());
}

TEST_CASE("patch requests are parsed with limits and token rules", "[ai_control][patch]") {
	const nlohmann::json body {
		{ "label", "generated path" },
		{ "tiles", { { { "x", 10 }, { "y", 20 }, { "z", 7 }, { "groundId", 102 }, { "addItemIds", { 271, 272 } } }, { { "x", 11 }, { "y", 20 }, { "z", 7 }, { "addItemIds", { 273 } } } } },
	};
	const auto patch = ParsePatchRequest(body, PatchMode::Preview, 100, 100);
	REQUIRE_FALSE(patch.error);
	CHECK(patch.request.label == "generated path");
	REQUIRE(patch.request.tiles.size() == 2);
	CHECK(patch.request.tiles[0].position == Position(10, 20, 7));
	CHECK(patch.request.tiles[0].groundId == 102);
	CHECK(patch.request.addedItemCount == 3);

	const auto replace = ParsePatchRequest({ { "tiles", { { { "x", 12 }, { "y", 20 }, { "z", 7 }, { "replaceItems", true } } } } }, PatchMode::Preview, 100, 100);
	REQUIRE_FALSE(replace.error);
	CHECK(replace.request.tiles[0].replaceItems);

	CHECK(ParsePatchRequest({ { "tiles", { { { "x", 12 }, { "y", 20 }, { "z", 7 }, { "replaceItems", 1 } } } } }, PatchMode::Preview, 100, 100).error->code == "bad_request");
	CHECK(ParsePatchRequest({ { "tiles", { { { "x", 1 }, { "y", 1 }, { "z", 7 } } } } }, PatchMode::Preview, 100, 100).error);
	CHECK(ParsePatchRequest({ { "tiles", { { { "x", 1 }, { "y", 1 }, { "z", 7 }, { "groundId", 0 } } } } }, PatchMode::Preview, 100, 100).error);
	CHECK(ParsePatchRequest({ { "tiles", { { { "x", 1 }, { "y", 1 }, { "z", 7 }, { "groundId", 65536 } } } } }, PatchMode::Preview, 100, 100).error);
	CHECK(ParsePatchRequest({ { "tiles", { { { "x", 1 }, { "y", 1 }, { "z", 7 }, { "groundId", 1 }, { "zOrder", "higher" } } } } }, PatchMode::Preview, 100, 100).error);
	CHECK(ParsePatchRequest({ { "tiles", { { { "x", 100 }, { "y", 1 }, { "z", 7 }, { "groundId", 1 } } } } }, PatchMode::Preview, 100, 100).error->code == "area_outside_map");
	CHECK(ParsePatchRequest({ { "tiles", nlohmann::json::array() } }, PatchMode::Preview, 100, 100).error);

	const nlohmann::json duplicate { { "tiles", { { { "x", 10 }, { "y", 20 }, { "z", 7 }, { "groundId", 102 } }, { { "x", 10 }, { "y", 20 }, { "z", 7 }, { "groundId", 103 } } } } };
	CHECK(ParsePatchRequest(duplicate, PatchMode::Preview, 100, 100).error->code == "duplicate_tile");

	const nlohmann::json one_tile { { "x", 1 }, { "y", 1 }, { "z", 7 }, { "groundId", 102 } };
	CHECK(ParsePatchRequest({ { "tiles", { one_tile } } }, PatchMode::Apply, 100, 100).error);
	CHECK(ParsePatchRequest({ { "previewToken", "E71FA2190541574B" }, { "tiles", { one_tile } } }, PatchMode::Apply, 100, 100).error);
	CHECK_FALSE(ParsePatchRequest({ { "previewToken", "e71fa2190541574b" }, { "tiles", { one_tile } } }, PatchMode::Apply, 100, 100).error);
	CHECK(ParsePatchRequest({ { "previewToken", "e71fa2190541574b" }, { "tiles", { one_tile } } }, PatchMode::Preview, 100, 100).error);

	nlohmann::json too_many = nlohmann::json::array();
	for (int index = 0; index <= static_cast<int>(PatchTileLimit); ++index) {
		too_many.push_back({ { "x", index % 100 }, { "y", index / 100 }, { "z", 7 }, { "groundId", 1 } });
	}
	CHECK(ParsePatchRequest({ { "tiles", too_many } }, PatchMode::Preview, 100, 100).error->code == "patch_too_large");

	const nlohmann::json heavy { { "x", 1 }, { "y", 1 }, { "z", 7 }, { "addItemIds", std::vector<int>(PatchAddedItemLimit + 1, 271) } };
	CHECK(ParsePatchRequest({ { "tiles", { heavy } } }, PatchMode::Preview, 100, 100).error->code == "patch_too_large");
}

TEST_CASE("stable token is deterministic FNV-1a", "[ai_control][patch]") {
	CHECK(StableToken("abc") == "e71fa2190541574b");
	CHECK(StableToken("abc") == StableToken("abc"));
	CHECK(StableToken("abd") != StableToken("abc"));
	CHECK(StableToken("").size() == 16);
}

TEST_CASE("paint requests collect deduplicated positions", "[ai_control][paint]") {
	const nlohmann::json body {
		{ "brush", "grass" },
		{ "type", "ground" },
		{ "z", 7 },
		{ "area", { { "x", 10 }, { "y", 10 }, { "width", 2 }, { "height", 2 } } },
		{ "tiles", { { 11, 11 }, { 20, 20 }, { 20, 20 } } },
		{ "automagic", false },
	};
	const auto paint = ParsePaintRequest(body, 100, 100);
	REQUIRE_FALSE(paint.error);
	CHECK(paint.request.brushName == "grass");
	CHECK(paint.request.kind == BrushKind::Ground);
	CHECK_FALSE(paint.request.automagic);
	CHECK(paint.request.positions == PositionVector { Position(10, 10, 7), Position(11, 10, 7), Position(10, 11, 7), Position(11, 11, 7), Position(20, 20, 7) });
}

TEST_CASE("paint requests reject unsupported or invalid input", "[ai_control][paint]") {
	const auto error_of = [](const nlohmann::json& body) {
		return ParsePaintRequest(body, 100, 100).error;
	};
	CHECK_FALSE(error_of({ { "brush", "grass" }, { "z", 7 }, { "tiles", { { 1, 1 } } } }));
	CHECK(error_of({ { "z", 7 }, { "tiles", { { 1, 1 } } } }));
	CHECK(error_of({ { "brush", "grass" }, { "tiles", { { 1, 1 } } } }));
	CHECK(error_of({ { "brush", "grass" }, { "z", 16 }, { "tiles", { { 1, 1 } } } }));
	CHECK(error_of({ { "brush", "grass" }, { "z", 7 } }));
	CHECK(error_of({ { "brush", "rat" }, { "creature", "rat" }, { "z", 7 }, { "tiles", { { 1, 1 } } } }));
	CHECK(error_of({ { "brush", "grass" }, { "zOrder", "higher" }, { "z", 7 }, { "tiles", { { 1, 1 } } } }));
	CHECK(error_of({ { "brush", "grass" }, { "type", "other" }, { "z", 7 }, { "tiles", { { 1, 1 } } } }));
	CHECK(error_of({ { "brush", "grass" }, { "type", "spawn" }, { "z", 7 }, { "tiles", { { 1, 1 } } } }));
	CHECK(error_of({ { "brush", "grass" }, { "automagic", 1 }, { "z", 7 }, { "tiles", { { 1, 1 } } } }));
	CHECK(error_of({ { "brush", "grass" }, { "z", 7 }, { "tiles", { { 100, 1 } } } })->code == "area_outside_map");
	CHECK(error_of({ { "brush", "grass" }, { "z", 7 }, { "tiles", { { 1 } } } }));
	CHECK(error_of({ { "brush", "grass" }, { "z", 7 }, { "area", { { "x", 0 }, { "y", 0 }, { "z", 7 }, { "width", 1 }, { "height", 1 } } } }));
	CHECK(ParsePaintRequest({ { "brush", "grass" }, { "z", 7 }, { "area", { { "x", 0 }, { "y", 0 }, { "width", 200 }, { "height", 200 } } } }, 1000, 1000).error->code == "area_too_large");
}

TEST_CASE("paintable brush kinds exclude markers and creatures", "[ai_control][paint]") {
	for (const BrushKind kind : { BrushKind::Ground, BrushKind::Wall, BrushKind::WallDecoration, BrushKind::Door, BrushKind::Doodad, BrushKind::Carpet, BrushKind::Table, BrushKind::Raw }) {
		CHECK(IsPaintableBrushKind(kind));
		CHECK(ParseBrushKind(BrushKindName(kind)) == kind);
	}
	CHECK_FALSE(IsPaintableBrushKind(BrushKind::Other));
	CHECK_FALSE(ParseBrushKind("creature"));
}

TEST_CASE("border targets are the patch plus its ring inside the map", "[ai_control][paint]") {
	const PositionRect map { 0, 0, 99, 99 };
	const PositionVector single { Position(10, 10, 7) };
	const PositionVector targets = BrushFillOperations::borderTargets(single, map);
	REQUIRE(targets.size() == 9);
	CHECK(targets.front() == Position(10, 10, 7));
	CHECK(KeysOf(targets).size() == 9);

	const PositionVector pair { Position(10, 10, 7), Position(11, 10, 7) };
	CHECK(BrushFillOperations::borderTargets(pair, map).size() == 12);

	const PositionVector corner { Position(0, 0, 7) };
	CHECK(KeysOf(BrushFillOperations::borderTargets(corner, map)) == std::set<std::tuple<int, int, int>> { { 0, 0, 7 }, { 1, 0, 7 }, { 0, 1, 7 }, { 1, 1, 7 } });
}

TEST_CASE("clear area requests default to removing everything", "[ai_control][clear]") {
	const auto clear = ParseClearAreaRequest({ { "x", 1 }, { "y", 2 }, { "z", 7 }, { "width", 3 }, { "height", 4 } }, 100, 100);
	REQUIRE_FALSE(clear.error);
	CHECK_FALSE(clear.request.keepGround);
	CHECK_FALSE(clear.request.keepFlags);
	CHECK(clear.request.label == "AI clear area");
	CHECK(clear.request.area.width == 3);

	const auto keep = ParseClearAreaRequest({ { "x", 1 }, { "y", 2 }, { "z", 7 }, { "width", 3 }, { "height", 4 }, { "keepGround", true }, { "label", "reset room" } }, 100, 100);
	REQUIRE_FALSE(keep.error);
	CHECK(keep.request.keepGround);
	CHECK(keep.request.label == "reset room");

	CHECK(ParseClearAreaRequest({ { "x", 1 }, { "y", 2 }, { "z", 7 }, { "width", 3 }, { "height", 4 }, { "keepGround", "yes" } }, 100, 100).error);
	CHECK(ParseClearAreaRequest({ { "x", 1 }, { "y", 2 }, { "z", 7 }, { "width", 3 }, { "height", 4 }, { "keepCreatures", true } }, 100, 100).error);
	CHECK(ParseClearAreaRequest({ { "x", 0 }, { "y", 0 }, { "z", 7 }, { "width", 300 }, { "height", 300 } }, 1000, 1000).error->code == "area_too_large");
}

TEST_CASE("history steps default to one and are bounded", "[ai_control][history]") {
	CHECK(ParseHistorySteps(nlohmann::json::object()).steps == 1);
	CHECK(ParseHistorySteps({ { "steps", 5 } }).steps == 5);
	CHECK(ParseHistorySteps({ { "steps", 0 } }).error);
	CHECK(ParseHistorySteps({ { "steps", HistoryStepLimit + 1 } }).error);
	CHECK(ParseHistorySteps({ { "steps", "2" } }).error);
	CHECK(ParseHistorySteps({ { "count", 2 } }).error);
}

TEST_CASE("brush queries filter by name and kind", "[ai_control][brushes]") {
	const auto query = ParseBrushQuery({ { "q", "GRASS" }, { "type", "ground" }, { "limit", 10 } });
	REQUIRE_FALSE(query.error);
	CHECK(query.query.limit == 10);
	CHECK(MatchesBrushQuery(query.query, "Dark Grass", BrushKind::Ground));
	CHECK_FALSE(MatchesBrushQuery(query.query, "Dark Grass", BrushKind::Doodad));
	CHECK_FALSE(MatchesBrushQuery(query.query, "sand", BrushKind::Ground));
	CHECK(MatchesBrushQuery(ParseBrushQuery(nlohmann::json::object()).query, "anything", BrushKind::Other));
	CHECK(ParseBrushQuery({ { "limit", 0 } }).error);
	CHECK(ParseBrushQuery({ { "type", "creature" } }).error);
	CHECK(ParseBrushQuery({ { "borderVariant", "x" } }).error);
}

TEST_CASE("brush suggestions rank exact, substring and close names", "[ai_control][brushes]") {
	const std::vector<std::string> names { "grass", "dark grass", "grass", "sand", "grss", "stone wall", "lava" };
	const auto suggestions = SuggestBrushNames("Grass", names);
	REQUIRE(suggestions.size() >= 3);
	CHECK(suggestions[0] == "grass");
	CHECK(suggestions[1] == "dark grass");
	CHECK(std::ranges::find(suggestions, std::string("grss")) != suggestions.end());
	CHECK(std::ranges::find(suggestions, std::string("stone wall")) == suggestions.end());
	CHECK(SuggestBrushNames("grass", names, 1).size() == 1);
}

TEST_CASE("export region requests need an absolute png path inside the pixel limit", "[ai_control][export]") {
	const nlohmann::json body { { "minX", 10 }, { "minY", 20 }, { "maxX", 19 }, { "maxY", 29 }, { "z", 7 }, { "path", AbsolutePng } };
	const auto valid = ParseExportRegionRequest(body, 1000, 1000);
	REQUIRE_FALSE(valid.error);
	CHECK(valid.request.region.min_x == 10);
	CHECK(valid.request.region.max_y == 29);
	CHECK(valid.request.z == 7);

	auto with = [&body](std::string_view key, nlohmann::json value) {
		nlohmann::json changed = body;
		changed[std::string(key)] = std::move(value);
		return ParseExportRegionRequest(changed, 1000, 1000).error;
	};
	CHECK(with("path", "relative/region.png"));
	CHECK(with("path", std::string(AbsolutePng.substr(0, AbsolutePng.size() - 4)) + ".jpg"));
	CHECK_FALSE(with("path", std::string(AbsolutePng.substr(0, AbsolutePng.size() - 4)) + ".PNG"));
	CHECK(with("scale", 0.5));
	CHECK(with("overlays", nlohmann::json::array({ "blocking" })));
	CHECK(with("maxX", 9));
	CHECK(with("z", 16));
	CHECK(with("maxX", 1000)->code == "area_outside_map");
	CHECK(with("maxX", 10 + ExportSideLimitPixels / TILE_SIZE)->code == "region_too_large");
}

TEST_CASE("export region rejects network and device paths and parses overwrite", "[ai_control][export]") {
	const nlohmann::json body { { "minX", 10 }, { "minY", 20 }, { "maxX", 19 }, { "maxY", 29 }, { "z", 7 }, { "path", AbsolutePng } };
	CHECK_FALSE(ParseExportRegionRequest(body, 1000, 1000).request.overwrite);

	auto with = [&body](std::string_view key, nlohmann::json value) {
		nlohmann::json changed = body;
		changed[std::string(key)] = std::move(value);
		return ParseExportRegionRequest(changed, 1000, 1000);
	};
	for (const std::string_view path : { R"(\\server\share\region.png)", "//server/share/region.png", R"(\\?\C:\exports\region.png)", R"(\\.\pipe\region.png)", R"(/\server\share\region.png)" }) {
		const auto parsed = with("path", std::string(path));
		REQUIRE(parsed.error);
		CHECK(parsed.error->message.find("network") != std::string::npos);
	}
	CHECK(with("overwrite", true).request.overwrite);
	CHECK_FALSE(with("overwrite", false).error);
	CHECK(with("overwrite", "yes").error);
	CHECK(with("overwrite", 1).error);
}

TEST_CASE("batch rollback undoes exactly the entries the batch pushed", "[ai_control][batch]") {
	// Plain case: three new entries, all still in history.
	auto plan = PlanBatchRollback({ 10, 2 }, { 13, 2 }, 40, false);
	CHECK(plan.undoSteps == 3);
	CHECK(plan.complete);

	// History full: the oldest entries were dropped, so the index barely moved, but the batch's
	// own three entries are still the newest ones and are what gets undone.
	plan = PlanBatchRollback({ 500, 0 }, { 503, 0 }, 500, false);
	CHECK(plan.undoSteps == 3);
	CHECK(plan.complete);

	// Nothing pushed: nothing to undo, and the start is already restored.
	plan = PlanBatchRollback({ 7, 1 }, { 7, 1 }, 12, false);
	CHECK(plan.undoSteps == 0);
	CHECK(plan.complete);

	// A tiny history dropped some of the batch's own entries.
	plan = PlanBatchRollback({ 0, 0 }, { 5, 0 }, 2, false);
	CHECK(plan.undoSteps == 2);
	CHECK_FALSE(plan.complete);

	// A change merged into an older entry cannot be undone on its own.
	plan = PlanBatchRollback({ 4, 0 }, { 5, 1 }, 20, false);
	CHECK(plan.undoSteps == 1);
	CHECK_FALSE(plan.complete);

	// An undo or redo op inside the batch: undoing could hit the user's own entries.
	plan = PlanBatchRollback({ 4, 0 }, { 6, 0 }, 20, true);
	CHECK(plan.undoSteps == 0);
	CHECK_FALSE(plan.complete);
}

TEST_CASE("batch requests check op names, nesting and the op limit", "[ai_control][batch]") {
	const nlohmann::json body {
		{ "ops", { { { "op", "paint" }, { "brush", "grass" } }, { { "op", "/undo" } }, { { "op", "ai/history" } } } },
		{ "rollbackOnError", false },
	};
	const auto batch = ParseBatchRequest(body);
	REQUIRE_FALSE(batch.error);
	CHECK_FALSE(batch.plan.rollbackOnError);
	REQUIRE(batch.plan.ops.size() == 3);
	CHECK(batch.plan.ops[0].route->id == RouteId::Paint);
	CHECK(batch.plan.ops[0].args == nlohmann::json { { "brush", "grass" } });
	CHECK(batch.plan.ops[1].route->id == RouteId::Undo);
	CHECK(batch.plan.ops[2].route->id == RouteId::History);
	CHECK(ParseBatchRequest({ { "ops", { { { "op", "undo" } } } } }).plan.rollbackOnError);

	CHECK(ParseBatchRequest({ { "ops", { { { "op", "batch" } } } } }).error->code == "invalid_op");
	CHECK(ParseBatchRequest({ { "ops", { { { "op", "health" } } } } }).error->code == "invalid_op");
	CHECK(ParseBatchRequest({ { "ops", { { { "op", "checkpoint" } } } } }).error->code == "unknown_op");
	CHECK(ParseBatchRequest({ { "ops", { { { "brush", "grass" } } } } }).error->code == "invalid_op");
	CHECK(ParseBatchRequest({ { "ops", nlohmann::json::array() } }).error);
	CHECK(ParseBatchRequest({ { "ops", { { { "op", "undo" } } } }, { "checkpoint", "x" } }).error);

	nlohmann::json too_many = nlohmann::json::array();
	for (size_t index = 0; index <= BatchOpLimit; ++index) {
		too_many.push_back({ { "op", "undo" } });
	}
	CHECK(ParseBatchRequest({ { "ops", too_many } }).error->code == "batch_too_large");
}

TEST_CASE("json nesting ignores brackets behind escaped quotes", "[ai_control][http]") {
	// The escaped quote does not end the string, so the brackets after it stay text.
	CHECK_FALSE(ParseJsonBody("{\"a\":\"\\\"" + std::string(100, '[') + "\"}").error);
	// An escaped backslash does end the escape: the quote after it closes the string.
	const std::string after_backslash = R"({"a":"\\","b":)" + std::string(65, '[') + std::string(65, ']') + "}";
	CHECK(ParseJsonBody(after_backslash).error->code == "json_too_deep");
	CHECK_FALSE(ParseJsonBody("{\"a\":" + std::string(63, '[') + std::string(63, ']') + "}").error);
}

TEST_CASE("http parser accepts headers exactly at the limit", "[ai_control][http]") {
	const std::string prefix = "GET /health HTTP/1.1\r\nHost: 127.0.0.1:17310\r\nX-Fill: ";
	const auto request_with_headers = [&prefix](size_t header_bytes) {
		// header_bytes counts everything up to and including the blank line.
		return prefix + std::string(header_bytes - prefix.size() - 4, 'a') + "\r\n\r\n";
	};
	CHECK(ParseHttpRequest(request_with_headers(MaxHeaderBytes)).state == HttpParseState::Complete);
	const auto over = ParseHttpRequest(request_with_headers(MaxHeaderBytes + 1));
	REQUIRE(over.state == HttpParseState::Rejected);
	CHECK(over.rejection->status == 431);
}

TEST_CASE("HTTP/1.0 without Host parses and is refused by the policy", "[ai_control][http][policy]") {
	const auto parsed = ParseHttpRequest("GET /health HTTP/1.0\r\n\r\n");
	REQUIRE(parsed.state == HttpParseState::Complete);
	CHECK_FALSE(parsed.request.host);
	const auto error = CheckRequestPolicy(parsed.request, TestPort);
	REQUIRE(error);
	CHECK(error->status == 403);
	CHECK(error->code == "host_forbidden");
}

TEST_CASE("bytes after Content-Length are not part of the body", "[ai_control][http]") {
	const std::string request = PostRequest("/paint", R"({"z":7})") + "GET /health HTTP/1.1\r\n\r\n";
	const auto parsed = ParseHttpRequest(request);
	REQUIRE(parsed.state == HttpParseState::Complete);
	CHECK(parsed.request.body == R"({"z":7})");
	CHECK(parsed.request.path == "/paint");
}

TEST_CASE("paint limit counts area and tiles together", "[ai_control][paint]") {
	// 200 x 100 = 20000 area tiles is exactly the limit; one extra tile outside it is too many.
	const nlohmann::json at_limit { { "brush", "grass" }, { "z", 7 }, { "area", { { "x", 0 }, { "y", 0 }, { "width", 200 }, { "height", 100 } } } };
	CHECK_FALSE(ParsePaintRequest(at_limit, 1000, 1000).error);

	nlohmann::json over = at_limit;
	over["tiles"] = nlohmann::json::array({ nlohmann::json::array({ 500, 500 }) });
	const auto parsed = ParsePaintRequest(over, 1000, 1000);
	REQUIRE(parsed.error);
	CHECK(parsed.error->code == "paint_too_large");

	// A tile already inside the area is deduplicated and does not count twice.
	nlohmann::json overlapping = at_limit;
	overlapping["tiles"] = nlohmann::json::array({ nlohmann::json::array({ 5, 5 }) });
	CHECK_FALSE(ParsePaintRequest(overlapping, 1000, 1000).error);
}

TEST_CASE("clear area label is 1 to PatchLabelLimit bytes", "[ai_control][clear]") {
	auto with_label = [](nlohmann::json label) {
		nlohmann::json body { { "x", 1 }, { "y", 2 }, { "z", 7 }, { "width", 3 }, { "height", 4 } };
		body["label"] = std::move(label);
		return ParseClearAreaRequest(body, 100, 100);
	};
	CHECK(with_label("").error);
	CHECK(with_label(std::string(PatchLabelLimit + 1, 'x')).error);
	CHECK(with_label(42).error);
	const auto longest = with_label(std::string(PatchLabelLimit, 'x'));
	REQUIRE_FALSE(longest.error);
	CHECK(longest.request.label.size() == PatchLabelLimit);
}

TEST_CASE("brush query limit is 1 to BrushListLimit", "[ai_control][brushes]") {
	CHECK(ParseBrushQuery({ { "limit", 0 } }).error);
	CHECK(ParseBrushQuery({ { "limit", BrushListLimit + 1 } }).error);
	CHECK(ParseBrushQuery({ { "limit", "10" } }).error);
	CHECK(ParseBrushQuery({ { "limit", 1 } }).query.limit == 1);
	CHECK(ParseBrushQuery({ { "limit", BrushListLimit } }).query.limit == static_cast<size_t>(BrushListLimit));
	CHECK(ParseBrushQuery(nlohmann::json::object()).query.limit == static_cast<size_t>(BrushListDefault));
}

TEST_CASE("connection phases expire after their deadline", "[ai_control][http]") {
	using std::chrono::milliseconds;
	using std::chrono::seconds;
	CHECK_FALSE(IsConnectionExpired(ConnectionPhase::Reading, seconds(10)));
	CHECK(IsConnectionExpired(ConnectionPhase::Reading, seconds(10) + milliseconds(1)));
	// Writing is a total budget, not per wait, so a slow reader cannot hold the socket longer.
	CHECK_FALSE(IsConnectionExpired(ConnectionPhase::Writing, seconds(5)));
	CHECK(IsConnectionExpired(ConnectionPhase::Writing, seconds(6)));
	CHECK(IsConnectionExpired(ConnectionPhase::Draining, seconds(3)));
	CHECK_FALSE(IsConnectionExpired(ConnectionPhase::Draining, milliseconds(500)));
}

TEST_CASE("draining connections do not count toward the connection limit", "[ai_control][http]") {
	CHECK(CountsTowardConnectionLimit(ConnectionPhase::Reading));
	CHECK(CountsTowardConnectionLimit(ConnectionPhase::Writing));
	CHECK_FALSE(CountsTowardConnectionLimit(ConnectionPhase::Draining));
}

TEST_CASE("batch prevalidation rejects a bad map-changing op before anything runs", "[ai_control][batch]") {
	const nlohmann::json good_paint { { "op", "paint" }, { "brush", "grass" }, { "z", 7 }, { "tiles", nlohmann::json::array({ nlohmann::json::array({ 5, 5 }) }) } };
	const auto plan_of = [](nlohmann::json ops) {
		const auto parsed = ParseBatchRequest({ { "ops", std::move(ops) } });
		REQUIRE_FALSE(parsed.error);
		return parsed.plan;
	};

	CHECK_FALSE(PrevalidateBatch(plan_of({ good_paint, { { "op", "undo" } }, { { "op", "inspect_tile" }, { "nonsense", 1 } } }), 100, 100));

	const auto missing_tiles = PrevalidateBatch(plan_of({ good_paint, { { "op", "paint" }, { "brush", "grass" } } }), 100, 100);
	REQUIRE(missing_tiles);
	CHECK(missing_tiles->status == 400);
	CHECK(missing_tiles->message.starts_with("Operation 2 (paint):"));

	CHECK(PrevalidateBatch(plan_of({ { { "op", "clear_area" }, { "x", 0 }, { "y", 0 }, { "z", 7 }, { "width", 200 }, { "height", 1 } } }), 100, 100));
	CHECK(PrevalidateBatch(plan_of({ { { "op", "undo" }, { "steps", 0 } } }), 100, 100));
	CHECK(PrevalidateBatch(plan_of({ { { "op", "apply_patch" }, { "tiles", nlohmann::json::array() } } }), 100, 100));
	CHECK(PrevalidateBatch(plan_of({ { { "op", "export_region" }, { "path", "relative.png" } } }), 100, 100));
}
