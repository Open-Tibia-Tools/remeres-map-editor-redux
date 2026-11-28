//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_AI_CONTROL_CONTRACT_H
#define RME_AI_CONTROL_CONTRACT_H

#include "map/position.h"
#include "map/position_rect.h"
#include "util/json.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Pure request/response contract of the local AI Control API: HTTP framing and policy, route
// table, request parsing and validation. No wxWidgets, map, editor or GUI access, so every
// decision here is unit-tested in isolation (tests/test_ai_control_contract.cpp).
namespace AiControl {

	inline constexpr uint16_t DefaultPort = 17310;
	inline constexpr int ApiVersion = 1;
	inline constexpr std::string_view ServiceName = "Remere's Map Editor AI Control API";
	inline constexpr std::string_view BindAddress = "127.0.0.1";

	inline constexpr size_t MaxHeaderBytes = 64 * 1024;
	inline constexpr size_t MaxBodyBytes = 1024 * 1024;

	// A connection reads its request, writes the response, then drains what the client still
	// sends until the client closes. Each phase has a deadline after which it is closed.
	enum class ConnectionPhase {
		Reading,
		Writing,
		Draining,
	};

	inline constexpr int64_t InspectAreaTileLimit = 20000;
	inline constexpr int InspectAreaDefaultSamples = 64;
	inline constexpr int InspectAreaSampleLimit = 512;
	inline constexpr int64_t TerrainGridTileLimit = 50000;
	inline constexpr int64_t ClearAreaTileLimit = 50000;
	inline constexpr size_t PaintTileLimit = 20000;
	inline constexpr size_t PatchTileLimit = 256;
	inline constexpr size_t PatchAddedItemLimit = 1024;
	inline constexpr size_t PatchLabelLimit = 120;
	inline constexpr size_t BatchOpLimit = 200;
	inline constexpr int HistoryStepLimit = 1000;
	inline constexpr size_t HistoryEntryLimit = 100;
	inline constexpr size_t SelectionListLimit = 4096;
	inline constexpr int BrushListDefault = 50;
	inline constexpr int BrushListLimit = 500;
	inline constexpr size_t BrushSuggestionLimit = 5;
	inline constexpr int ExportSideLimitPixels = 16384;
	inline constexpr int64_t ExportPixelLimit = 16LL * 1024 * 1024;

	// An error as the API reports it: HTTP status, machine-readable code and a sentence for humans.
	struct ApiError {
		int status = 400;
		std::string code;
		std::string message;
	};

	[[nodiscard]] nlohmann::json ErrorBody(const ApiError& error);
	// Never throws on invalid UTF-8 in map or item names: such bytes are replaced.
	[[nodiscard]] std::string SerializeJson(const nlohmann::json& value);
	[[nodiscard]] std::string ToLower(std::string_view text);

	//=========================================================================
	// HTTP framing and request policy

	enum class HttpMethod {
		Get,
		Post,
	};

	[[nodiscard]] std::string_view HttpMethodName(HttpMethod method);

	struct HttpRequest {
		HttpMethod method = HttpMethod::Get;
		std::string path;
		std::optional<std::string> host;
		std::optional<std::string> contentType;
		bool hasOrigin = false;
		std::string body;
	};

	enum class HttpParseState {
		Incomplete,
		Complete,
		Rejected,
	};

	struct HttpParseResult {
		HttpParseState state = HttpParseState::Incomplete;
		HttpRequest request;
		std::optional<ApiError> rejection;
	};

	// Frames one request from the bytes received so far. Only GET and POST with a Content-Length
	// body are accepted; headers are limited to MaxHeaderBytes and the body to MaxBodyBytes.
	[[nodiscard]] HttpParseResult ParseHttpRequest(std::string_view received);

	// Browser and DNS-rebinding defense: no Origin header, Host is 127.0.0.1:<port> or
	// localhost:<port>, and every POST declares Content-Type: application/json.
	[[nodiscard]] std::optional<ApiError> CheckRequestPolicy(const HttpRequest& request, uint16_t port);

	struct JsonBodyResult {
		nlohmann::json body = nlohmann::json::object();
		std::optional<ApiError> error;
	};

	// An empty body is an empty object; anything else must be a JSON object.
	[[nodiscard]] JsonBodyResult ParseJsonBody(std::string_view text);

	[[nodiscard]] std::string_view ReasonPhrase(int status);
	[[nodiscard]] std::string BuildHttpResponse(int status, std::string_view payload);

	// RME_AI_CONTROL_PORT value: a decimal port from 1024 to 65535.
	[[nodiscard]] std::optional<uint16_t> ParsePort(std::string_view text);

	//=========================================================================
	// Route table

