//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"
#include "editor/operations/magic_wand.h"

#include "editor/action.h"
#include "editor/action_queue.h"
#include "editor/editor.h"
#include "editor/selection.h"
#include "map/map.h"
#include "map/tile.h"
#include "map/tile_operations.h"
#include "ui/gui.h"

#include <algorithm>
#include <chrono>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>

namespace {

	using Clock = std::chrono::steady_clock;
	using MagicWand::CreatureTarget;
	using MagicWand::Entry;
	using MagicWand::ItemFamily;
	using MagicWand::Plan;
	using MagicWand::Target;

	// Opens a cancellable progress bar only when the search runs long, and throttles its updates.
	// The bar is app-modal, so the map cannot be edited while it pumps events. A live client
	// gets no bar at all: pumping socket events there could close the editor under us.
	class SearchProgress {
	public:
		explicit SearchProgress(bool allowed) :
			allowed(allowed) { }

		// Returns false once the user cancelled.
		bool Tick(std::string_view phase, size_t done, size_t total) {
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
			const wxString text = wxstr(std::format("{} {}", phase, done));
			if (total > 0) {
				// Never report 100% here: SetLoadDone(100) destroys the bar, and only Finish()/
				// ~ScopedLoadingBar may end the operation (one broadcast, no stuck live clients).
				const size_t percent = std::min<size_t>(99, done * 100 / total);
				g_gui.SetLoadDone(static_cast<int32_t>(percent), text);
			} else {
				g_gui.SetLoadText(text);
			}
			return !g_gui.WasLoadCancelled();
		}

		// Closing the bar also finishes the live-server operation (see LoadingManager::DestroyLoadBar).
		void Finish() {
			bar.reset();
		}

	private:
		const bool allowed;
		const Clock::time_point started = Clock::now();
		Clock::time_point last_update = started;
		std::optional<ScopedLoadingBar> bar;
	};

	std::string_view KindLabel(const Target& target) {
		struct Label {
			std::string_view operator()(GroundBrush*) const {
				return "tile(s) of";
			}
			std::string_view operator()(WallBrush*) const {
				return "wall tile(s) of";
			}
			std::string_view operator()(CarpetBrush*) const {
				return "carpet tile(s) of";
			}
			std::string_view operator()(TableBrush*) const {
				return "table tile(s) of";
			}
			std::string_view operator()(DoodadBrush*) const {
				return "tile(s) on this floor with";
			}
			std::string_view operator()(const ItemFamily&) const {
				return "tile(s) with";
			}
			std::string_view operator()(const CreatureTarget&) const {
				return "creature(s)";
			}
		};
		return std::visit(Label {}, target);
	}

	uint64_t positionKey(const Position& position) {
		const uint64_t x = static_cast<uint64_t>(static_cast<uint32_t>(position.x));
		const uint64_t y = static_cast<uint64_t>(static_cast<uint32_t>(position.y));
		const uint64_t z = static_cast<uint64_t>(position.z & 0xF);
		return (x << 21) | (y << 4) | z;
	}

} // namespace

namespace MagicWand {

	void Select(Editor& editor, const WandHit& hit, const PositionRect& viewport, bool add_to_selection) {
		if (editor.selection.isBusy() || !hit.tile) {
			return;
		}
		const std::optional<Target> target = ResolveTarget(hit);
		if (!target) {
			g_gui.SetStatusText("Magic wand: nothing to select here.");
			return;
		}
		const std::string target_name = DescribeTarget(*target);

		SearchProgress progress(!editor.live_manager.IsClient());
		const auto tick = [&progress](std::string_view phase, size_t done, size_t total) {
			return progress.Tick(phase, done, total);
		};
		const TileLookup lookup = [&editor](const Position& position) {
			return static_cast<const Tile*>(editor.map.getTile(position));
		};

		std::optional<Plan> plan = BuildPlan(*target, lookup, hit.tile->getPosition(), viewport, tick);
		if (!plan) {
			g_gui.SetStatusText("Magic wand cancelled, selection unchanged.");
			return;
		}
		if (plan->entries.empty()) {
			progress.Finish();
			g_gui.SetStatusText(wxstr(std::format("Magic wand: nothing visible for {}.", target_name)));
			return;
		}

		std::unique_ptr<Action> action = editor.actionQueue->createAction(ACTION_SELECT);
		// Positions describe the same tiles even if a pumped event swaps them, so this stays valid.
		std::unordered_set<uint64_t> targeted;
		targeted.reserve(plan->entries.size());
		for (size_t index = 0; index < plan->entries.size(); ++index) {
			if ((index % 256) == 0 && !tick("Selecting tiles...", index, plan->entries.size())) {
				g_gui.SetStatusText("Magic wand cancelled, selection unchanged.");
				return;
			}
			const Entry& entry = plan->entries[index];
			Tile* tile = editor.map.getTile(entry.position);
			if (!tile) {
				continue;
			}
			targeted.insert(positionKey(entry.position));

			std::unique_ptr<Tile> copy = TileOperations::deepCopy(tile, editor.map);
			if (!add_to_selection) {
				TileOperations::deselect(copy.get());
			}
			SelectMatching(*copy, *target, entry.select_ground, target_name);
			if (!SameSelection(*tile, *copy)) {
				action->addChange(std::make_unique<Change>(std::move(copy)));
			}
		}

		if (!add_to_selection) {
			for (Tile* tile : editor.selection) {
				if (!tile || targeted.contains(positionKey(tile->getPosition()))) {
					continue;
				}
				std::unique_ptr<Tile> copy = TileOperations::deepCopy(tile, editor.map);
				TileOperations::deselect(copy.get());
				action->addChange(std::make_unique<Change>(std::move(copy)));
			}
		}

		progress.Finish();
		editor.selection.commitPreparedAction(std::move(action));
		editor.selection.updateSelectionCount();
		g_gui.SetStatusText(wxstr(std::format("Magic wand: {} {} {}", plan->target_tiles, KindLabel(*target), target_name)));
	}

} // namespace MagicWand
