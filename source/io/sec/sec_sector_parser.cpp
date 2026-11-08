//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "io/sec/sec_sector_parser.h"

#include "io/sec/srv_script_lexer.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <format>
#include <iterator>
#include <limits>

namespace rme::sec {

	namespace {

		using srv::equalsIgnoreCase;
		using srv::ScriptError;
		using srv::ScriptLexer;
		using srv::Token;
		using srv::TokenKind;

		constexpr std::array<std::string_view, 18> kAttributeNames = {
			"Content",
			"ChestQuestNumber",
			"Amount",
			"KeyNumber",
			"KeyholeNumber",
			"Level",
			"DoorQuestNumber",
			"DoorQuestValue",
			"Charges",
			"String",
			"Editor",
			"ContainerLiquidType",
			"PoolLiquidType",
			"AbsTeleportDestination",
			"Responsible",
			"RemainingExpireTime",
			"SavedExpireTime",
			"RemainingUses",
		};

		// Containers nest a few levels at most; this only guards the recursion.
		constexpr int kMaxContentDepth = 64;

		std::optional<InstanceAttribute> findAttribute(std::string_view name) {
			for (size_t i = 0; i < kAttributeNames.size(); ++i) {
				if (equalsIgnoreCase(kAttributeNames[i], name)) {
					return static_cast<InstanceAttribute>(i);
				}
			}
			return std::nullopt;
		}

		// Follows the server's LoadSector / LoadObjects.
		class SectorReader {
		public:
			explicit SectorReader(std::string_view text) :
				lexer_(text) {
				tile_index_.fill(-1);
			}

			Sector read() {
				SectorTile* current = nullptr;
				while (true) {
					Token token = lexer_.next();
					if (token.kind == TokenKind::End) {
						break;
					}
					if (token.isSpecial(',')) {
						continue;
					}
					if (token.kind == TokenKind::Bytes) {
						current = &selectTile(token);
						continue;
					}
					if (token.kind != TokenKind::Identifier) {
						lexer_.fail("next map point expected");
					}
					if (!current) {
						lexer_.fail("coordinate expected");
					}

					if (equalsIgnoreCase(token.identifier, "Refresh")) {
						current->flags |= static_cast<uint8_t>(TileFlag::Refresh);
					} else if (equalsIgnoreCase(token.identifier, "NoLogout")) {
						current->flags |= static_cast<uint8_t>(TileFlag::NoLogout);
					} else if (equalsIgnoreCase(token.identifier, "ProtectionZone")) {
						current->flags |= static_cast<uint8_t>(TileFlag::ProtectionZone);
					} else if (equalsIgnoreCase(token.identifier, "Content")) {
						lexer_.expectSpecial('=');
						readObjectList(current->objects, 0);
					} else {
						lexer_.fail(std::format("unknown map flag '{}'", token.identifier));
					}
				}
				return std::move(sector_);
			}

		private:
			SectorTile& selectTile(const Token& token) {
				if (token.byte_count != 2) {
					lexer_.fail("tile offset must be two numbers");
				}
				const int64_t x = token.bytes[0];
				const int64_t y = token.bytes[1];
				if (x < 0 || x >= kSectorSize || y < 0 || y >= kSectorSize) {
					lexer_.fail(std::format("tile offset {}-{} is outside the sector", x, y));
				}
				lexer_.expectSpecial(':');

				// A tile may be listed more than once; later entries add to it.
				int& index = tile_index_[static_cast<size_t>(x * kSectorSize + y)];
				if (index < 0) {
					index = static_cast<int>(sector_.tiles.size());
					sector_.tiles.push_back(SectorTile { .offset_x = static_cast<uint8_t>(x), .offset_y = static_cast<uint8_t>(y) });
				}
				return sector_.tiles[static_cast<size_t>(index)];
			}

			void readObjectList(std::vector<SectorObject>& out, int depth) {
				if (depth >= kMaxContentDepth) {
					lexer_.fail("content nested too deeply");
				}
				lexer_.expectSpecial('{');

				Token token = lexer_.next();
				if (token.isSpecial('}')) {
					return;
				}

				while (true) {
					if (token.kind != TokenKind::Number) {
						lexer_.fail("object type expected");
					}
					if (token.number > std::numeric_limits<uint16_t>::max()) {
						lexer_.fail(std::format("object type {} is out of range", token.number));
					}

					SectorObject object { .type_id = static_cast<uint16_t>(token.number) };
					token = lexer_.next();
					while (token.kind == TokenKind::Identifier) {
						const auto attribute = findAttribute(token.identifier);
						if (!attribute) {
							lexer_.fail(std::format("unknown attribute '{}'", token.identifier));
						}
						lexer_.expectSpecial('=');

						if (*attribute == InstanceAttribute::Content) {
							readObjectList(object.content, depth + 1);
						} else if (*attribute == InstanceAttribute::String || *attribute == InstanceAttribute::Editor) {
							object.attributes.push_back(ObjectAttribute { .key = *attribute, .text = lexer_.readString() });
						} else {
							object.attributes.push_back(ObjectAttribute { .key = *attribute, .number = lexer_.readNumber() });
						}
						token = lexer_.next();
					}
					out.push_back(std::move(object));

					if (token.isSpecial('}')) {
						return;
					}
					if (!token.isSpecial(',')) {
						lexer_.fail("',' or '}' expected");
					}
					token = lexer_.next();
				}
			}

			ScriptLexer lexer_;
			Sector sector_;
			std::array<int, kSectorSize * kSectorSize> tile_index_ {};
		};