	enum class RouteId {
		Health,
		Capabilities,
		Snapshot,
		Brushes,
		InspectTile,
		InspectArea,
		TerrainGrid,
		Paint,
		ClearArea,
		PreviewPatch,
		ApplyPatch,
		Undo,
		Redo,
		History,
		Batch,
		ExportRegion,
	};

	// Status routes always answer; Query and Mutation routes pass the busy gate first, and
	// Mutation routes additionally need an editor that accepts map changes.
	enum class RouteKind {
		Status,
		Query,
		Mutation,
	};

	struct RouteSpec {
		RouteId id;
		HttpMethod method;
		std::string_view path;
		RouteKind kind;
		bool batchable;
		std::string_view summary;
		std::string_view body;
	};

	[[nodiscard]] std::span<const RouteSpec> Routes();
	[[nodiscard]] const RouteSpec& RouteFor(RouteId id);
	// Accepts the "/ai" prefix alias: "/ai/health" is "/health".
	[[nodiscard]] std::string_view NormalizeRoutePath(std::string_view path);

	struct RouteMatch {
		const RouteSpec* route = nullptr;
		std::optional<ApiError> error;
	};

	// 404 for an unknown path, 405 for a known path with the wrong method.
	[[nodiscard]] RouteMatch LookupRoute(HttpMethod method, std::string_view path);
	[[nodiscard]] nlohmann::json CapabilitiesJson();

	struct EditorActivity {
		bool loading = false;
		bool renderingDisabled = false;
		bool modalDialogOpen = false;
		bool requestInProgress = false;
		// The user is drawing, dragging, selecting with the mouse or pasting.
		bool userInteracting = false;
	};

	// 503 for every non-status route while the editor cannot safely run a request; while the user
	// is interacting, 503 only for mutations and export_region (which re-renders the view).
	[[nodiscard]] std::optional<ApiError> CheckBusyGate(const RouteSpec& route, const EditorActivity& activity);

	// Reading: seconds since the last received byte. Writing and draining: seconds since the phase
	// began, so a client reading a few bytes at a time cannot hold the connection open.
	[[nodiscard]] int PhaseTimeoutSeconds(ConnectionPhase phase);
	[[nodiscard]] bool IsConnectionExpired(ConnectionPhase phase, std::chrono::steady_clock::duration elapsed);
	// Draining connections already have their answer; they never block a new client.
	[[nodiscard]] bool CountsTowardConnectionLimit(ConnectionPhase phase);

	//=========================================================================
	// Request parsing

	[[nodiscard]] std::optional<int> Integer(const nlohmann::json& body, std::string_view key);
	// Rejects any key of `body` that is not listed, so misspelled or unsupported options fail loudly.
	[[nodiscard]] std::optional<ApiError> RejectUnknownFields(const nlohmann::json& body, std::span<const std::string_view> allowed, std::string_view context);

	struct Area {
		int x = 0;
		int y = 0;
		int z = GROUND_LAYER;
		int width = 0;
		int height = 0;
	};

	[[nodiscard]] std::optional<Area> ParseArea(const nlohmann::json& body);
	[[nodiscard]] std::optional<ApiError> ValidateArea(const Area& area, int mapWidth, int mapHeight, int64_t tileLimit);
	[[nodiscard]] nlohmann::json AreaJson(const Area& area);

	struct TerrainLegendEntry {
		char symbol = '.';
		uint16_t itemId = 0;
		int count = 0;
	};

	struct TerrainGrid {
		char emptyChar = '.';
		char overflowChar = '#';
		std::vector<TerrainLegendEntry> legend;
		std::vector<TerrainLegendEntry> overflow;
		std::vector<std::string> rows;
		int foundTiles = 0;
		int emptyTiles = 0;
	};

	// Row-major ground ids, north to south. The most frequent ids get the 62 letter/digit symbols.
	[[nodiscard]] TerrainGrid EncodeTerrainGrid(int width, int height, std::span<const std::optional<uint16_t>> itemIds);

	enum class PatchMode {
		Preview,
		Apply,
	};

	struct PatchTileRequest {
		Position position;
		std::optional<uint16_t> groundId;
		bool replaceItems = false;
		std::vector<uint16_t> addItemIds;
	};

	struct PatchRequest {
		std::string label;
		std::string previewToken;
		std::vector<PatchTileRequest> tiles;
		size_t addedItemCount = 0;
	};

	struct PatchParseResult {
		std::optional<ApiError> error;
		PatchRequest request;
	};

	[[nodiscard]] PatchParseResult ParsePatchRequest(const nlohmann::json& body, PatchMode mode, int mapWidth, int mapHeight);
	// 16 lowercase hex digits of FNV-1a over `value`.
	[[nodiscard]] std::string StableToken(std::string_view value);

