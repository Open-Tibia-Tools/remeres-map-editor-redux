//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "io/sec/srv_creature_files.h"

#include "io/sec/srv_script_lexer.h"

#include <algorithm>
#include <format>
#include <iterator>
#include <optional>
#include <tuple>

namespace rme::sec {

	namespace {

		using srv::equalsIgnoreCase;
		using srv::Field;
		using srv::forEachField;
		using srv::ScriptError;
		using srv::ScriptLexer;
		using srv::Token;
		using srv::TokenKind;

		[[noreturn]] void fail(const Field& field, const ScriptError& error) {
			throw ScriptError { .line = field.line, .message = std::format("{}: {}", field.key, error.message) };
		}

		CreatureOutfit readOutfit(const Field& field) {
			try {
				ScriptLexer lexer(field.value);
				lexer.expectSpecial('(');
				CreatureOutfit outfit;
				outfit.look_type = static_cast<int>(lexer.readNumber());
				lexer.expectSpecial(',');
				const Token look = lexer.next();
				if (look.kind == TokenKind::Number) {
					// `(0, typeId)`: the creature looks like an object.
					outfit.look_item = outfit.look_type == 0 ? static_cast<int>(look.number) : 0;
				} else if (look.kind == TokenKind::Bytes && look.byte_count == 4) {
					outfit.head = static_cast<int>(look.bytes[0]);
					outfit.body = static_cast<int>(look.bytes[1]);
					outfit.legs = static_cast<int>(look.bytes[2]);
					outfit.feet = static_cast<int>(look.bytes[3]);
				} else {
					lexer.fail("colours or object type expected");
				}
				lexer.expectSpecial(')');
				return outfit;
			} catch (const ScriptError& e) {
				fail(field, e);
			}
		}

		std::string readStringValue(const Field& field) {
			try {
				ScriptLexer lexer(field.value);
				return lexer.readString();
			} catch (const ScriptError& e) {
				fail(field, e);
			}
		}

		int readNumberValue(const Field& field) {
			try {
				ScriptLexer lexer(field.value);
				return static_cast<int>(lexer.readNumber());
			} catch (const ScriptError& e) {
				fail(field, e);
			}
		}

		AbsolutePosition readCoordinate(const Field& field) {
			try {
				ScriptLexer lexer(field.value);
				const auto [x, y, z] = lexer.readCoordinate();
				return AbsolutePosition { .x = static_cast<int>(x), .y = static_cast<int>(y), .z = static_cast<int>(z) };
			} catch (const ScriptError& e) {
				fail(field, e);
			}
		}

		template <typename T>
		T require(const std::optional<T>& value, std::string_view key) {
			if (!value) {
				throw ScriptError { .line = 0, .message = std::format("{} missing", key) };
			}
			return *value;
		}


	} // namespace

	std::vector<NamedMonsterRace> nameMonsterRaces(std::vector<std::pair<std::string, MonsterRace>> races_by_file) {
		std::ranges::sort(races_by_file, {}, [](const auto& entry) { return std::pair(entry.second.race, entry.first); });
		std::vector<NamedMonsterRace> named;
		named.reserve(races_by_file.size());
		for (auto& [stem, race] : races_by_file) {
			const bool taken = std::ranges::any_of(named, [&](const NamedMonsterRace& other) { return equalsIgnoreCase(other.race.name, race.name); });
			std::string editor_name = taken ? std::format("{} ({})", race.name, stem) : race.name;
			named.push_back(NamedMonsterRace { .editor_name = std::move(editor_name), .race = std::move(race) });
		}
		return named;
	}

