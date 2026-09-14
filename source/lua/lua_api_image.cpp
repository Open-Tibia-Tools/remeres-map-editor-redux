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
#include "lua_api_image.h"
#include "ui/gui.h"
#include "rendering/core/graphics.h"
#include "rendering/core/game_sprite.h"
#include "ui/icons/sprite_icon_service.h"
#include "rendering/core/normal_image.h"
#include "game/items.h"
#include <filesystem>
#include <cstring>
#include <vector>
#include "lua_script_manager.h"
#include "util/file_system.h"

namespace LuaAPI {

	static bool imagesHaveSamePixels(const wxImage& lhs, const wxImage& rhs) {
		if (lhs.IsOk() != rhs.IsOk()) {
			return false;
		}
		if (!lhs.IsOk()) {
			return true;
		}
		if (lhs.GetWidth() != rhs.GetWidth() || lhs.GetHeight() != rhs.GetHeight()) {
			return false;
		}
		if (lhs.HasAlpha() != rhs.HasAlpha()) {
			return false;
		}

		const size_t rgbBytes = static_cast<size_t>(lhs.GetWidth()) * static_cast<size_t>(lhs.GetHeight()) * 3;
		if (std::memcmp(lhs.GetData(), rhs.GetData(), rgbBytes) != 0) {
			return false;
		}
		if (lhs.HasAlpha()) {
			const size_t alphaBytes = static_cast<size_t>(lhs.GetWidth()) * static_cast<size_t>(lhs.GetHeight());
			if (std::memcmp(lhs.GetAlpha(), rhs.GetAlpha(), alphaBytes) != 0) {
				return false;
			}
		}
		return true;
	}

	LuaImage::LuaImage() :
		spriteId(0), spriteSource(false) {
		// Empty image
	}

	LuaImage::LuaImage(const std::string& path) :
		filePath(path), spriteId(0), spriteSource(false) {

		if (path.empty()) {
			return;
		}

		namespace fs = std::filesystem;
		fs::path scriptsPath = fs::weakly_canonical(LuaScriptManager::getInstance().getScriptsDirectory());
		fs::path dataPath = fs::weakly_canonical(FileSystem::GetDataDirectory().ToStdString());
		fs::path execPath = fs::weakly_canonical(FileSystem::GetExecDirectory().ToStdString());
		std::vector<fs::path> lookupRoots;

		try {
			std::string activeScriptDir = LuaScriptManager::getInstance().getEngine().getState()["SCRIPT_DIR"].get_or(std::string(""));
			if (!activeScriptDir.empty()) {
				fs::path packagePath = fs::path(activeScriptDir);
				if (fs::exists(packagePath)) {
					if (fs::is_regular_file(packagePath)) {
						packagePath = packagePath.parent_path();
					}
					lookupRoots.push_back(fs::weakly_canonical(packagePath));
				}
			}
		} catch (...) {
			// Ignore failures and continue with the default search roots.
		}
		lookupRoots.push_back(scriptsPath);
		lookupRoots.push_back(dataPath);
		lookupRoots.push_back(execPath);

		fs::path fullPath;
		bool allowed = false;

		try {
			fs::path p(path);
			if (p.is_absolute()) {
				fullPath = fs::weakly_canonical(p);
				// Check if absolute path is within allowed roots
				for (const auto& root : lookupRoots) {
					auto relative = fullPath.lexically_relative(root);
					if (!relative.empty() && relative.string().find("..") == std::string::npos) {
						if (fs::is_regular_file(fullPath)) {
							allowed = true;
							break;
						}
					}
				}
			} else {
				// For relative paths, try anchoring to each root
				for (const auto& root : lookupRoots) {
					fs::path candidate = fs::weakly_canonical(root / p);
					auto relative = candidate.lexically_relative(root);
					if (!relative.empty() && relative.string().find("..") == std::string::npos) {
						if (fs::is_regular_file(candidate)) {
							fullPath = candidate;
							allowed = true;
							break;
						}
					}
				}
			}
		} catch (...) {
			printf("[Lua Security] Failed to canonicalize image path: %s\n", path.c_str());
			return;
		}

		if (!allowed) {
			printf("[Lua Security] Blocked image path outside allowed directories: %s\n", path.c_str());
			return;
		}
		
		filePath = fullPath.string();

		if (!filePath.empty()) {
			image.LoadFile(wxString(filePath));
		}
	}

