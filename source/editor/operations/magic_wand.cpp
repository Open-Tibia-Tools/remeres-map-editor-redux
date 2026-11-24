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
#include <vector>

// Injective position key for one wand invocation. x owns the bits from 21 up, y the bits 4..20
// and z the bits 0..3, so two coordinates can only collide once the map limits are exceeded; the
// static_asserts below pin exactly those limits. The floor is part of the key because the plan's
// positions are all on the click floor while the cleared selection can hold tiles from any floor.
namespace {

	using Clock = std::chrono::steady_clock;
	using MagicWand::Entry;
	using MagicWand::Target;

	// Opens a cancellable progress bar only when the search runs long, and throttles its updates.
	// The bar is app-modal, so local input cannot edit the map while it pumps events, but a live
	// server still receives remote edits through those pumps, so callers must refetch tiles by
	// position instead of holding a Tile* across a tick. A live client gets no bar at all:
	// pumping socket events there could close the editor under us.
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
			std::string_view operator()(const MagicWand::ItemFamily&) const {
				return "tile(s) with";
			}
			std::string_view operator()(const MagicWand::CreatureTarget&) const {
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

	// y must stay below 2^17 so y << 4 cannot bleed into x's bits, x below 2^43 so x << 21
	// cannot overflow, and z at or below 0xF so the mask never folds two floors together.
	static_assert(MAP_MAX_WIDTH < (1ull << 43) && MAP_MAX_HEIGHT < (1 << 17) && MAP_MAX_LAYER <= 0xF, "positionKey bit ranges no longer hold");

} // namespace

namespace MagicWand {

	void Select(Editor& editor, const WandHit& hit, const PositionRect& viewport, bool add_to_selection, SearchMode mode) {
		if (editor.selection.isBusy() || !hit.tile) {
			return;
		}
		const TileLookup lookup = [&editor](const Position& position) {
			return static_cast<const Tile*>(editor.map.getTile(position));
		};

		const std::optional<ResolvedTarget> resolved = ResolveTarget(hit, lookup);
		if (!resolved) {
			g_gui.SetStatusText("Magic wand: nothing to select here.");
			return;
		}
		const Target& target = resolved->target;
		const TargetNames& names = resolved->names;
		// Nameless targets still deserve a readable status line.
		const std::string_view display_name = names.primary.empty() ? "(unnamed)" : std::string_view(names.primary);

		SearchProgress progress(!editor.live_manager.IsClient());
		const auto tick = [&progress](std::string_view phase, size_t done, size_t total) {
			return progress.Tick(phase, done, total);
		};

		std::optional<Plan> plan = BuildPlan(target, names, mode, lookup, resolved->origin, viewport, tick);
		if (!plan) {
			g_gui.SetStatusText("Magic wand cancelled, selection unchanged.");
			return;
		}
		if (plan->entries.empty()) {
			progress.Finish();
			g_gui.SetStatusText(wxstr(std::format("Magic wand: nothing visible for {}.", display_name)));
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
			SelectMatching(*copy, target, names, entry.select_ground);
			if (!SameSelection(*tile, *copy)) {
				action->addChange(std::make_unique<Change>(std::move(copy)));
			}
		}

		if (!add_to_selection) {
			// Snapshot positions, not Tile*: the ticked bar pumps events, and a remote edit
			// dispatched during a pump can swap or destroy the selected tiles (live server).
			std::vector<Position> previously_selected;
			for (Tile* tile : editor.selection) {
				if (tile) {
					previously_selected.push_back(tile->getPosition());
				}
			}
			for (size_t index = 0; index < previously_selected.size(); ++index) {
				if (((index + 1) % 256) == 0 && !tick("Clearing previous selection...", index + 1, previously_selected.size())) {
					g_gui.SetStatusText("Magic wand cancelled, selection unchanged.");
					return;
				}
				const Position& position = previously_selected[index];
				if (targeted.contains(positionKey(position))) {
					continue;
				}
				Tile* tile = editor.map.getTile(position);
				if (!tile) {
					continue;
				}
				std::unique_ptr<Tile> copy = TileOperations::deepCopy(tile, editor.map);
				TileOperations::deselect(copy.get());
				if (!SameSelection(*tile, *copy)) {
					action->addChange(std::make_unique<Change>(std::move(copy)));
				}
			}
		}

		progress.Finish();
		const size_t changed_tiles = action->size();
		editor.selection.commitPreparedAction(std::move(action));
		editor.selection.updateSelectionCount();
		g_gui.SetStatusText(wxstr(std::format("Magic wand: {} {} {}", changed_tiles, KindLabel(target), display_name)));
	}

} // namespace MagicWand