		std::optional<int> parseInt(std::string_view text) {
			int value = 0;
			const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
			if (ec != std::errc {} || end != text.data() + text.size()) {
				return std::nullopt;
			}
			return value;
		}

	} // namespace

	std::string_view attributeName(InstanceAttribute attribute) {
		const auto index = static_cast<size_t>(attribute);
		return index < kAttributeNames.size() ? kAttributeNames[index] : std::string_view {};
	}

	std::optional<SectorCoordinates> parseSectorFileName(std::string_view file_name) {
		if (file_name.size() <= kSectorFileExtension.size() || !equalsIgnoreCase(file_name.substr(file_name.size() - kSectorFileExtension.size()), kSectorFileExtension)) {
			return std::nullopt;
		}
		const std::string_view stem = file_name.substr(0, file_name.size() - kSectorFileExtension.size());

		const size_t first = stem.find('-');
		const size_t second = first == std::string_view::npos ? std::string_view::npos : stem.find('-', first + 1);
		if (second == std::string_view::npos) {
			return std::nullopt;
		}

		const auto x = parseInt(stem.substr(0, first));
		const auto y = parseInt(stem.substr(first + 1, second - first - 1));
		const auto z = parseInt(stem.substr(second + 1));
		if (!x || !y || !z) {
			return std::nullopt;
		}
		return SectorCoordinates { .x = *x, .y = *y, .z = *z };
	}

	std::expected<Sector, ParseError> parseSector(std::string_view text) {
		try {
			return SectorReader(text).read();
		} catch (const ScriptError& e) {
			return std::unexpected(ParseError { .line = e.line, .message = e.message });
		}
	}

	std::string sectorFileName(const SectorCoordinates& coordinates) {
		return std::format("{:04}-{:04}-{:02}.sec", coordinates.x, coordinates.y, coordinates.z);
	}

	namespace {

		void appendObjects(std::string& out, const std::vector<SectorObject>& objects) {
			out += "{";
			for (size_t i = 0; i < objects.size(); ++i) {
				const SectorObject& object = objects[i];
				if (i > 0) {
					out += ", ";
				}
				std::format_to(std::back_inserter(out), "{}", object.type_id);

				// The server writes attributes in its own order, never zero values.
				std::array<const ObjectAttribute*, kAttributeNames.size()> by_key {};
				for (const ObjectAttribute& attribute : object.attributes) {
					by_key[static_cast<size_t>(attribute.key)] = &attribute;
				}
				for (const ObjectAttribute* attribute : by_key) {
					if (!attribute || attribute->key == InstanceAttribute::Content) {
						continue;
					}
					const bool is_string = attribute->key == InstanceAttribute::String || attribute->key == InstanceAttribute::Editor;
					if (is_string ? attribute->text.empty() : attribute->number == 0) {
						continue;
					}
					std::format_to(std::back_inserter(out), " {}=", kAttributeNames[static_cast<size_t>(attribute->key)]);
					if (is_string) {
						srv::appendQuoted(out, attribute->text);
					} else {
						std::format_to(std::back_inserter(out), "{}", attribute->number);
					}
				}
				if (!object.content.empty()) {
					out += " Content=";
					appendObjects(out, object.content);
				}
			}
			out += "}";
		}

	} // namespace

	std::string writeSector(const SectorCoordinates& coordinates, const Sector& sector) {
		std::string out = std::format("# Tibia - graphical Multi-User-Dungeon\n# Data for sector {}/{}/{}\n\n", coordinates.x, coordinates.y, coordinates.z);

		std::vector<const SectorTile*> tiles;
		tiles.reserve(sector.tiles.size());
		for (const SectorTile& tile : sector.tiles) {
			if (tile.flags != 0 || !tile.objects.empty()) {
				tiles.push_back(&tile);
			}
		}
		std::ranges::sort(tiles, {}, [](const SectorTile* tile) { return std::pair(tile->offset_x, tile->offset_y); });

		for (const SectorTile* tile : tiles) {
			std::format_to(std::back_inserter(out), "{}-{}: ", tile->offset_x, tile->offset_y);
			bool first = true;
			const auto separate = [&] {
				if (!first) {
					out += ", ";
				}
				first = false;
			};
			if (tile->hasFlag(TileFlag::Refresh)) {
				separate();
				out += "Refresh";
			}
			if (tile->hasFlag(TileFlag::NoLogout)) {
				separate();
				out += "NoLogout";
			}
			if (tile->hasFlag(TileFlag::ProtectionZone)) {
				separate();
				out += "ProtectionZone";
			}
			if (!tile->objects.empty()) {
				separate();
				out += "Content=";
				appendObjects(out, tile->objects);
			}
			out += "\n";
		}
		return out;
	}

	int32_t packAbsoluteCoordinate(const AbsolutePosition& position) {
		return static_cast<int32_t>((static_cast<uint32_t>(position.x - 24576) & 0x3FFF) << 18 | (static_cast<uint32_t>(position.y - 24576) & 0x3FFF) << 4 | (static_cast<uint32_t>(position.z) & 0xF));
	}

	AbsolutePosition unpackAbsoluteCoordinate(int64_t packed) {
		const auto bits = static_cast<uint32_t>(packed);
		return AbsolutePosition {
			.x = static_cast<int>((bits >> 18) & 0x3FFF) + 24576,
			.y = static_cast<int>((bits >> 4) & 0x3FFF) + 24576,
			.z = static_cast<int>(bits & 0xF),
		};
	}

} // namespace rme::sec
