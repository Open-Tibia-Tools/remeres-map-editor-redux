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
#include "rendering/ui/screenshot_controller.h"
#include "rendering/ui/map_display.h"
#include "rendering/io/screenshot_saver.h"
#include "ui/gui.h"
#include "ui/dialog_util.h"
#include "ui/map_window.h"
#include "map/position_rect.h"
#include "rendering/core/map_view_math.h"
#include "rendering/core/sprite_preloader.h"

#include <wx/log.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>

namespace {

	// A frame drawn while the preloader still decodes sprites can miss them; the capture is
	// redrawn a few times, briefly waiting for the workers, before the frame is taken as is.
	constexpr int SpriteSettleRetryLimit = 8;
	constexpr auto SpriteSettleDelay = std::chrono::milliseconds(5);

	// Puts the view back (floor, zoom, scroll, minimap viewport) and frees the capture buffer on
	// every exit. Zooming re-attaches the minimap to the camera, so its state is restored as well.
	class ExportViewRestore {
	public:
		ExportViewRestore(MapCanvas& canvas, MapWindow& window, ScreenshotSaver& saver) :
			canvas_(canvas),
			window_(window),
			saver_(saver),
			floor_(canvas.GetFloor()),
			zoom_(canvas.GetZoom()),
			minimap_(window.GetMinimapViewportState()) {
			window_.GetViewStart(&scroll_x_, &scroll_y_);
		}
		~ExportViewRestore() {
			saver_.Cleanup();
			canvas_.ChangeFloor(floor_);
			canvas_.SetZoom(zoom_);
			window_.Scroll(scroll_x_, scroll_y_);
			window_.GetMinimapViewportState() = minimap_;
			canvas_.RequestLocalRefresh();
			g_gui.UpdateMinimap(true);
		}
		ExportViewRestore(const ExportViewRestore&) = delete;
		ExportViewRestore& operator=(const ExportViewRestore&) = delete;

	private:
		MapCanvas& canvas_;
		MapWindow& window_;
		ScreenshotSaver& saver_;
		int floor_;
		double zoom_;
		MinimapViewportState minimap_;
		int scroll_x_ = 0;
		int scroll_y_ = 0;
	};

} // namespace

ScreenshotController::ScreenshotController(MapCanvas* canvas) :
	canvas(canvas) {
	screenshot_saver = std::make_unique<ScreenshotSaver>();
}

ScreenshotController::~ScreenshotController() {
}

bool ScreenshotController::IsCapturing() const {
	return screenshot_saver->IsCapturing();
}

uint8_t* ScreenshotController::GetBuffer() {
	return screenshot_saver->GetBuffer();
}

void ScreenshotController::CaptureSettledFrame(int width, int height) {
	for (int attempt = 0;; ++attempt) {
		screenshot_saver->PrepareCapture(width, height);
		canvas->RequestLocalRefresh(true);
		if (attempt >= SpriteSettleRetryLimit || !SpritePreloader::get().hasPendingWork()) {
			return;
		}
		std::this_thread::sleep_for(SpriteSettleDelay);
	}
}

std::optional<std::string> ScreenshotController::ExportRegion(const wxString& path, const PositionRect& region, int floor) {
	if (region.empty()) {
		return "The region is empty.";
	}
	auto* window = dynamic_cast<MapWindow*>(canvas->GetParent());
	if (!window || !canvas->IsShownOnScreen()) {
		return "The map view must be visible.";
	}

	const ExportViewRestore restore(*canvas, *window, *screenshot_saver);
	canvas->ChangeFloor(floor);
	canvas->SetZoom(1.0);

	int viewport_width = 0;
	int viewport_height = 0;
	window->GetViewSize(&viewport_width, &viewport_height);
	if (viewport_width <= 0 || viewport_height <= 0) {
		return "The map view has no size.";
	}

	const int width = (region.max_x - region.min_x + 1) * TILE_SIZE;
	const int height = (region.max_y - region.min_y + 1) * TILE_SIZE;
	wxImage output(width, height, false);
	if (!output.IsOk()) {
		return "Cannot allocate the PNG image.";
	}

	const int origin_x = MainMapViewMath::WorldTileToScrollPixel(region.min_x, floor);
	const int origin_y = MainMapViewMath::WorldTileToScrollPixel(region.min_y, floor);
	unsigned char* destination = output.GetData();
	for (int y = 0; y < height; y += viewport_height) {
		for (int x = 0; x < width; x += viewport_width) {
			window->Scroll(origin_x + x, origin_y + y);
			int actual_x = 0;
			int actual_y = 0;
			window->GetViewStart(&actual_x, &actual_y);
			const int crop_x = origin_x + x - actual_x;
			const int crop_y = origin_y + y - actual_y;
			const int copy_width = std::min(viewport_width, width - x);
			const int copy_height = std::min(viewport_height, height - y);
			if (crop_x < 0 || crop_y < 0 || crop_x + copy_width > viewport_width || crop_y + copy_height > viewport_height) {
				return "The region reaches beyond the scrollable view; choose a region away from the map edge.";
			}

			CaptureSettledFrame(viewport_width, viewport_height);
			const uint8_t* pixels = screenshot_saver->GetBuffer();
			for (int row = 0; row < copy_height; ++row) {
				std::memcpy(
					destination + (static_cast<size_t>(y + row) * width + x) * 3,
					pixels + (static_cast<size_t>(crop_y + row) * viewport_width + crop_x) * 3,
					static_cast<size_t>(copy_width) * 3
				);
			}
		}
	}

	// The caller reports the failure; wxImage must not raise its own log dialog.
	const wxLogNull no_log_dialog;
	if (!output.SaveFile(path, wxBITMAP_TYPE_PNG)) {
		return "Could not write the PNG file.";
	}
	return std::nullopt;
}

void ScreenshotController::TakeScreenshot(const wxFileName& path, const wxString& format) {
	int view_scroll_x, view_scroll_y;
	int screensize_x, screensize_y;
	canvas->GetViewBox(&view_scroll_x, &view_scroll_y, &screensize_x, &screensize_y);

	screenshot_saver->PrepareCapture(screensize_x, screensize_y);

	// Draw the window
	canvas->RequestLocalRefresh();
	canvas->Update(); // Forces immediate redraws the window.

	// Buffer should now contain the screenbuffer

	// We need to re-get view size as it might be used in SaveCapture logic indirectly or just to be safe
	// But in original code: static_cast<MapWindow*>(GetParent())->GetViewSize(&screensize_x, &screensize_y);
	// In MapCanvas::GetViewBox we already do that.

	// Delegate saving to ScreenshotSaver
	wxString result = screenshot_saver->SaveCapture(path, format, screensize_x, screensize_y);

	if (result.StartsWith("Error:")) {
		DialogUtil::PopupDialog("Screenshot Error", result, wxOK);
	} else {
		g_gui.SetStatusText(result);
	}

	canvas->RequestLocalRefresh();

	screenshot_saver->Cleanup();
}
