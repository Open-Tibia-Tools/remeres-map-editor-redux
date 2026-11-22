//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "editor/operations/magic_wand.h"

#include "editor/connected_positions.h"
#include "editor/editor.h"
#include "editor/action.h"
#include "editor/action_queue.h"
#include "editor/selection.h"
#include "map/map.h"
#include "map/tile.h"
#include "map/tile_operations.h"
#include "game/item.h"
#include "game/spawn.h"
#include "game/creature.h"
#include "brushes/ground/ground_brush.h"
#include "brushes/wall/wall_brush.h"
#include "brushes/doodad/doodad_brush.h"
#include "app/settings.h"
#include "ui/gui.h"

#include <chrono>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <variant>
#include <vector>

namespace {

	using Clock = std::chrono::steady_clock;
	using Target = std::variant<GroundBrush*, WallBrush*, DoodadBrush*>;

	// Opens a cancellable progress bar only when the search runs long, and throttles its updates.
	// The bar is app-modal, so the map cannot be edited while it pumps events. A live client
	// gets no bar at all: pumping socket events there could close the editor under us.
	class SearchProgress {
	public:
		explicit SearchProgress(bool allowed) :
			allowed(allowed) { }

		// Returns false once the user cancelled.
		bool Tick(std::string_view phase, size_t done) {
			if (!allowed) {
				return true;
			}
			const auto now = Clock::now();
			if (now - last_update < std::chrono::milliseconds(100)) {
				return true;
			}
			last_update = now;
			if (!bar) {
				if (now - started < std::chrono::milliseconds(250)) {
					return true;
				}
				bar.emplace(wxstr(std::string(phase)), true, "Magic Wand");
			}
			g_gui.SetLoadText(wxstr(std::format("{} {}", phase, done)));
			return !g_gui.WasLoadCancelled();
		}

	private:
		const bool allowed;
		const Clock::time_point started = Clock::now();
		Clock::time_point last_update = started;
		std::optional<ScopedLoadingBar> bar;
	};

	struct Entry {
		Position position;
		bool with_ground; // Patch tiles of a ground target also take the ground item itself.
	};

	struct Collected {
		std::vector<Entry> entries;
		size_t matched_tiles = 0;
	};

	bool WallBelongsTo(WallBrush* wall, Item* item) {
		return item->isWall() && (item->getWallBrush() == wall || wall->hasWall(item));
	}

	std::optional<Target> PickTarget(const Tile& tile) {
		for (auto it = tile.items.rbegin(); it != tile.items.rend(); ++it) {
			Brush* brush = (*it)->getDoodadBrush();
			if (brush && brush->is<DoodadBrush>()) {
				return Target { brush->as<DoodadBrush>() };
			}
		}
		for (auto it = tile.items.rbegin(); it != tile.items.rend(); ++it) {
			if ((*it)->isWall()) {
				if (WallBrush* wall = (*it)->getWallBrush()) {
					return Target { wall };
				}
			}
		}
		if (tile.ground) {
			if (GroundBrush* ground = tile.getGroundBrush()) {
				return Target { ground };
			}
		}
		return std::nullopt;
	}

	// True when the item (not the ground of a patch tile) is part of the target brush.
	struct OwnsItem {
		Item* item;
		bool operator()(GroundBrush* ground) const {
			return item->isBorder() && ground->ownsBorderItem(item->getID());
		}
		bool operator()(WallBrush* wall) const {
			return WallBelongsTo(wall, item);
		}
		bool operator()(DoodadBrush* doodad) const {
			return doodad->ownsItem(item);
		}
	};

	struct Collect {
		Map& map;
		const Position& origin;
		SearchProgress& progress;

		std::optional<Collected> operator()(GroundBrush* ground) const {
			auto patch = ConnectedPositions::collect(
				origin,
				[&](const Position& position) {
					const Tile* tile = map.getTile(position);
					return tile && tile->getGroundBrush() == ground;
				},
				[&](size_t found) { return progress.Tick("Finding connected tiles...", found); }
			);
			if (!patch) {
				return std::nullopt;
			}

			Collected collected;
			collected.matched_tiles = patch->size();
			const PositionVector ring = ConnectedPositions::borderRing(*patch);
			collected.entries.reserve(patch->size() + ring.size());
			for (const Position& position : *patch) {
				collected.entries.push_back({ position, true });
			}
			for (const Position& position : ring) {
				if (map.getTile(position)) {
					collected.entries.push_back({ position, false });
				}
			}
			return collected;
		}

		std::optional<Collected> operator()(WallBrush* wall) const {
			auto run = ConnectedPositions::collect(
				origin,
				[&](const Position& position) {
					const Tile* tile = map.getTile(position);
					if (!tile) {
						return false;
					}
					for (const auto& item : tile->items) {
						if (WallBelongsTo(wall, item.get())) {
							return true;
						}
					}
					return false;
				},
				[&](size_t found) { return progress.Tick("Finding connected walls...", found); }
			);
			if (!run) {
				return std::nullopt;
			}

			Collected collected;
			collected.matched_tiles = run->size();
			collected.entries.reserve(run->size());
			for (const Position& position : *run) {
				collected.entries.push_back({ position, false });
			}
			return collected;
		}

