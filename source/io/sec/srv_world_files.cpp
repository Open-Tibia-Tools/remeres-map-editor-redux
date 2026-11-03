//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "io/sec/srv_world_files.h"

#include "io/sec/sec_file_io.h"
#include "io/sec/srv_script_lexer.h"

#include <algorithm>
#include <array>
#include <format>
#include <iterator>
#include <optional>

namespace rme::sec {

	namespace {

		using srv::equalsIgnoreCase;
		using srv::Field;
		using srv::forEachField;
		using srv::ScriptError;
		using srv::ScriptLexer;
		using srv::Token;
		using srv::TokenKind;

		AbsolutePosition toPosition(const std::array<int64_t, 3>& coordinate) {
			return AbsolutePosition { .x = static_cast<int>(coordinate[0]), .y = static_cast<int>(coordinate[1]), .z = static_cast<int>(coordinate[2]) };
		}

		// Parses one `Key = value` line with `read`, reporting errors at that line.
		template <typename Read>
		auto readField(const Field& field, Read read) {
			try {
				ScriptLexer lexer(field.value);
				return read(lexer);
			} catch (const ScriptError& e) {
				throw ScriptError { .line = field.line, .message = std::format("{}: {}", field.key, e.message) };
			}
		}

	} // namespace

	std::expected<MapInfo, ParseError> readMapInfo(std::string_view text) {
		try {
			MapInfo info;
			// SectorXMin, SectorXMax, SectorYMin, SectorYMax, SectorZMin, SectorZMax
			constexpr std::array<std::string_view, 6> kBoundKeys { "SectorXMin", "SectorXMax", "SectorYMin", "SectorYMax", "SectorZMin", "SectorZMax" };
			std::array<std::optional<int>, 6> bounds;
			forEachField(text, [&](const Field& field) {
				if (const auto bound = std::ranges::find_if(kBoundKeys, [&](std::string_view key) { return equalsIgnoreCase(field.key, key); }); bound != kBoundKeys.end()) {
					bounds[static_cast<size_t>(bound - kBoundKeys.begin())] = readField(field, [](ScriptLexer& lexer) { return static_cast<int>(lexer.readNumber()); });
				} else if (equalsIgnoreCase(field.key, "Mark")) {
					// Mark = ("Thais",[32369,32215,7])
					info.marks.push_back(readField(field, [](ScriptLexer& lexer) {
						lexer.expectSpecial('(');
						MapMark mark { .name = lexer.readString() };
						lexer.expectSpecial(',');
						mark.position = toPosition(lexer.readCoordinate());
						lexer.expectSpecial(')');
						return mark;
					}));
				} else if (equalsIgnoreCase(field.key, "Depot")) {
					// Depot = (0,"Thais",1000)
					info.depots.push_back(readField(field, [](ScriptLexer& lexer) {
						lexer.expectSpecial('(');
						MapDepot depot { .id = static_cast<int>(lexer.readNumber()) };
						lexer.expectSpecial(',');
						depot.name = lexer.readString();
						lexer.expectSpecial(',');
						depot.capacity = static_cast<int>(lexer.readNumber());
						lexer.expectSpecial(')');
						return depot;
					}));
				} else if (equalsIgnoreCase(field.key, "NewbieStart")) {
					info.newbie_start = readField(field, [](ScriptLexer& lexer) { return toPosition(lexer.readCoordinate()); });
				} else if (equalsIgnoreCase(field.key, "VeteranStart")) {
					info.veteran_start = readField(field, [](ScriptLexer& lexer) { return toPosition(lexer.readCoordinate()); });
				}
				return true;
			});
			if (std::ranges::all_of(bounds, [](const std::optional<int>& bound) { return bound.has_value(); })) {
				info.sectors = SectorRange {
					.min = SectorCoordinates { .x = *bounds[0], .y = *bounds[2], .z = *bounds[4] },
					.max = SectorCoordinates { .x = *bounds[1], .y = *bounds[3], .z = *bounds[5] },
				};
			}
			return info;
		} catch (const ScriptError& e) {
			return std::unexpected(ParseError { .line = e.line, .message = e.message });
		}
	}