	LuaImage::LuaImage(int id, bool isItemSprite) :
		spriteId(0), spriteSource(false) {
		if (isItemSprite) {
			// Get sprite ID from item type
			if (g_items.typeExists(id)) {
				ItemType itemType = g_items.getItemType(id);
				if (itemType.id != 0) {
					loadFromSpriteId(itemType.clientID);
					if (image.IsOk()) {
						spriteId = itemType.clientID;
						spriteSource = true;
					}
				}
			}
		} else {
			loadFromSpriteId(id);
			if (image.IsOk()) {
				spriteId = id;
				spriteSource = true;
			}
		}
	}

	LuaImage::LuaImage(const LuaImage& other) :
		image(other.image.IsOk() ? other.image.Copy() : wxImage()),
		filePath(other.filePath),
		spriteId(other.spriteId),
		spriteSource(other.spriteSource) {
	}

	LuaImage& LuaImage::operator=(const LuaImage& other) {
		if (this != &other) {
			image = other.image.IsOk() ? other.image.Copy() : wxImage();
			filePath = other.filePath;
			spriteId = other.spriteId;
			spriteSource = other.spriteSource;
		}
		return *this;
	}

	LuaImage::~LuaImage() {
		// wxImage handles its own cleanup
	}

	LuaImage LuaImage::loadFromFile(const std::string& path) {
		return LuaImage(path);
	}

	LuaImage LuaImage::loadFromItemSprite(int itemId) {
		return LuaImage(itemId, true);
	}

	LuaImage LuaImage::loadFromSprite(int spriteId) {
		return LuaImage(spriteId, false);
	}

	// Helper to reliably get sprite ID from item ID, handling client ID mapping
	int getItemSpriteId(int itemId) {
		if (!g_items.typeExists(itemId)) {
			return 0;
		}
		ItemType itemType = g_items.getItemType(itemId);
		return itemType.clientID;
	}

	void LuaImage::loadFromSpriteId(int id) {
		Sprite* sprite = g_gui.gfx.getSprite(id);
		if (!sprite) {
			return;
		}

		GameSprite* gameSprite = dynamic_cast<GameSprite*>(sprite);
		if (gameSprite && gameSprite->width > 0 && gameSprite->height > 0) {
			// Calculate full sprite size (can be larger than 32x32 for multi-tile sprites)
			int spriteWidth = gameSprite->width * 32;
			int spriteHeight = gameSprite->height * 32;

			// Create image with alpha channel
			image.Create(spriteWidth, spriteHeight);
			image.InitAlpha();

			// Fill with transparent background
			unsigned char* imgData = image.GetData();
			unsigned char* alphaData = image.GetAlpha();
			memset(imgData, 0, spriteWidth * spriteHeight * 3);
			memset(alphaData, 0, spriteWidth * spriteHeight); // Fully transparent

			// Get sprite data for each part
			for (int y = 0; y < gameSprite->height; ++y) {
				for (int x = 0; x < gameSprite->width; ++x) {
					size_t spriteIndex = gameSprite->getIndex(x, y, 0, 0, 0, 0, 0);
					if (spriteIndex < gameSprite->spriteList.size()) {
						auto* normalImage = gameSprite->spriteList[spriteIndex];
						if (normalImage) {
							std::unique_ptr<uint8_t[]> rgbaData = normalImage->getRGBAData();
							if (rgbaData) {
								// Copy pixel data to the correct position
								int destX = (gameSprite->width - 1 - x) * 32;
								int destY = (gameSprite->height - 1 - y) * 32;

								for (int py = 0; py < 32; ++py) {
									for (int px = 0; px < 32; ++px) {
										int srcIdx = (py * 32 + px) * 4;
										int destIdx = (destY + py) * spriteWidth + (destX + px);

										// Only copy non-transparent pixels
										uint8_t alpha = rgbaData[srcIdx + 3];
										if (alpha > 0) {
											imgData[destIdx * 3 + 0] = rgbaData[srcIdx + 0]; // R
											imgData[destIdx * 3 + 1] = rgbaData[srcIdx + 1]; // G
											imgData[destIdx * 3 + 2] = rgbaData[srcIdx + 2]; // B
											alphaData[destIdx] = alpha;
										}
									}
								}
							}
						}
					}
				}
			}
			return;
		}

		// Fallback: use DC-based rendering
		wxBitmap bmp(32, 32, 32);
		wxMemoryDC dc(bmp);
		dc.SetBackground(*wxWHITE_BRUSH);
		dc.Clear();
		SpriteIconService::DrawTo(sprite, &dc, SPRITE_SIZE_32x32, 0, 0, 32, 32);
		dc.SelectObject(wxNullBitmap);
		image = bmp.ConvertToImage();
	}

	int LuaImage::getWidth() const {
		return image.IsOk() ? image.GetWidth() : 0;
	}

