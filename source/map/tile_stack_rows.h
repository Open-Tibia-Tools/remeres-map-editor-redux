#ifndef RME_MAP_TILE_STACK_ROWS_H_
#define RME_MAP_TILE_STACK_ROWS_H_

#include <cstddef>
#include <optional>

// Row layout of the Tile Properties item list: the ground (when present) is row 0,
// followed by the stacked items in tile order. Pure index math, no Tile dependency.
namespace TileStackRows {

	struct Row {
		bool is_ground = false;
		// Index into Tile::items; only meaningful when !is_ground.
		std::size_t item_index = 0;
	};

	constexpr std::size_t RowCount(bool has_ground, std::size_t item_count) {
		return item_count + (has_ground ? 1 : 0);
	}

	constexpr std::optional<Row> Resolve(int row, bool has_ground, std::size_t item_count) {
		if (row < 0 || static_cast<std::size_t>(row) >= RowCount(has_ground, item_count)) {
			return std::nullopt;
		}
		if (has_ground) {
			if (row == 0) {
				return Row { .is_ground = true };
			}
			return Row { .is_ground = false, .item_index = static_cast<std::size_t>(row - 1) };
		}
		return Row { .is_ground = false, .item_index = static_cast<std::size_t>(row) };
	}

	// The ground is not part of the item stack, so it never moves and items never move below it.
	constexpr bool CanMoveUp(int row, bool has_ground, std::size_t item_count) {
		const auto resolved = Resolve(row, has_ground, item_count);
		return resolved && !resolved->is_ground && resolved->item_index > 0;
	}

	constexpr bool CanMoveDown(int row, bool has_ground, std::size_t item_count) {
		const auto resolved = Resolve(row, has_ground, item_count);
		return resolved && !resolved->is_ground && resolved->item_index + 1 < item_count;
	}

	// Row a context menu acts on: the row under the mouse, or the selected row when the menu is opened
	// from the keyboard. Empty space below the rows yields no row, so no menu is shown there.
	constexpr std::optional<int> ContextMenuRow(bool from_keyboard, int hit_row, int selected_row, bool has_ground, std::size_t item_count) {
		const int row = from_keyboard ? selected_row : hit_row;
		if (!Resolve(row, has_ground, item_count)) {
			return std::nullopt;
		}
		return row;
	}

	// Row to select after `deleted_row` was removed from a list that now has `remaining_rows` rows:
	// the row just before it, or the new first row, or none when the list is empty.
	constexpr std::optional<int> FallbackRowAfterDelete(int deleted_row, std::size_t remaining_rows) {
		if (remaining_rows == 0) {
			return std::nullopt;
		}
		if (deleted_row > 0) {
			const int previous = deleted_row - 1;
			const int last = static_cast<int>(remaining_rows) - 1;
			return previous < last ? previous : last;
		}
		return 0;
	}

} // namespace TileStackRows

#endif