	std::expected<std::vector<HouseArea>, ParseError> readHouseAreas(std::string_view text) {
		try {
			std::vector<HouseArea> areas;
			forEachField(text, [&](const Field& field) {
				if (equalsIgnoreCase(field.key, "Area")) {
					// Area = (100,"Wilderness around Thais",45,0)
					areas.push_back(readField(field, [](ScriptLexer& lexer) {
						lexer.expectSpecial('(');
						HouseArea area { .id = static_cast<int>(lexer.readNumber()) };
						lexer.expectSpecial(',');
						area.name = lexer.readString();
						lexer.expectSpecial(',');
						area.square_price = static_cast<int>(lexer.readNumber());
						lexer.expectSpecial(',');
						area.depot = static_cast<int>(lexer.readNumber());
						lexer.expectSpecial(')');
						return area;
					}));
				}
				return true;
			});
			return areas;
		} catch (const ScriptError& e) {
			return std::unexpected(ParseError { .line = e.line, .message = e.message });
		}
	}

	std::string writeHouses(const std::vector<HouseDefinition>& houses, std::string_view newline) {
		std::string out;
		const auto coordinate = [&out](const AbsolutePosition& position) {
			std::format_to(std::back_inserter(out), "[{},{},{}]", position.x, position.y, position.z);
		};
		for (size_t i = 0; i < houses.size(); ++i) {
			const HouseDefinition& house = houses[i];
			if (i > 0) {
				out += newline;
			}
			std::format_to(std::back_inserter(out), "ID = {}{}", house.id, newline);
			out += "Name = ";
			srv::appendQuoted(out, house.name);
			out += newline;
			out += "Description = ";
			srv::appendQuoted(out, house.description);
			out += newline;
			std::format_to(std::back_inserter(out), "RentOffset = {}{}", house.rent_offset, newline);
			std::format_to(std::back_inserter(out), "Area = {}{}", house.area, newline);
			std::format_to(std::back_inserter(out), "GuildHouse = {}{}", house.guild_house ? "true" : "false", newline);
			out += "Exit = ";
			coordinate(house.exit);
			out += newline;
			out += "Center = ";
			coordinate(house.center);
			out += newline;
			out += "Fields = {";
			for (size_t f = 0; f < house.fields.size(); ++f) {
				if (f > 0) {
					out += ",";
				}
				coordinate(house.fields[f]);
			}
			out += "}";
			out += newline;
		}
		return out;
	}

	namespace {

		// True for a `key = ...` line (spaces before the key allowed).
		bool isKeyLine(std::string_view line, std::string_view key) {
			const size_t start = line.find_first_not_of(" \t");
			if (start == std::string_view::npos || line.size() < start + key.size() || !equalsIgnoreCase(line.substr(start, key.size()), key)) {
				return false;
			}
			const size_t after = start + key.size();
			return line.find('=') != std::string_view::npos && (line.size() == after || !srv::isAlpha(line[after]));
		}

		// `text` with its `key` lines replaced by `lines` (each ending in a
		// newline), written where the first of them was or else at the end.
		std::string replaceKeyLines(std::string_view text, std::string_view key, std::string_view lines) {
			std::string out;
			out.reserve(text.size() + lines.size());
			bool written = false;
			size_t pos = 0;
			while (pos < text.size()) {
				size_t end = text.find('\n', pos);
				end = end == std::string_view::npos ? text.size() : end + 1;
				const std::string_view line = text.substr(pos, end - pos);
				pos = end;
				if (!isKeyLine(line, key)) {
					out += line;
				} else if (!written) {
					out += lines;
					written = true;
				}
			}
			if (!written && !lines.empty()) {
				if (!out.empty() && !out.ends_with('\n')) {
					out += newlineOf(text);
				}
				out += lines;
			}
			return out;
		}

