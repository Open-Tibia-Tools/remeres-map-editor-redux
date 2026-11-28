//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "ai/ai_control_service.h"

#include "app/settings.h"
#include "brushes/brush.h"
#include "brushes/carpet/carpet_brush.h"
#include "brushes/doodad/doodad_brush.h"
#include "brushes/door/door_brush.h"
#include "brushes/ground/ground_brush.h"
#include "brushes/raw/raw_brush.h"
#include "brushes/table/table_brush.h"
#include "brushes/wall/wall_brush.h"
#include "editor/action_queue.h"
#include "editor/editor.h"
#include "editor/managers/editor_manager.h"
#include "editor/operations/brush_fill_operations.h"
#include "game/item.h"
#include "item_definitions/core/item_definition_store.h"
#include "map/map.h"
#include "map/tile.h"
#include "map/tile_operations.h"
#include "rendering/ui/drawing_controller.h"
#include "rendering/ui/map_display.h"
#include "rendering/ui/screenshot_controller.h"
#include "rendering/ui/selection_controller.h"
#include "ui/gui.h"
#include "ui/map_tab.h"

#include <wx/window.h>

#include <algorithm>
#include <filesystem>
#include <format>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace AiControl {

	namespace {

		Response Ok(nlohmann::json body) {
			body["ok"] = true;
			return { 200, std::move(body) };
		}

		Response Fail(const ApiError& error) {
			return { error.status, ErrorBody(error) };
		}

		ApiError NoDocument() {
			return { 400, "no_document", "No map is open in the editor." };
		}

		nlohmann::json PositionJson(const Position& position) {
			return { { "x", position.x }, { "y", position.y }, { "z", position.z } };
		}

		std::string ItemName(uint16_t item_id) {
			const auto definition = g_item_definitions.get(item_id);
			return definition ? std::string(definition.name()) : std::string();
		}

		BrushKind BrushKindOf(const Brush& brush) {
			if (brush.is<GroundBrush>()) {
				return BrushKind::Ground;
			}
			if (brush.is<WallDecorationBrush>()) {
				return BrushKind::WallDecoration;
			}
			if (brush.is<WallBrush>()) {
				return BrushKind::Wall;
			}
			if (brush.is<DoorBrush>()) {
				return BrushKind::Door;
			}
			if (brush.is<DoodadBrush>()) {
				return BrushKind::Doodad;
			}
			if (brush.is<CarpetBrush>()) {
				return BrushKind::Carpet;
			}
			if (brush.is<TableBrush>()) {
				return BrushKind::Table;
			}
			if (brush.is<RAWBrush>()) {
				return BrushKind::Raw;
			}
			return BrushKind::Other;
		}

		nlohmann::json BrushJson(const Brush& brush) {
			return { { "id", brush.getID() }, { "name", brush.getName() }, { "type", BrushKindName(BrushKindOf(brush)) } };
		}

		nlohmann::json ItemJson(const Item& item, int stack_index, bool ground) {
			const auto definition = item.getDefinition();
			nlohmann::json result {
				{ "itemId", item.getID() },
				{ "clientId", item.getClientID() },
				{ "resolved", static_cast<bool>(definition) },
				{ "stackIndex", stack_index },
				{ "ground", ground },
				{ "count", item.getCount() },
				{ "actionId", item.getActionID() },
				{ "uniqueId", item.getUniqueID() },
			};
			if (!item.getText().empty()) {
				result["text"] = std::string(item.getText());
			}
			if (!item.getDescription().empty()) {
				result["description"] = std::string(item.getDescription());
			}
			if (definition) {
				result["name"] = std::string(definition.name());
				result["flags"] = {
					{ "stackable", definition.hasFlag(ItemFlag::Stackable) },
					{ "moveable", definition.hasFlag(ItemFlag::Moveable) },
					{ "border", definition.hasFlag(ItemFlag::IsBorder) },
					{ "wall", definition.hasFlag(ItemFlag::IsWall) },
					{ "door", definition.hasFlag(ItemFlag::IsBrushDoor) },
					{ "unpassable", definition.hasFlag(ItemFlag::Unpassable) },
					{ "blockPathfinder", definition.hasFlag(ItemFlag::BlockPathfinder) },
					{ "floorChange", definition.hasFlag(ItemFlag::FloorChange) },
					{ "elevation", definition.hasFlag(ItemFlag::HasElevation) },
				};
			}
			const Brush* brush = item.getBrush() ? item.getBrush() : item.getDoodadBrush();
			if (brush) {
				result["brush"] = BrushJson(*brush);
			}
			return result;
		}

		nlohmann::json TileJson(const Tile* tile, const Position& position) {
			nlohmann::json result {
				{ "position", PositionJson(position) },
				{ "tileFound", tile != nullptr },
				{ "items", nlohmann::json::array() },
			};
			if (!tile) {
				return result;
			}
			result["mapFlags"] = tile->getMapFlags();
			result["houseId"] = tile->getHouseID();
			result["blocking"] = tile->isBlocking();
			const auto zones = tile->getZoneIds();
			result["zoneIds"] = std::vector<uint16_t>(zones.begin(), zones.end());
			int stack_index = 0;
			if (tile->ground) {
				result["items"].push_back(ItemJson(*tile->ground, stack_index++, true));
			}
			for (const auto& item : tile->items) {
				if (item) {
					result["items"].push_back(ItemJson(*item, stack_index++, false));
				}
			}
			return result;
		}

		void RefreshAfterMapChange() {
			g_gui.RefreshView();
			g_gui.UpdateMinimap();
		}

		// Ends the previous history entry before a mutation and this one after it, so the
		// ActionQueue never stacks an API change into a user action or the other way round.
		class HistoryIsolation {
		public:
			explicit HistoryIsolation(Editor& editor) :
				editor_(editor) {
				editor_.actionQueue->resetTimer();
			}
			~HistoryIsolation() {
				editor_.actionQueue->resetTimer();
			}
			HistoryIsolation(const HistoryIsolation&) = delete;
			HistoryIsolation& operator=(const HistoryIsolation&) = delete;

		private:
			Editor& editor_;
		};

		std::optional<ApiError> CheckEditorAcceptsChanges(Editor& editor) {
			if (editor.live_manager.IsClient()) {
				return ApiError { 403, "live_client_read_only", "A Live client session cannot be changed through the AI Control API." };
			}
			if (editor.selection.isBusy()) {
				return ApiError { 503, "busy", "The selection is being updated." };
			}
			return std::nullopt;
		}

		std::optional<ApiError> RequireCurrentEditor(Editor& editor) {
			if (g_gui.GetCurrentEditor() != &editor) {
				return ApiError { 400, "document_changed", "The active map changed while the request ran." };
			}
			return std::nullopt;
		}

		//=====================================================================
		// Read handlers

		Response HandleHealth() {
			return Ok({
				{ "service", ServiceName },
				{ "version", ApiVersion },
				{ "documentOpen", g_gui.GetCurrentEditor() != nullptr },
				{ "openMaps", g_gui.GetOpenMapCount() },
			});
		}

		Response HandleSnapshot(Editor& editor) {
			const Map& map = editor.map;
			const MapVersion version = map.getVersion();
			const auto& selected = editor.selection.getTiles();
			nlohmann::json selection = nlohmann::json::array();
			for (size_t index = 0; index < std::min(SelectionListLimit, selected.size()); ++index) {
				if (selected[index]) {
					selection.push_back(PositionJson(selected[index]->getPosition()));
				}
			}

			nlohmann::json brushes = nlohmann::json::array();
			std::vector<uint16_t> related_items;
			for (const auto& [name, brush] : g_brushes.getMap()) {
				if (!brush) {
					continue;
				}
				related_items.clear();
				brush->getRelatedItems(related_items);
				brushes.push_back({
					{ "id", brush->getID() },
					{ "name", name },
					{ "type", BrushKindName(BrushKindOf(*brush)) },
					{ "lookId", brush->getLookID() },
					{ "relatedItemCount", related_items.size() },
					{ "needsBorders", brush->needBorders() },
					{ "canDrag", brush->canDrag() },
				});
			}

			const Brush* active_brush = g_gui.GetCurrentBrush();
			return Ok({
				{ "documentOpen", true },
				{ "map", {
							 { "name", map.getName() },
							 { "width", map.getWidth() },
							 { "height", map.getHeight() },
							 { "tileCount", map.getTileCount() },
							 { "otbmVersion", static_cast<int>(version.otbm) },
							 { "clientVersion", static_cast<int>(version.client) },
						 } },
				{ "selection", std::move(selection) },
				{ "selectionCount", selected.size() },
				{ "selectionTruncated", selected.size() > SelectionListLimit },
				{ "tools", {
							   { "activeBrush", active_brush ? BrushJson(*active_brush) : nlohmann::json(nullptr) },
							   { "brushSizeX", g_gui.GetBrushSizeX() },
							   { "brushSizeY", g_gui.GetBrushSizeY() },
							   { "brushShape", g_gui.GetBrushShape() == BRUSHSHAPE_CIRCLE ? "circle" : "square" },
							   { "automagic", g_settings.getInteger(Config::USE_AUTOMAGIC) != 0 },
							   { "doorsLocked", g_gui.HasDoorLocked() },
						   } },
				{ "brushes", std::move(brushes) },
			});
		}

		Response HandleBrushes(const nlohmann::json& body) {
			const auto parsed = ParseBrushQuery(body);
			if (parsed.error) {
				return Fail(*parsed.error);
			}
			nlohmann::json list = nlohmann::json::array();
			size_t matches = 0;
			for (const auto& [name, brush] : g_brushes.getMap()) {
				if (!brush) {
					continue;
				}
				const BrushKind kind = BrushKindOf(*brush);
				if (!MatchesBrushQuery(parsed.query, name, kind)) {
					continue;
				}
				++matches;
				if (list.size() < parsed.query.limit) {
					list.push_back({ { "name", name }, { "type", BrushKindName(kind) }, { "lookId", brush->getLookID() }, { "paintable", IsPaintableBrushKind(kind) } });
				}
			}
			return Ok({ { "matches", matches }, { "brushes", std::move(list) } });
		}

		Response HandleInspectTile(Editor& editor, const nlohmann::json& body) {
			static constexpr std::array<std::string_view, 3> Fields { "x", "y", "z" };
			if (auto error = RejectUnknownFields(body, Fields, "inspect_tile")) {
				return Fail(*error);
			}
			const auto x = Integer(body, "x");
			const auto y = Integer(body, "y");
			const auto z = Integer(body, "z");
			if (!x || !y || !z) {
				return Fail({ 400, "bad_request", "Expected integer x, y and z." });
			}
			if (auto error = ValidateArea({ *x, *y, *z, 1, 1 }, editor.map.getWidth(), editor.map.getHeight(), 1)) {
				return Fail(*error);
			}
			const Position position(*x, *y, *z);
			return Ok(TileJson(editor.map.getTile(position), position));
		}

		Response HandleInspectArea(Editor& editor, const nlohmann::json& body) {
			static constexpr std::array<std::string_view, 6> Fields { "x", "y", "z", "width", "height", "sampleLimit" };
			if (auto error = RejectUnknownFields(body, Fields, "inspect_area")) {
				return Fail(*error);
			}
			const auto area = ParseArea(body);
			if (!area) {
				return Fail({ 400, "bad_request", "Expected integer x, y, z, width and height." });
			}
			if (auto error = ValidateArea(*area, editor.map.getWidth(), editor.map.getHeight(), InspectAreaTileLimit)) {
				return Fail(*error);
			}
			int sample_limit = InspectAreaDefaultSamples;
			if (body.contains("sampleLimit")) {
				const auto requested = Integer(body, "sampleLimit");
				if (!requested || *requested < 0 || *requested > InspectAreaSampleLimit) {
					return Fail({ 400, "bad_request", std::format("sampleLimit must be an integer from 0 to {}.", InspectAreaSampleLimit) });
				}
				sample_limit = *requested;
			}

			int found_tiles = 0;
			int empty_tiles = 0;
			int total_items = 0;
			int unresolved_items = 0;
			std::map<uint16_t, int> histogram;
			nlohmann::json samples = nlohmann::json::array();
			const auto count_item = [&](const Item* item) {
				if (!item) {
					return;
				}
				++histogram[item->getID()];
				++total_items;
				if (!item->getDefinition()) {
					++unresolved_items;
				}
			};
			for (int dy = 0; dy < area->height; ++dy) {
				for (int dx = 0; dx < area->width; ++dx) {
					const Position position(area->x + dx, area->y + dy, area->z);
					const Tile* tile = editor.map.getTile(position);
					if (!tile || tile->empty()) {
						++empty_tiles;
						continue;
					}
					++found_tiles;
					count_item(tile->ground.get());
					for (const auto& item : tile->items) {
						count_item(item.get());
					}
					if (static_cast<int>(samples.size()) < sample_limit) {
						samples.push_back(TileJson(tile, position));
					}
				}
			}

			std::vector<std::pair<uint16_t, int>> ranked(histogram.begin(), histogram.end());
			std::ranges::sort(ranked, [](const auto& left, const auto& right) {
				return left.second != right.second ? left.second > right.second : left.first < right.first;
			});
			nlohmann::json item_histogram = nlohmann::json::array();
			for (const auto& [item_id, count] : ranked) {
				item_histogram.push_back({ { "itemId", item_id }, { "name", ItemName(item_id) }, { "count", count } });
			}
			return Ok({
				{ "area", AreaJson(*area) },
				{ "foundTiles", found_tiles },
				{ "emptyTiles", empty_tiles },
				{ "totalItems", total_items },
				{ "unresolvedItems", unresolved_items },
				{ "itemHistogram", std::move(item_histogram) },
				{ "samples", std::move(samples) },
			});
		}

		nlohmann::json LegendJson(const std::vector<TerrainLegendEntry>& entries) {
			nlohmann::json json = nlohmann::json::array();
			for (const auto& entry : entries) {
				json.push_back({ { "char", std::string(1, entry.symbol) }, { "itemId", entry.itemId }, { "name", ItemName(entry.itemId) }, { "count", entry.count } });
			}
			return json;
		}

		Response HandleTerrainGrid(Editor& editor, const nlohmann::json& body) {
			static constexpr std::array<std::string_view, 5> Fields { "x", "y", "z", "width", "height" };
			if (auto error = RejectUnknownFields(body, Fields, "terrain_grid")) {
				return Fail(*error);
			}
			const auto area = ParseArea(body);
			if (!area) {
				return Fail({ 400, "bad_request", "Expected integer x, y, z, width and height." });
			}
			if (auto error = ValidateArea(*area, editor.map.getWidth(), editor.map.getHeight(), TerrainGridTileLimit)) {
				return Fail(*error);
			}
			std::vector<std::optional<uint16_t>> ground_ids;
			ground_ids.reserve(static_cast<size_t>(area->width) * area->height);
			for (int dy = 0; dy < area->height; ++dy) {
				for (int dx = 0; dx < area->width; ++dx) {
					const Tile* tile = editor.map.getTile(area->x + dx, area->y + dy, area->z);
					ground_ids.push_back(tile && tile->ground ? std::optional<uint16_t>(tile->ground->getID()) : std::nullopt);
				}
			}
			const TerrainGrid grid = EncodeTerrainGrid(area->width, area->height, ground_ids);
			return Ok({
				{ "area", AreaJson(*area) },
				{ "emptyChar", std::string(1, grid.emptyChar) },
				{ "overflowChar", std::string(1, grid.overflowChar) },
				{ "legend", LegendJson(grid.legend) },
				{ "overflow", LegendJson(grid.overflow) },
				{ "rows", grid.rows },
				{ "foundTiles", grid.foundTiles },
				{ "emptyTiles", grid.emptyTiles },
			});
		}

		Response HandleHistory(Editor& editor) {
			const ActionQueue& queue = *editor.actionQueue;
			const size_t size = queue.getSize();
			nlohmann::json entries = nlohmann::json::array();
			for (size_t index = size > HistoryEntryLimit ? size - HistoryEntryLimit : 0; index < size; ++index) {
				entries.push_back({ { "index", index }, { "label", queue.getActionName(index) }, { "applied", index < queue.getCurrentIndex() } });
			}
			return Ok({
				{ "index", queue.getCurrentIndex() },
				{ "size", size },
				{ "canUndo", editor.actionQueue->canUndo() },
				{ "canRedo", editor.actionQueue->canRedo() },
				{ "entries", std::move(entries) },
			});
		}

		//=====================================================================
		// Patches

		struct PreparedPatch {
			std::string label;
			std::string previewToken;
			size_t addedItemCount = 0;
			nlohmann::json previewTiles = nlohmann::json::array();
			std::vector<std::unique_ptr<Tile>> stagedTiles;
		};

		struct PreparedPatchResult {
			std::optional<ApiError> error;
			PreparedPatch patch;
		};

		std::optional<ApiError> ValidatePatchItems(const PatchRequest& request) {
			for (const PatchTileRequest& tile : request.tiles) {
				if (tile.groundId) {
					const auto definition = g_item_definitions.get(*tile.groundId);
					if (!definition) {
						return ApiError { 400, "unknown_item", std::format("Unknown ground item id {}.", *tile.groundId) };
					}
					if (!definition.isGroundTile()) {
						return ApiError { 400, "invalid_ground", std::format("Item {} is not a ground item.", *tile.groundId) };
					}
				}
				for (const uint16_t item_id : tile.addItemIds) {
					const auto definition = g_item_definitions.get(item_id);
					if (!definition) {
						return ApiError { 400, "unknown_item", std::format("Unknown item id {}.", item_id) };
					}
					if (definition.isGroundTile()) {
						return ApiError { 400, "invalid_stack_item", std::format("addItemIds cannot contain ground item {}.", item_id) };
					}
				}
			}
			return std::nullopt;
		}

		// Builds the after-state of every patched tile on copies. Preview copies never touch the map;
		// apply copies of new positions get their map location, like DrawOperations does.
		PreparedPatchResult PreparePatch(Editor& editor, const nlohmann::json& body, PatchMode mode) {
			const auto fail = [](ApiError error) {
				return PreparedPatchResult { std::move(error), {} };
			};
			const PatchParseResult parsed = ParsePatchRequest(body, mode, editor.map.getWidth(), editor.map.getHeight());
			if (parsed.error) {
				return fail(*parsed.error);
			}
			if (auto error = ValidatePatchItems(parsed.request)) {
				return fail(std::move(*error));
			}

			PreparedPatch prepared;
			prepared.label = parsed.request.label.empty() ? "AI map patch" : parsed.request.label;
			prepared.addedItemCount = parsed.request.addedItemCount;
			prepared.stagedTiles.reserve(parsed.request.tiles.size());
			nlohmann::json token_material {
				{ "map", { { "name", editor.map.getName() }, { "width", editor.map.getWidth() }, { "height", editor.map.getHeight() }, { "undoIndex", editor.actionQueue->getCurrentIndex() }, { "undoSize", editor.actionQueue->getSize() } } },
				{ "label", prepared.label },
				{ "tiles", nlohmann::json::array() },
			};

			for (const PatchTileRequest& request : parsed.request.tiles) {
				const Position& position = request.position;
				const Tile* current = editor.map.getTile(position);
				std::unique_ptr<Tile> changed;
				if (current) {
					changed = TileOperations::deepCopy(current, editor.map);
				} else if (mode == PatchMode::Apply) {
					changed = editor.map.allocator(editor.map.createTileL(position));
				} else {
					changed = std::make_unique<Tile>(position.x, position.y, position.z);
				}

				nlohmann::json operation {
					{ "x", position.x },
					{ "y", position.y },
					{ "z", position.z },
					{ "replaceItems", request.replaceItems },
					{ "addItemIds", request.addItemIds },
				};
				if (request.replaceItems) {
					changed->items.clear();
					TileOperations::update(changed.get());
				}
				if (request.groundId) {
					auto ground = Item::Create(*request.groundId);
					if (!ground) {
						return fail({ 400, "item_creation_failed", std::format("Could not create ground item {}.", *request.groundId) });
					}
					changed->setGround(std::move(ground));
					operation["groundId"] = *request.groundId;
				}
				for (const uint16_t item_id : request.addItemIds) {
					auto item = Item::Create(item_id);
					if (!item) {
						return fail({ 400, "item_creation_failed", std::format("Could not create item {}.", item_id) });
					}
					changed->addItem(std::move(item));
				}

				nlohmann::json before = TileJson(current, position);
				prepared.previewTiles.push_back({ { "position", PositionJson(position) }, { "before", before }, { "after", TileJson(changed.get(), position) } });
				token_material["tiles"].push_back({ { "operation", std::move(operation) }, { "before", std::move(before) } });
				prepared.stagedTiles.push_back(std::move(changed));
			}

			prepared.previewToken = StableToken(SerializeJson(token_material));
			if (mode == PatchMode::Apply && parsed.request.previewToken != prepared.previewToken) {
				return fail({ 400, "stale_preview", "The map or the undo history changed after the preview; request a new preview before applying." });
			}
			return { std::nullopt, std::move(prepared) };
		}

		Response HandlePreviewPatch(Editor& editor, const nlohmann::json& body) {
			PreparedPatchResult result = PreparePatch(editor, body, PatchMode::Preview);
			if (result.error) {
				return Fail(*result.error);
			}
			return Ok({
				{ "previewToken", result.patch.previewToken },
				{ "label", result.patch.label },
				{ "tileCount", result.patch.stagedTiles.size() },
				{ "addedItemCount", result.patch.addedItemCount },
				{ "tiles", std::move(result.patch.previewTiles) },
			});
		}

		Response HandleApplyPatch(Editor& editor, const nlohmann::json& body) {
			PreparedPatchResult result = PreparePatch(editor, body, PatchMode::Apply);
			if (result.error) {
				return Fail(*result.error);
			}
			const size_t changed_tiles = result.patch.stagedTiles.size();
			{
				const HistoryIsolation isolation(editor);
				auto batch = editor.actionQueue->createBatch(ACTION_DRAW);
				batch->setLabel(result.patch.label);
				auto action = editor.actionQueue->createAction(batch.get());
				for (auto& tile : result.patch.stagedTiles) {
					action->addChange(std::make_unique<Change>(std::move(tile)));
				}
				batch->addAndCommitAction(std::move(action));
				editor.addBatch(std::move(batch));
			}
			RefreshAfterMapChange();
			return Ok({
				{ "applied", true },
				{ "previewToken", result.patch.previewToken },
				{ "label", result.patch.label },
				{ "changedTiles", changed_tiles },
				{ "addedItems", result.patch.addedItemCount },
				{ "undoIndex", editor.actionQueue->getCurrentIndex() },
			});
		}

		//=====================================================================
		// Painting and clearing

		std::vector<std::string> AllBrushNames() {
			std::vector<std::string> names;
			names.reserve(g_brushes.getMap().size());
			for (const auto& [name, brush] : g_brushes.getMap()) {
				if (brush) {
					names.push_back(name);
				}
			}
			return names;
		}

		struct ResolvedBrush {
			Brush* brush = nullptr;
			std::optional<ApiError> error;
		};

		ResolvedBrush ResolvePaintBrush(const PaintRequest& request) {
			bool name_exists = false;
			const auto [first, last] = g_brushes.getMap().equal_range(request.brushName);
			for (auto it = first; it != last; ++it) {
				if (!it->second) {
					continue;
				}
				name_exists = true;
				const BrushKind kind = BrushKindOf(*it->second);
				if ((!request.kind || *request.kind == kind) && IsPaintableBrushKind(kind)) {
					return { it->second.get(), std::nullopt };
				}
			}
			if (name_exists && !request.kind) {
				return { nullptr, ApiError { 400, "unsupported_brush", std::format("Brush '{}' is not a ground, wall, door, doodad, carpet, table or RAW brush.", request.brushName) } };
			}
			ApiError error { 400, "unknown_brush", request.kind ? std::format("No {} brush named '{}'.", BrushKindName(*request.kind), request.brushName) : std::format("No brush named '{}'.", request.brushName) };
			return { nullptr, std::move(error) };
		}

		Response HandlePaint(Editor& editor, const nlohmann::json& body) {
			const PaintParseResult parsed = ParsePaintRequest(body, editor.map.getWidth(), editor.map.getHeight());
			if (parsed.error) {
				return Fail(*parsed.error);
			}
			const ResolvedBrush resolved = ResolvePaintBrush(parsed.request);
			if (resolved.error) {
				Response response = Fail(*resolved.error);
				if (resolved.error->code == "unknown_brush") {
					response.body["suggestions"] = SuggestBrushNames(parsed.request.brushName, AllBrushNames());
				}
				return response;
			}

			const uint64_t entries_before = editor.actionQueue->getPushedCount() + editor.actionQueue->getMergedCount();
			BrushFillResult result = BrushFillResult::Drawn;
			{
				const HistoryIsolation isolation(editor);
				result = BrushFillOperations::fill(editor, *resolved.brush, parsed.request.positions, parsed.request.automagic);
			}
			RefreshAfterMapChange();

			if (result == BrushFillResult::NoDrawableTile) {
				return Fail({ 400, "nothing_drawn", "None of the tiles has a wall to place the door on." });
			}
			return Ok({
				{ "brush", resolved.brush->getName() },
				{ "type", BrushKindName(BrushKindOf(*resolved.brush)) },
				{ "tiles", parsed.request.positions.size() },
				{ "automagic", parsed.request.automagic },
				// False when the brush placed nothing, e.g. a doodad on tiles without ground.
				{ "changed", editor.actionQueue->getPushedCount() + editor.actionQueue->getMergedCount() != entries_before },
				{ "undoIndex", editor.actionQueue->getCurrentIndex() },
			});
		}

		Response HandleClearArea(Editor& editor, const nlohmann::json& body) {
			const ClearAreaParseResult parsed = ParseClearAreaRequest(body, editor.map.getWidth(), editor.map.getHeight());
			if (parsed.error) {
				return Fail(*parsed.error);
			}
			const ClearAreaRequest& request = parsed.request;
			int cleared = 0;
			{
				const HistoryIsolation isolation(editor);
				auto batch = editor.actionQueue->createBatch(ACTION_DELETE_TILES);
				batch->setLabel(request.label);
				auto action = editor.actionQueue->createAction(batch.get());
				for (int dy = 0; dy < request.area.height; ++dy) {
					for (int dx = 0; dx < request.area.width; ++dx) {
						const Tile* tile = editor.map.getTile(request.area.x + dx, request.area.y + dy, request.area.z);
						if (!tile) {
							continue;
						}
						auto changed = TileOperations::deepCopy(tile, editor.map);
						changed->items.clear();
						if (!request.keepGround) {
							changed->ground.reset();
						}
						changed->creature.reset();
						changed->spawn.reset();
						changed->setHouseID(0);
						changed->setZoneIds({});
						if (!request.keepFlags) {
							changed->setMapFlags(0);
						}
						TileOperations::update(changed.get());
						action->addChange(std::make_unique<Change>(std::move(changed)));
						++cleared;
					}
				}
				if (cleared > 0) {
					batch->addAndCommitAction(std::move(action));
					editor.addBatch(std::move(batch));
				}
			}
			RefreshAfterMapChange();
			return Ok({ { "tilesCleared", cleared }, { "undoIndex", editor.actionQueue->getCurrentIndex() } });
		}

		//=====================================================================
		// History

		enum class HistoryDirection {
			Undo,
			Redo,
		};

		Response HandleHistoryStep(Editor& editor, const nlohmann::json& body, HistoryDirection direction) {
			const StepsParseResult parsed = ParseHistorySteps(body);
			if (parsed.error) {
				return Fail(*parsed.error);
			}
			if (auto error = RequireCurrentEditor(editor)) {
				return Fail(*error);
			}
			int done = 0;
			{
				const HistoryIsolation isolation(editor);
				while (done < parsed.steps && (direction == HistoryDirection::Undo ? g_editors.DoUndo() : g_editors.DoRedo())) {
					++done;
				}
			}
			RefreshAfterMapChange();
			return Ok({
				{ direction == HistoryDirection::Undo ? "undone" : "redone", done },
				{ "index", editor.actionQueue->getCurrentIndex() },
				{ "canUndo", editor.actionQueue->canUndo() },
				{ "canRedo", editor.actionQueue->canRedo() },
			});
		}

		//=====================================================================
		// Export

		Response HandleExportRegion(Editor& editor, const nlohmann::json& body) {
			const ExportRegionParseResult parsed = ParseExportRegionRequest(body, editor.map.getWidth(), editor.map.getHeight());
			if (parsed.error) {
				return Fail(*parsed.error);
			}
			MapTab* tab = g_gui.GetCurrentMapTab();
			MapCanvas* canvas = tab ? tab->GetCanvas() : nullptr;
			if (!canvas || tab->GetEditor() != &editor) {
				return Fail({ 400, "no_view", "The open map has no visible view to render." });
			}
			const ExportRegionRequest& request = parsed.request;
			std::error_code exists_error;
			const std::filesystem::path target(std::u8string(reinterpret_cast<const char8_t*>(request.path.data()), request.path.size()));
			if (!request.overwrite && std::filesystem::exists(target, exists_error)) {
				return Fail({ 409, "file_exists", "The file already exists; pass \"overwrite\": true to replace it." });
			}
			if (const auto error = canvas->screenshot_controller->ExportRegion(wxstr(request.path), request.region, request.z)) {
				return Fail({ 400, "export_failed", *error });
			}
			return Ok({
				{ "path", request.path },
				{ "z", request.z },
				{ "width", (request.region.max_x - request.region.min_x + 1) * TILE_SIZE },
				{ "height", (request.region.max_y - request.region.min_y + 1) * TILE_SIZE },
			});
		}

		//=====================================================================
		// Routing

		Response DispatchForEditor(const RouteSpec& route, const nlohmann::json& body, Editor& editor);

		HistoryCounters CountersOf(const ActionQueue& queue) {
			return { queue.getPushedCount(), queue.getMergedCount() };
		}

		Response HandleBatch(Editor& editor, const nlohmann::json& body) {
			const BatchParseResult parsed = ParseBatchRequest(body);
			if (parsed.error) {
				return Fail(*parsed.error);
			}
			if (auto error = PrevalidateBatch(parsed.plan, editor.map.getWidth(), editor.map.getHeight())) {
				return Fail(*error);
			}
			const size_t start_index = editor.actionQueue->getCurrentIndex();
			const HistoryCounters start_counters = CountersOf(*editor.actionQueue);
			nlohmann::json results = nlohmann::json::array();
			std::optional<size_t> failed_op;
			bool moved_history = false;
			for (size_t index = 0; index < parsed.plan.ops.size(); ++index) {
				const BatchOp& op = parsed.plan.ops[index];
				const Response response = DispatchForEditor(*op.route, op.args, editor);
				results.push_back({ { "op", op.route->path.substr(1) }, { "status", response.status }, { "result", response.body } });
				if (op.route->id == RouteId::Undo || op.route->id == RouteId::Redo) {
					moved_history = true;
				}
				if (response.status >= 400) {
					failed_op = index;
					break;
				}
			}

			if (!failed_op) {
				return Ok({ { "results", std::move(results) }, { "startIndex", start_index }, { "index", editor.actionQueue->getCurrentIndex() } });
			}

			// Undo exactly the entries this batch pushed. The start index cannot be used: a full
			// history drops its oldest entries and shifts every index.
			bool rolled_back = false;
			if (parsed.plan.rollbackOnError && !RequireCurrentEditor(editor)) {
				const RollbackPlan plan = PlanBatchRollback(start_counters, CountersOf(*editor.actionQueue), editor.actionQueue->getCurrentIndex(), moved_history);
				size_t undone = 0;
				{
					const HistoryIsolation isolation(editor);
					while (undone < plan.undoSteps && g_editors.DoUndo()) {
						++undone;
					}
				}
				RefreshAfterMapChange();
				rolled_back = plan.complete && undone == plan.undoSteps;
			}
			Response response = Fail({ 400, "batch_failed", std::format("Operation {} ({}) failed.", *failed_op + 1, parsed.plan.ops[*failed_op].route->path.substr(1)) });
			response.body["results"] = std::move(results);
			response.body["rolledBack"] = rolled_back;
			response.body["startIndex"] = start_index;
			response.body["index"] = editor.actionQueue->getCurrentIndex();
			return response;
		}

		Response DispatchForEditor(const RouteSpec& route, const nlohmann::json& body, Editor& editor) {
			if (route.kind == RouteKind::Mutation) {
				if (auto error = CheckEditorAcceptsChanges(editor)) {
					return Fail(*error);
				}
			}
			switch (route.id) {
				case RouteId::Health:
					return HandleHealth();
				case RouteId::Capabilities:
					return Ok(CapabilitiesJson());
				case RouteId::Snapshot:
					return HandleSnapshot(editor);
				case RouteId::Brushes:
					return HandleBrushes(body);
				case RouteId::InspectTile:
					return HandleInspectTile(editor, body);
				case RouteId::InspectArea:
					return HandleInspectArea(editor, body);
				case RouteId::TerrainGrid:
					return HandleTerrainGrid(editor, body);
				case RouteId::Paint:
					return HandlePaint(editor, body);
				case RouteId::ClearArea:
					return HandleClearArea(editor, body);
				case RouteId::PreviewPatch:
					return HandlePreviewPatch(editor, body);
				case RouteId::ApplyPatch:
					return HandleApplyPatch(editor, body);
				case RouteId::Undo:
					return HandleHistoryStep(editor, body, HistoryDirection::Undo);
				case RouteId::Redo:
					return HandleHistoryStep(editor, body, HistoryDirection::Redo);
				case RouteId::History:
					return HandleHistory(editor);
				case RouteId::Batch:
					return HandleBatch(editor, body);
				case RouteId::ExportRegion:
					return HandleExportRegion(editor, body);
			}
			return Fail({ 404, "not_found", "Unknown AI Control API endpoint." });
		}

	} // namespace

	bool IsUserInteracting() {
		if (wxWindow::GetCapture() != nullptr || g_gui.IsPasting()) {
			return true;
		}
		MapTab* tab = g_gui.GetCurrentMapTab();
		const MapCanvas* canvas = tab ? tab->GetCanvas() : nullptr;
		if (!canvas) {
			return false;
		}
		if (canvas->screendragging) {
			return true;
		}
		if (const DrawingController* drawing = canvas->drawing_controller.get()) {
			if (drawing->IsDrawing() || drawing->IsDraggingDraw() || drawing->IsReplaceDragging()) {
				return true;
			}
		}
		if (const SelectionController* selection = canvas->selection_controller.get()) {
			if (selection->IsDragging() || selection->IsBoundboxSelection()) {
				return true;
			}
		}
		return false;
	}

	Response Dispatch(const RouteSpec& route, const nlohmann::json& body) {
		switch (route.id) {
			case RouteId::Health:
				return HandleHealth();
			case RouteId::Capabilities:
				return Ok(CapabilitiesJson());
			case RouteId::Brushes:
				return HandleBrushes(body);
			default:
				break;
		}
		Editor* editor = g_gui.GetCurrentEditor();
		if (!editor) {
			return Fail(NoDocument());
		}
		return DispatchForEditor(route, body, *editor);
	}

} // namespace AiControl