	std::string writeMonsterHomes(std::vector<MonsterHome> homes) {
		const auto sector = [](const MonsterHome& home) {
			return std::tuple(home.position.x / kSectorSize, home.position.y / kSectorSize, home.position.z);
		};
		// Grouped by sector; the homes of a sector keep their order.
		std::ranges::stable_sort(homes, {}, sector);

		std::string out =
			"# Tibia - graphical Multi-User-Dungeon\n"
			"# MonsterHomes File\n"
			"\n"
			"# Race     X     Y  Z Radius Amount Regen.\n"
			"\n";
		std::optional<std::tuple<int, int, int>> current;
		for (const MonsterHome& home : homes) {
			if (sector(home) != current) {
				current = sector(home);
				const auto [sx, sy, sz] = *current;
				std::format_to(std::back_inserter(out), "# ====== {:04},{:04},{:02} ====================\n", sx, sy, sz);
			}
			std::format_to(std::back_inserter(out), "{:6}{:6}{:6}{:3}{:7}{:7}{:7}\n", home.race, home.position.x, home.position.y, home.position.z, home.radius, home.amount, home.regen);
		}
		out += "0 # zero for end of file\n";
		return out;
	}

	std::string updateNpcHome(std::string_view text, const AbsolutePosition& home, int radius) {
		std::string out;
		out.reserve(text.size() + 16);
		size_t pos = 0;
		while (pos < text.size()) {
			size_t end = text.find('\n', pos);
			end = end == std::string_view::npos ? text.size() : end + 1;
			std::string_view line = text.substr(pos, end - pos);
			pos = end;

			// Keep the key and its spacing ("Home = "), replace the value.
			const size_t key_start = line.find_first_not_of(" \t");
			const size_t equals = line.find('=');
			if (key_start != std::string_view::npos && equals != std::string_view::npos && key_start < equals) {
				std::string_view key = line.substr(key_start, equals - key_start);
				key = key.substr(0, key.find_last_not_of(" \t") + 1);
				const size_t value_start = line.find_first_not_of(" \t", equals + 1);
				const std::string_view prefix = line.substr(0, value_start == std::string_view::npos ? line.size() : value_start);
				const std::string_view ending = line.ends_with("\r\n") ? "\r\n" : (line.ends_with('\n') ? "\n" : "");
				if (equalsIgnoreCase(key, "Home")) {
					std::format_to(std::back_inserter(out), "{}[{},{},{}]{}", prefix, home.x, home.y, home.z, ending);
					continue;
				}
				if (equalsIgnoreCase(key, "Radius")) {
					std::format_to(std::back_inserter(out), "{}{}{}", prefix, radius, ending);
					continue;
				}
				if (equalsIgnoreCase(key, "Behaviour")) {
					out += text.substr(pos - line.size());
					return out;
				}
			}
			out += line;
		}
		return out;
	}

	std::expected<MonsterRace, ParseError> readMonsterRace(std::string_view text) {
		try {
			std::optional<int> race;
			std::optional<std::string> name;
			std::optional<CreatureOutfit> outfit;
			forEachField(text, [&](const Field& field) {
				if (equalsIgnoreCase(field.key, "RaceNumber")) {
					race = readNumberValue(field);
				} else if (equalsIgnoreCase(field.key, "Name")) {
					name = readStringValue(field);
				} else if (equalsIgnoreCase(field.key, "Outfit")) {
					outfit = readOutfit(field);
				}
				return !(race && name && outfit);
			});
			return MonsterRace { .race = require(race, "RaceNumber"), .name = require(name, "Name"), .outfit = require(outfit, "Outfit") };
		} catch (const ScriptError& e) {
			return std::unexpected(ParseError { .line = e.line, .message = e.message });
		}
	}

	std::expected<NpcDefinition, ParseError> readNpc(std::string_view text) {
		try {
			std::optional<std::string> name;
			std::optional<CreatureOutfit> outfit;
			std::optional<AbsolutePosition> home;
			int radius = 0;
			forEachField(text, [&](const Field& field) {
				if (equalsIgnoreCase(field.key, "Behaviour")) {
					return false; // The behaviour rules have their own syntax
				}
				if (equalsIgnoreCase(field.key, "Name")) {
					name = readStringValue(field);
				} else if (equalsIgnoreCase(field.key, "Outfit")) {
					outfit = readOutfit(field);
				} else if (equalsIgnoreCase(field.key, "Home")) {
					home = readCoordinate(field);
				} else if (equalsIgnoreCase(field.key, "Radius")) {
					radius = readNumberValue(field);
				}
				return true;
			});
			return NpcDefinition { .name = require(name, "Name"), .outfit = require(outfit, "Outfit"), .home = require(home, "Home"), .radius = radius };
		} catch (const ScriptError& e) {
			return std::unexpected(ParseError { .line = e.line, .message = e.message });
		}
	}

