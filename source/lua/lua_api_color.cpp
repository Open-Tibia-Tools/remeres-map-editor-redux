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
#include "lua_api.h"

#include <algorithm>
#include <cctype>

namespace LuaAPI {

	namespace {
		static wxColour parseColorObject(sol::state_view lua, const sol::object& obj, const wxColour& fallback) {
			if (!obj.valid()) {
				return fallback;
			}

			auto parseHex = [&fallback](const std::string& hexValue) -> wxColour {
				std::string h = (!hexValue.empty() && hexValue.front() == '#') ? hexValue.substr(1) : hexValue;
				if (h.length() == 3) {
					h = { h[0], h[0], h[1], h[1], h[2], h[2] };
				}

				if (h.length() != 6 || !std::all_of(h.begin(), h.end(), [](unsigned char c) { return std::isxdigit(c) != 0; })) {
					return fallback;
				}

				try {
					size_t parsed = 0;
					const unsigned long value = std::stoul(h, &parsed, 16);
					if (parsed != h.length()) {
						return fallback;
					}
					return wxColour(
						static_cast<unsigned char>((value >> 16) & 0xFF),
						static_cast<unsigned char>((value >> 8) & 0xFF),
						static_cast<unsigned char>(value & 0xFF),
						255
					);
				} catch (...) {
					return fallback;
				}
			};

			if (obj.is<std::string>()) {
				const std::string hexValue = obj.as<std::string>();
				wxColour parsed = parseHex(hexValue);
				if (parsed != fallback) {
					return parsed;
				}

				sol::table colorTable = lua["Color"];
				if (colorTable.valid()) {
					sol::object named = colorTable[hexValue];
					if (named.valid()) {
						return parseColorObject(lua, named, fallback);
					}
				}

				return fallback;
			}

			if (!obj.is<sol::table>()) {
				return fallback;
			}

			sol::table tbl = obj.as<sol::table>();
			const int r = std::clamp<int>(tbl.get_or(std::string("r"), tbl.get_or(std::string("red"), fallback.Red())), 0, 255);
			const int g = std::clamp<int>(tbl.get_or(std::string("g"), tbl.get_or(std::string("green"), fallback.Green())), 0, 255);
			const int b = std::clamp<int>(tbl.get_or(std::string("b"), tbl.get_or(std::string("blue"), fallback.Blue())), 0, 255);
			const int a = std::clamp<int>(tbl.get_or(std::string("a"), tbl.get_or(std::string("alpha"), fallback.Alpha())), 0, 255);
			return wxColour(r, g, b, a);
		}

		static sol::table makeColorTable(sol::state& lua, const wxColour& color) {
			sol::table c = lua.create_table();
			c["r"] = static_cast<int>(color.Red());
			c["g"] = static_cast<int>(color.Green());
			c["b"] = static_cast<int>(color.Blue());
			c["a"] = static_cast<int>(color.Alpha());
			return c;
		}
	} // namespace

	void registerColor(sol::state& lua) {
		sol::table Color = lua.create_table();

		// Constructor rgb
		Color["rgb"] = [&lua](int r, int g, int b) {
			return makeColorTable(lua, wxColour(r, g, b, 255));
		};

		// Constructor hex
		Color["hex"] = [&lua](const std::string& hex) {
			return makeColorTable(lua, parseColorObject(lua, sol::make_object(lua, hex), wxColour(0, 0, 0, 255)));
		};

		// Lighten/Darken helper using wxColour
		Color["lighten"] = [&lua](sol::object colorObj, int percent) {
			wxColour wx = parseColorObject(lua, colorObj, wxColour(0, 0, 0, 255));
			wxColour result = wx.ChangeLightness(100 + percent);

			return makeColorTable(lua, result);
		};

		Color["darken"] = [&lua](sol::object colorObj, int percent) {
			wxColour wx = parseColorObject(lua, colorObj, wxColour(0, 0, 0, 255));
			wxColour result = wx.ChangeLightness(100 - percent);

			return makeColorTable(lua, result);
		};

		// Helper to create a color table
		auto mkColor = [&lua](int r, int g, int b) {
			return makeColorTable(lua, wxColour(r, g, b, 255));
		};

		// Predefined colors
		Color["white"] = mkColor(255, 255, 255);
		Color["black"] = mkColor(0, 0, 0);
		Color["blue"] = mkColor(49, 130, 206);
		Color["red"] = mkColor(220, 53, 69);
		Color["green"] = mkColor(40, 167, 69);
		Color["yellow"] = mkColor(255, 193, 7);
		Color["orange"] = mkColor(253, 126, 20);
		Color["gray"] = mkColor(128, 128, 128);
		Color["lightGray"] = mkColor(245, 247, 250);
		Color["darkGray"] = mkColor(45, 55, 72);

		lua["Color"] = Color;
	}

} // namespace LuaAPI
