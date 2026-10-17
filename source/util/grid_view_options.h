#ifndef RME_UTIL_GRID_VIEW_OPTIONS_H_
#define RME_UTIL_GRID_VIEW_OPTIONS_H_

/**
 * View options shared by the palette tileset grid and the item grids of the
 * replace tool, so a single toolbar can drive both.
 */
enum class TilesetSortKey {
	ID,
	Name
};

enum class TilesetSortDirection {
	Ascending,
	Descending
};

#endif