	int LuaImage::getHeight() const {
		return image.IsOk() ? image.GetHeight() : 0;
	}

	bool LuaImage::isValid() const {
		return image.IsOk();
	}

	LuaImage LuaImage::resize(int width, int height, bool smooth) const {
		LuaImage result;
		if (image.IsOk() && width > 0 && height > 0) {
			wxImageResizeQuality quality = smooth ? wxIMAGE_QUALITY_HIGH : wxIMAGE_QUALITY_NEAREST;
			result.image = image.Scale(width, height, quality);
			// Do not inherit identity fields for resized images
			result.filePath = "";
			result.spriteId = 0;
			result.spriteSource = false;
		}
		return result;
	}

	LuaImage LuaImage::scale(double factor, bool smooth) const {
		if (factor <= 0 || !image.IsOk()) {
			return LuaImage();
		}
		int w = static_cast<int>(image.GetWidth() * factor);
		int h = static_cast<int>(image.GetHeight() * factor);

		LuaImage result = resize(w, h, smooth);
		// resize already cleared identity fields
		return result;
	}

	wxBitmap LuaImage::getBitmap() const {
		if (!image.IsOk()) {
			return wxNullBitmap;
		}
		return wxBitmap(image);
	}

	wxBitmap LuaImage::getBitmap(int width, int height, bool smooth) const {
		if (!image.IsOk() || width <= 0 || height <= 0) {
			return wxNullBitmap;
		}
		wxImageResizeQuality quality = smooth ? wxIMAGE_QUALITY_HIGH : wxIMAGE_QUALITY_NEAREST;
		wxImage scaled = image.Scale(width, height, quality);
		return wxBitmap(scaled);
	}

	bool LuaImage::operator==(const LuaImage& other) const {
		if (spriteSource && other.spriteSource) {
			return spriteId == other.spriteId;
		}
		if (!spriteSource && !other.spriteSource) {
			return imagesHaveSamePixels(image, other.image);
		}
		return false;
	}

	void registerImage(sol::state& lua) {
		// Register LuaImage as "Image" usertype
		lua.new_usertype<LuaImage>(
			"Image",
			// Constructors
			sol::constructors<
				LuaImage(),
				LuaImage(const std::string&)>(),

			// Alternative constructor from table: Image{path = "..."} or Image{itemid = 100}
			sol::call_constructor, sol::factories(
									   // Default constructor
									   []() { return LuaImage(); },
									   // Path constructor
									   [](const std::string& path) { return LuaImage(path); },
									   // Table constructor
									   [](sol::table t) {
										   if (t["path"].valid()) {
											   return LuaImage::loadFromFile(t.get<std::string>("path"));
										   }
										   if (t["itemid"].valid()) {
											   return LuaImage::loadFromItemSprite(t.get<int>("itemid"));
										   }
										   if (t["spriteid"].valid()) {
											   return LuaImage::loadFromSprite(t.get<int>("spriteid"));
										   }
										   return LuaImage();
									   }
								   ),

			// Static factory methods
			"fromFile", &LuaImage::loadFromFile,
			"fromItemSprite", &LuaImage::loadFromItemSprite,
			"fromSprite", &LuaImage::loadFromSprite,

			// Properties (read-only)
			"width", sol::property(&LuaImage::getWidth),
			"height", sol::property(&LuaImage::getHeight),
			"valid", sol::property(&LuaImage::isValid),
			"path", sol::property(&LuaImage::getPath),
			"spriteId", sol::property(&LuaImage::getSpriteId),
			"isFromSprite", sol::property(&LuaImage::isSpriteSource),

			// Methods
			"resize", [](const LuaImage& img, int w, int h, sol::optional<bool> smooth) { return img.resize(w, h, smooth.value_or(true)); },
			"scale", [](const LuaImage& img, double factor, sol::optional<bool> smooth) { return img.scale(factor, smooth.value_or(true)); },

			// Equality comparison
			sol::meta_function::equal_to, &LuaImage::operator==,

			// String representation
			sol::meta_function::to_string, [](const LuaImage& img) {
			if (!img.isValid()) {
				return std::string("Image(invalid)");
			}
			if (img.isSpriteSource()) {
				return "Image(sprite=" + std::to_string(img.getSpriteId()) +
					   ", " + std::to_string(img.getWidth()) + "x" + std::to_string(img.getHeight()) + ")";
			}
			return "Image(\"" + img.getPath() + "\", " +
				   std::to_string(img.getWidth()) + "x" + std::to_string(img.getHeight()) + ")"; }
		);
	}

} // namespace LuaAPI