	std::expected<Raid, ParseError> readRaid(std::string_view text) {
		try {
			Raid raid;
			// Each wave starts with its Delay; Position and Race are required.
			std::optional<int> wave_line;
			const auto checkWave = [&]() {
				if (!wave_line) {
					return;
				}
				const RaidWave& wave = raid.waves.back();
				if (wave.position.x == 0 || wave.race == 0) {
					throw ScriptError { .line = *wave_line, .message = "wave without Position or Race" };
				}
			};
			forEachField(text, [&](const Field& field) {
				const auto wave = [&]() -> RaidWave& {
					if (raid.waves.empty()) {
						throw ScriptError { .line = field.line, .message = std::format("{}: Delay expected first", field.key) };
					}
					return raid.waves.back();
				};
				if (equalsIgnoreCase(field.key, "Type")) {
					try {
						ScriptLexer lexer(field.value);
						const Token type = lexer.next();
						if (type.kind != TokenKind::Identifier) {
							lexer.fail("raid type expected");
						}
						raid.type = type.identifier;
					} catch (const ScriptError& e) {
						fail(field, e);
					}
				} else if (equalsIgnoreCase(field.key, "Interval")) {
					raid.interval = readNumberValue(field);
				} else if (equalsIgnoreCase(field.key, "Delay")) {
					checkWave();
					raid.waves.push_back(RaidWave { .delay = readNumberValue(field) });
					wave_line = field.line;
				} else if (equalsIgnoreCase(field.key, "Position")) {
					wave().position = readCoordinate(field);
				} else if (equalsIgnoreCase(field.key, "Spread")) {
					wave().spread = readNumberValue(field);
				} else if (equalsIgnoreCase(field.key, "Race")) {
					wave().race = readNumberValue(field);
				} else if (equalsIgnoreCase(field.key, "Count")) {
					// Count = (min,max)
					RaidWave& current = wave();
					try {
						ScriptLexer lexer(field.value);
						lexer.expectSpecial('(');
						current.min_count = static_cast<int>(lexer.readNumber());
						lexer.expectSpecial(',');
						current.max_count = static_cast<int>(lexer.readNumber());
						lexer.expectSpecial(')');
					} catch (const ScriptError& e) {
						fail(field, e);
					}
				} else if (equalsIgnoreCase(field.key, "Message")) {
					wave().message = readStringValue(field);
				}
				return true;
			});
			checkWave();
			return raid;
		} catch (const ScriptError& e) {
			return std::unexpected(ParseError { .line = e.line, .message = e.message });
		}
	}

	std::expected<std::vector<MonsterHome>, ParseError> readMonsterHomes(std::string_view text) {
		try {
			ScriptLexer lexer(text);
			std::vector<MonsterHome> homes;
			while (true) {
				const Token token = lexer.next();
				if (token.kind == TokenKind::End) {
					break;
				}
				if (token.kind != TokenKind::Number) {
					lexer.fail("race number expected");
				}
				if (token.number == 0) {
					break; // The file ends with a 0 race
				}
				MonsterHome home { .race = static_cast<int>(token.number) };
				home.position.x = static_cast<int>(lexer.readNumber());
				home.position.y = static_cast<int>(lexer.readNumber());
				home.position.z = static_cast<int>(lexer.readNumber());
				home.radius = static_cast<int>(lexer.readNumber());
				home.amount = static_cast<int>(lexer.readNumber());
				home.regen = static_cast<int>(lexer.readNumber());
				homes.push_back(home);
			}
			return homes;
		} catch (const ScriptError& e) {
			return std::unexpected(ParseError { .line = e.line, .message = e.message });
		}
	}

} // namespace rme::sec