		std::optional<Collected> operator()(DoodadBrush* doodad) const {
			Collected collected;
			size_t scanned = 0;
			for (auto& location : map.tiles()) {
				if ((++scanned % 1024) == 0 && !progress.Tick("Searching this floor...", scanned)) {
					return std::nullopt;
				}
				const Tile* tile = location.get();
				if (!tile || tile->getZ() != origin.z) {
					continue;
				}
				for (const auto& item : tile->items) {
					if (doodad->ownsItem(item.get())) {
						collected.entries.push_back({ tile->getPosition(), false });
						break;
					}
				}
			}
			collected.matched_tiles = collected.entries.size();
			return collected;
		}
	};

	std::string_view KindLabel(const Target& target) {
		struct Label {
			std::string_view operator()(GroundBrush*) const {
				return "tile(s) of";
			}
			std::string_view operator()(WallBrush*) const {
				return "wall tile(s) of";
			}
			std::string_view operator()(DoodadBrush*) const {
				return "tile(s) on this floor with";
			}
		};
		return std::visit(Label {}, target);
	}

	// deepCopy keeps the item order, so the copy and the live tile can be compared slot by slot.
	bool SameSelection(const Tile& live, const Tile& copy) {
		const auto selected = [](const auto& entity) { return entity && entity->isSelected(); };
		if (selected(live.ground) != selected(copy.ground) || selected(live.spawn) != selected(copy.spawn) || selected(live.creature) != selected(copy.creature)) {
			return false;
		}
		for (size_t index = 0; index < live.items.size(); ++index) {
			if (live.items[index]->isSelected() != copy.items[index]->isSelected()) {
				return false;
			}
		}
		return true;
	}

} // namespace

void MagicWand::Select(Editor& editor, const Position& origin, bool add_to_selection) {
	if (editor.selection.isBusy()) {
		return;
	}
	const Tile* origin_tile = editor.map.getTile(origin);
	if (!origin_tile) {
		return;
	}
	const std::optional<Target> target = PickTarget(*origin_tile);
	if (!target) {
		return;
	}

	std::optional<Collected> collected;
	{
		SearchProgress progress(!editor.live_manager.IsClient());
		collected = std::visit(Collect { editor.map, origin, progress }, *target);
	}
	if (!collected) {
		g_gui.SetStatusText("Magic wand cancelled, selection unchanged.");
		return;
	}

	// From here on no events are pumped: tiles are fetched and the action committed in one go.
	std::unique_ptr<Action> action = editor.actionQueue->createAction(ACTION_SELECT);
	std::unordered_set<const Tile*> targeted;
	targeted.reserve(collected->entries.size());
	for (const Entry& entry : collected->entries) {
		Tile* tile = editor.map.getTile(entry.position);
		if (!tile) {
			continue;
		}
		targeted.insert(tile);

		std::unique_ptr<Tile> copy = TileOperations::deepCopy(tile, editor.map);
		if (!add_to_selection) {
			TileOperations::deselect(copy.get());
		}
		if (entry.with_ground && copy->ground) {
			copy->ground->select();
		}
		for (const auto& item : copy->items) {
			if (std::visit(OwnsItem { item.get() }, *target)) {
				item->select();
			}
		}
		if (!SameSelection(*tile, *copy)) {
			action->addChange(std::make_unique<Change>(std::move(copy)));
		}
	}
	if (!add_to_selection) {
		for (Tile* tile : editor.selection) {
			if (!targeted.contains(tile)) {
				std::unique_ptr<Tile> copy = TileOperations::deepCopy(tile, editor.map);
				TileOperations::deselect(copy.get());
				action->addChange(std::make_unique<Change>(std::move(copy)));
			}
		}
	}

	editor.selection.commitPreparedAction(std::move(action));
	editor.selection.updateSelectionCount();
	const std::string name = std::visit([](const auto* brush) { return brush->getName(); }, *target);
	g_gui.SetStatusText(wxstr(std::format("Magic wand: {} {} {}", collected->matched_tiles, KindLabel(*target), name)));
}

bool MagicWand::IsToolEnabled() {
	return g_settings.getBoolean(Config::MAGIC_WAND_SELECTION);
}

void MagicWand::SetToolEnabled(bool enabled) {
	g_settings.setInteger(Config::MAGIC_WAND_SELECTION, enabled ? 1 : 0);
	if (enabled) {
		g_gui.SetSelectionMode();
	}
	g_gui.UpdateMenubar();
	g_gui.SetStatusText(enabled ? "Magic wand on: click a ground, wall or doodad. Ctrl adds to the selection, Shift drags a rectangle." : "Magic wand off.");
}
