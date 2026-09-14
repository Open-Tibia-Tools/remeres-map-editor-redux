#include "rendering/drawers/overlays/grid_drawer.h"
#include "rendering/core/sprite_batch.h"
#include "rendering/core/graphics.h"

#include "rendering/core/render_view.h"
#include "rendering/core/drawing_options.h"
#include "app/definitions.h"
#include <wx/gdicmn.h>

void GridDrawer::DrawGrid(SpriteBatch& sprite_batch, const RenderView& view, const DrawingOptions& options, const ViewBounds& bounds, const AtlasManager& atlas) {
	if (!options.show_grid) {
		return;
	}

	glm::vec4 color(0.0f, 0.0f, 0.0f, 0.50f); // Subtle dark overlay

	// Use zoom as line thickness so lines are always ~1 screen pixel
	const float line_thickness = view.zoom;

	// Hoisted invariants for horizontal lines
	const float h_xStart = bounds.start_x * TILE_SIZE - view.view_scroll_x;
	const float h_xEnd = bounds.end_x * TILE_SIZE - view.view_scroll_x;
	const float h_width = h_xEnd - h_xStart;

	for (int y = bounds.start_y; y <= bounds.end_y; ++y) {
		float yPos = y * TILE_SIZE - view.view_scroll_y;
		sprite_batch.drawRect(h_xStart, yPos, h_width, line_thickness, color, atlas);
	}

	// Hoisted invariants for vertical lines
	const float v_yStart = bounds.start_y * TILE_SIZE - view.view_scroll_y;
	const float v_yEnd = bounds.end_y * TILE_SIZE - view.view_scroll_y;
	const float v_height = v_yEnd - v_yStart;

	for (int x = bounds.start_x; x <= bounds.end_x; ++x) {
		float xPos = x * TILE_SIZE - view.view_scroll_x;
		sprite_batch.drawRect(xPos, v_yStart, line_thickness, v_height, color, atlas);
	}
}

void GridDrawer::DrawIngameBox(SpriteBatch& sprite_batch, const RenderView& view, const DrawingOptions& options, const ViewBounds& bounds, const AtlasManager& atlas) {
	if (!options.show_ingame_box) {
		return;
	}

	int center_x = bounds.start_x + int(view.screensize_x * view.zoom / 64);
	int center_y = bounds.start_y + int(view.screensize_y * view.zoom / 64);

	int offset_y = 2;
	int box_start_map_x = center_x;
	int box_start_map_y = center_y + offset_y;
	int box_end_map_x = center_x + ClientMapWidth;
	int box_end_map_y = center_y + ClientMapHeight + offset_y;

	int box_start_x = box_start_map_x * TILE_SIZE - view.view_scroll_x;
	int box_start_y = box_start_map_y * TILE_SIZE - view.view_scroll_y;
	int box_end_x = box_end_map_x * TILE_SIZE - view.view_scroll_x;
	int box_end_y = box_end_map_y * TILE_SIZE - view.view_scroll_y;

	static wxColor side_color(0, 0, 0, 200);

	// left side
	if (box_start_map_x >= bounds.start_x) {
		drawFilledRect(sprite_batch, 0, 0, box_start_x, view.screensize_y * view.zoom, side_color, atlas);
	}

	// right side
	if (box_end_map_x < bounds.end_x) {
		drawFilledRect(sprite_batch, box_end_x, 0, view.screensize_x * view.zoom, view.screensize_y * view.zoom, side_color, atlas);
	}

	// top side
	if (box_start_map_y >= bounds.start_y) {
		drawFilledRect(sprite_batch, box_start_x, 0, box_end_x - box_start_x, box_start_y, side_color, atlas);
	}

	// bottom side
	if (box_end_map_y < bounds.end_y) {
		drawFilledRect(sprite_batch, box_start_x, box_end_y, box_end_x - box_start_x, view.screensize_y * view.zoom, side_color, atlas);
	}

	// hidden tiles
	drawRect(sprite_batch, box_start_x, box_start_y, box_end_x - box_start_x, box_end_y - box_start_y, *wxRED, atlas);

	// visible tiles
	box_start_x += TILE_SIZE;
	box_start_y += TILE_SIZE;
	box_end_x -= 1 * TILE_SIZE;
	box_end_y -= 1 * TILE_SIZE;
	drawRect(sprite_batch, box_start_x, box_start_y, box_end_x - box_start_x, box_end_y - box_start_y, *wxGREEN, atlas);

	// player position
	box_start_x += (ClientMapWidth - 3) / 2 * TILE_SIZE;
	box_start_y += (ClientMapHeight - 3) / 2 * TILE_SIZE;
	box_end_x = box_start_x + TILE_SIZE;
	box_end_y = box_start_y + TILE_SIZE;
	drawRect(sprite_batch, box_start_x, box_start_y, box_end_x - box_start_x, box_end_y - box_start_y, *wxGREEN, atlas);
}

void GridDrawer::DrawNodeLoadingPlaceholder(SpriteBatch& sprite_batch, int nd_map_x, int nd_map_y, const RenderView& view, const AtlasManager& atlas) {
	int cy = (nd_map_y)*TILE_SIZE - view.view_scroll_y - view.getFloorAdjustment();
	int cx = (nd_map_x)*TILE_SIZE - view.view_scroll_x - view.getFloorAdjustment();

	glm::vec4 color(1.0f, 0.0f, 1.0f, 0.5f); // 255, 0, 255, 128
	sprite_batch.drawRect((float)cx, (float)cy, (float)TILE_SIZE * 4, (float)TILE_SIZE * 4, color, atlas);
}

void GridDrawer::drawRect(SpriteBatch& sprite_batch, int x, int y, int w, int h, const wxColor& color, const AtlasManager& atlas, int width) {
	glm::vec4 c(color.Red() / 255.0f, color.Green() / 255.0f, color.Blue() / 255.0f, color.Alpha() / 255.0f);
	sprite_batch.drawRectLines((float)x, (float)y, (float)w, (float)h, c, atlas);
}

void GridDrawer::drawFilledRect(SpriteBatch& sprite_batch, int x, int y, int w, int h, const wxColor& color, const AtlasManager& atlas) {
	glm::vec4 c(color.Red() / 255.0f, color.Green() / 255.0f, color.Blue() / 255.0f, color.Alpha() / 255.0f);
	sprite_batch.drawRect((float)x, (float)y, (float)w, (float)h, c, atlas);
}
