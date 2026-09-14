#include "app/main.h"

#include "rendering/drawers/cursors/live_cursor_drawer.h"

#include "rendering/core/sprite_batch.h"
#include "rendering/core/render_view.h"
#include "editor/editor.h"
#include "live/live_socket.h"
#include "rendering/core/drawing_options.h"
#include "rendering/core/graphics.h"

void LiveCursorDrawer::draw(SpriteBatch& sprite_batch, const RenderView& view, Editor& editor, const DrawingOptions& options, const AtlasManager& atlas) {
	if (options.ingame || !editor.live_manager.IsLive()) {
		return;
	}

	LiveSocket& live = editor.live_manager.GetSocket();
	for (LiveCursor& cursor : live.getCursorList()) {
		if (cursor.pos.z <= GROUND_LAYER && view.floor > GROUND_LAYER) {
			continue;
		}

		if (cursor.pos.z > GROUND_LAYER && view.floor <= 8) {
			continue;
		}

		wxColor draw_color = cursor.color;
		if (cursor.pos.z < view.floor) {
			draw_color = wxColor(
				draw_color.Red(),
				draw_color.Green(),
				draw_color.Blue(),
				std::max<uint8_t>(static_cast<uint8_t>(draw_color.Alpha() / 2), static_cast<uint8_t>(64))
			);
		}

		int offset;
		if (cursor.pos.z <= GROUND_LAYER) {
			offset = (GROUND_LAYER - cursor.pos.z) * TILE_SIZE;
		} else {
			offset = TILE_SIZE * (view.floor - cursor.pos.z);
		}

		float draw_x = ((cursor.pos.x * TILE_SIZE) - view.view_scroll_x) - offset;
		float draw_y = ((cursor.pos.y * TILE_SIZE) - view.view_scroll_y) - offset;

		glm::vec4 color(
			draw_color.Red() / 255.0f,
			draw_color.Green() / 255.0f,
			draw_color.Blue() / 255.0f,
			draw_color.Alpha() / 255.0f
		);

		sprite_batch.drawRect(draw_x, draw_y, static_cast<float>(TILE_SIZE), static_cast<float>(TILE_SIZE), color, atlas);
	}
}

