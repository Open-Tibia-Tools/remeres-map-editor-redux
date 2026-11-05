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
#include "lua_api_selection.h"
#include "editor/selection.h"
#include "map/tile.h"
#include "map/position.h"
#include "ui/gui.h"
#include "editor/editor.h"

#include <string>

namespace LuaAPI {

	namespace {
	class ScopedInternalSelectionSession {
	public:
		explicit ScopedInternalSelectionSession(Selection* selection) :
			selection(selection),
			ownsSession(selection && !selection->isBusy()) {
			if (ownsSession) {
				selection->start(Selection::INTERNAL);
			}
		}

		ScopedInternalSelectionSession(const ScopedInternalSelectionSession&) = delete;
		ScopedInternalSelectionSession& operator=(const ScopedInternalSelectionSession&) = delete;

		~ScopedInternalSelectionSession() {
			if (ownsSession && selection) {
				selection->finish(Selection::INTERNAL);
			}
		}

	private:
		Selection* selection;
		bool ownsSession;
	};

	template <typename Fn>
	void withInternalSelectionSession(Selection* selection, Fn fn) {
		if (!selection) {
			return;
		}

		const ScopedInternalSelectionSession guard(selection);
		fn();
	}
	} // namespace



	// Get tiles as a Lua table
	static sol::table getSelectionTiles(sol::this_state ts, Selection* sel) {
		sol::state_view lua(ts);
		sol::table result = lua.create_table();

		if (!sel) {
			return result;
		}

		int idx = 1;
		for (Tile* tile : sel->getTiles()) {
			if (tile) {
				result[idx++] = tile;
			}
		}
		return result;
	}

	// Get bounds as a table with min/max positions
	static sol::table getSelectionBounds(sol::this_state ts, Selection* sel) {
		sol::state_view lua(ts);
		sol::table result = lua.create_table();

		if (!sel || sel->size() == 0) {
			return result;
		}

		result["min"] = sel->minPosition();
		result["max"] = sel->maxPosition();
		return result;
	}

	void registerSelection(sol::state& lua) {
		// Register Selection usertype
		lua.new_usertype<Selection>(
			"Selection",
			// No public constructor - selection is obtained from app.selection
			sol::no_constructor,

			// Properties
			"isEmpty", sol::property([](Selection* sel) {
				return sel == nullptr || sel->size() == 0;
			}),
			"size", sol::property([](Selection* sel) -> size_t {
				return sel ? sel->size() : 0;
			}),
			"isBusy", sol::property([](Selection* sel) {
				return sel && sel->isBusy();
			}),

			// Tiles collection (as a table)
			"tiles", sol::property([](Selection* sel, sol::this_state ts) {
				return getSelectionTiles(ts, sel);
			}),

			// Bounds
			"bounds", sol::property([](Selection* sel, sol::this_state ts) -> sol::table {
				return getSelectionBounds(ts, sel);
			}),
			"minPosition", sol::property([](Selection* sel) -> sol::optional<Position> {
				if (!sel || sel->size() == 0) {
					return sol::nullopt;
				}
				return sel->minPosition();
			}),
			"maxPosition", sol::property([](Selection* sel) -> sol::optional<Position> {
				if (!sel || sel->size() == 0) {
					return sol::nullopt;
				}
				return sel->maxPosition();
			}),

			// Methods
			"start", sol::overload(
				[](Selection* sel) {
					if (sel && !sel->isBusy()) {
						sel->start(Selection::INTERNAL);
					}
				},
				[](Selection* sel, int flags) {
					if (sel && !sel->isBusy()) {
						sel->start(static_cast<Selection::SessionFlags>(flags));
					}
				}),
			"finish", sol::overload(
				[](Selection* sel) {
					if (sel && sel->isBusy() && sel->isDeferred()) {
						sel->finish(Selection::INTERNAL);
					}
				},
				[](Selection* sel, int flags) {
					if (!sel || !sel->isBusy()) {
						return;
					}

					if (sel->isDeferred()) {
						sel->finish(Selection::INTERNAL);
					} else {
						sel->finish(static_cast<Selection::SessionFlags>(flags));
					}
				}),
			"transaction", [](Selection* sel, sol::function callback) {
				if (!sel || !callback.valid()) {
					return;
				}

				const ScopedInternalSelectionSession guard(sel);
				callback();
			},
			"clear", [](Selection* sel) {
				withInternalSelectionSession(sel, [sel] {
					sel->clear();
				});
			},

			"add", sol::overload([](Selection* sel, Tile* tile) {
				if (sel && tile) {
					withInternalSelectionSession(sel, [sel, tile] {
						sel->add(tile);
					});
				}
			}, [](Selection* sel, Tile* tile, Item* item) {
				if (sel && tile && item) {
					withInternalSelectionSession(sel, [sel, tile, item] {
						sel->add(tile, item);
					});
				}
			}),

			"remove", sol::overload([](Selection* sel, Tile* tile) {
				if (sel && tile) {
					withInternalSelectionSession(sel, [sel, tile] {
						sel->remove(tile);
					});
				}
			}, [](Selection* sel, Tile* tile, Item* item) {
				if (sel && tile && item) {
					withInternalSelectionSession(sel, [sel, tile, item] {
						sel->remove(tile, item);
					});
				}
			}),

			// String representation
			sol::meta_function::to_string, [](Selection* sel) {
				if (!sel) {
					return std::string("Selection(invalid)");
				}
				return "Selection(size=" + std::to_string(sel->size()) + ")";
			}
		);
	}

} // namespace LuaAPI