		void appendMark(std::string& out, const MapMark& mark, std::string_view newline) {
			out += "Mark = (";
			srv::appendQuoted(out, mark.name);
			std::format_to(std::back_inserter(out), ",[{},{},{}]){}", mark.position.x, mark.position.y, mark.position.z, newline);
		}

		void appendDepot(std::string& out, const MapDepot& depot, std::string_view newline) {
			std::format_to(std::back_inserter(out), "Depot = ({},", depot.id);
			srv::appendQuoted(out, depot.name);
			std::format_to(std::back_inserter(out), ",{}){}", depot.capacity, newline);
		}

	} // namespace

	std::string updateMapDat(std::string_view text, const std::vector<MapMark>& marks, const std::vector<MapDepot>& depots) {
		const std::string_view newline = newlineOf(text);
		std::string mark_lines;
		for (const MapMark& mark : marks) {
			appendMark(mark_lines, mark, newline);
		}
		std::string depot_lines;
		for (const MapDepot& depot : depots) {
			appendDepot(depot_lines, depot, newline);
		}
		return replaceKeyLines(replaceKeyLines(text, "Mark", mark_lines), "Depot", depot_lines);
	}

	std::expected<std::vector<HouseDefinition>, ParseError> readHouses(std::string_view text) {
		try {
			ScriptLexer lexer(text);
			std::vector<HouseDefinition> houses;
			while (true) {
				const Token key = lexer.next();
				if (key.kind == TokenKind::End) {
					break;
				}
				if (key.kind != TokenKind::Identifier) {
					lexer.fail("key expected");
				}
				lexer.expectSpecial('=');

				if (equalsIgnoreCase(key.identifier, "ID")) {
					houses.push_back(HouseDefinition { .id = static_cast<int>(lexer.readNumber()) });
					continue;
				}
				if (houses.empty()) {
					lexer.fail("ID expected");
				}
				HouseDefinition& house = houses.back();
				if (equalsIgnoreCase(key.identifier, "Name")) {
					house.name = lexer.readString();
				} else if (equalsIgnoreCase(key.identifier, "Description")) {
					house.description = lexer.readString();
				} else if (equalsIgnoreCase(key.identifier, "RentOffset")) {
					house.rent_offset = static_cast<int>(lexer.readNumber());
				} else if (equalsIgnoreCase(key.identifier, "Area")) {
					house.area = static_cast<int>(lexer.readNumber());
				} else if (equalsIgnoreCase(key.identifier, "GuildHouse")) {
					const Token value = lexer.next();
					if (value.kind != TokenKind::Identifier) {
						lexer.fail("true or false expected");
					}
					house.guild_house = equalsIgnoreCase(value.identifier, "true");
				} else if (equalsIgnoreCase(key.identifier, "Exit")) {
					house.exit = toPosition(lexer.readCoordinate());
				} else if (equalsIgnoreCase(key.identifier, "Center")) {
					house.center = toPosition(lexer.readCoordinate());
				} else if (equalsIgnoreCase(key.identifier, "Fields")) {
					// Fields = {[x,y,z],[x,y,z],...}
					lexer.expectSpecial('{');
					Token token = lexer.next();
					while (!token.isSpecial('}')) {
						if (!token.isSpecial('[')) {
							lexer.fail("coordinate expected");
						}
						AbsolutePosition field;
						field.x = static_cast<int>(lexer.readNumber());
						lexer.expectSpecial(',');
						field.y = static_cast<int>(lexer.readNumber());
						lexer.expectSpecial(',');
						field.z = static_cast<int>(lexer.readNumber());
						lexer.expectSpecial(']');
						house.fields.push_back(field);
						token = lexer.next();
						if (token.isSpecial(',')) {
							token = lexer.next();
						}
					}
				} else {
					lexer.fail(std::format("unknown key '{}'", key.identifier));
				}
			}
			return houses;
		} catch (const ScriptError& e) {
			return std::unexpected(ParseError { .line = e.line, .message = e.message });
		}
	}

} // namespace rme::sec