	enum class BrushKind {
		Ground,
		Wall,
		WallDecoration,
		Door,
		Doodad,
		Carpet,
		Table,
		Raw,
		Other,
	};

	[[nodiscard]] std::string_view BrushKindName(BrushKind kind);
	[[nodiscard]] std::optional<BrushKind> ParseBrushKind(std::string_view name);
	// Kinds that /paint draws; markers, flags, houses, creatures, spawns and the eraser are not.
	[[nodiscard]] bool IsPaintableBrushKind(BrushKind kind);

	struct PaintRequest {
		std::string brushName;
		std::optional<BrushKind> kind;
		int z = GROUND_LAYER;
		PositionVector positions;
		bool automagic = true;
	};

	struct PaintParseResult {
		std::optional<ApiError> error;
		PaintRequest request;
	};

	// Positions come from "area" and/or "tiles" ([x, y] pairs), deduplicated in request order.
	[[nodiscard]] PaintParseResult ParsePaintRequest(const nlohmann::json& body, int mapWidth, int mapHeight);

	struct ClearAreaRequest {
		Area area;
		bool keepGround = false;
		bool keepFlags = false;
		std::string label;
	};

	struct ClearAreaParseResult {
		std::optional<ApiError> error;
		ClearAreaRequest request;
	};

	[[nodiscard]] ClearAreaParseResult ParseClearAreaRequest(const nlohmann::json& body, int mapWidth, int mapHeight);

	struct StepsParseResult {
		std::optional<ApiError> error;
		int steps = 1;
	};

	// Undo/redo body: optional "steps", 1 to HistoryStepLimit.
	[[nodiscard]] StepsParseResult ParseHistorySteps(const nlohmann::json& body);

	struct BrushQuery {
		std::string needle;
		std::optional<BrushKind> kind;
		size_t limit = BrushListDefault;
	};

	struct BrushQueryParseResult {
		std::optional<ApiError> error;
		BrushQuery query;
	};

	[[nodiscard]] BrushQueryParseResult ParseBrushQuery(const nlohmann::json& body);
	[[nodiscard]] bool MatchesBrushQuery(const BrushQuery& query, std::string_view name, BrushKind kind);
	// "Did you mean": exact (case-insensitive) first, then substring matches, then names within
	// edit distance 3; shorter names win ties.
	[[nodiscard]] std::vector<std::string> SuggestBrushNames(std::string_view wanted, std::span<const std::string> names, size_t limit = BrushSuggestionLimit);

	struct ExportRegionRequest {
		PositionRect region;
		int z = GROUND_LAYER;
		std::string path;
		bool overwrite = false;
	};

	struct ExportRegionParseResult {
		std::optional<ApiError> error;
		ExportRegionRequest request;
	};

	// Plain PNG export: absolute local path ending in .png (no UNC or device paths), region inside
	// the map and the pixel limits. Whether the file already exists is the caller's check.
	[[nodiscard]] ExportRegionParseResult ParseExportRegionRequest(const nlohmann::json& body, int mapWidth, int mapHeight);

	struct BatchOp {
		const RouteSpec* route = nullptr;
		nlohmann::json args = nlohmann::json::object();
	};

	struct BatchPlan {
		std::vector<BatchOp> ops;
		bool rollbackOnError = true;
	};

	struct BatchParseResult {
		std::optional<ApiError> error;
		BatchPlan plan;
	};

	// Validates every op before anything runs: 1 to BatchOpLimit ops, each a batchable route.
	[[nodiscard]] BatchParseResult ParseBatchRequest(const nlohmann::json& body);

	// Runs the request parser of every map-changing op (paint, clear_area, apply_patch, undo,
	// redo) and of export_region before the batch starts. Brush names, preview tokens and read
	// ops are only checked when the op runs. Returns the first error with its 1-based op number.
	[[nodiscard]] std::optional<ApiError> PrevalidateBatch(const BatchPlan& plan, int mapWidth, int mapHeight);

	// ActionQueue's monotonic entry counters (pushed as a new entry, merged into the previous one).
	struct HistoryCounters {
		uint64_t pushed = 0;
		uint64_t merged = 0;
	};

	struct RollbackPlan {
		size_t undoSteps = 0;
		// True when undoing undoSteps entries restores the history to where the batch started.
		bool complete = false;
	};

	// Undo exactly the entries the batch pushed, never more than are still undoable. Incomplete when
	// a change merged into an older entry, history dropped one of the batch's own entries, or an
	// undo/redo op moved history (then nothing is undone: an older user entry could be hit).
	[[nodiscard]] RollbackPlan PlanBatchRollback(const HistoryCounters& before, const HistoryCounters& after, size_t currentIndex, bool historyMoved);

} // namespace AiControl

#endif
