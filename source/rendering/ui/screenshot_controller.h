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

#ifndef RME_SCREENSHOT_CONTROLLER_H
#define RME_SCREENSHOT_CONTROLLER_H

#include <wx/wx.h>
#include <wx/filename.h>
#include <memory>
#include <optional>
#include <string>

class MapCanvas;
class ScreenshotSaver;
struct PositionRect;

class ScreenshotController {
public:
	ScreenshotController(MapCanvas* canvas);
	~ScreenshotController();

	void TakeScreenshot(const wxFileName& path, const wxString& format);
	// Renders the inclusive tile region of `floor` at 100% zoom, one viewport at a time, into a
	// PNG at `path`. The view's floor, zoom and scroll are restored afterwards. Returns an error
	// message on failure.
	[[nodiscard]] std::optional<std::string> ExportRegion(const wxString& path, const PositionRect& region, int floor);
	bool IsCapturing() const;
	uint8_t* GetBuffer();

private:
	// Captures one frame, re-rendering a bounded number of times while sprites are still loading.
	void CaptureSettledFrame(int width, int height);

	MapCanvas* canvas;
	std::unique_ptr<ScreenshotSaver> screenshot_saver;
};

#endif
