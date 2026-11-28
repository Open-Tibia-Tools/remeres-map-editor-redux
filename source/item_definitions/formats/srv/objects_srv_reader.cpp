//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "item_definitions/formats/srv/objects_srv_reader.h"

#include "io/sec/srv_script_lexer.h"

#include <format>
#include <limits>

namespace rme::srv {

	namespace {

		class ObjectTypeReader {
		public:
			explicit ObjectTypeReader(std::string_view text) :
				lexer_(text) { }

			std::vector<ObjectType> read() {
				std::vector<ObjectType> types;
				while (true) {
					const Token key = lexer_.next();
					if (key.kind == TokenKind::End) {
						break;
					}
					if (key.kind != TokenKind::Identifier) {
						lexer_.fail("key expected");
					}
					lexer_.expectSpecial('=');

					if (equalsIgnoreCase(key.identifier, "TypeID")) {
						const int64_t type_id = lexer_.readNumber();
						if (type_id < 0 || type_id > std::numeric_limits<uint16_t>::max()) {
							lexer_.fail(std::format("type id {} is out of range", type_id));
						}
						types.push_back(ObjectType { .type_id = static_cast<uint16_t>(type_id) });
						continue;
					}

					if (types.empty()) {
						lexer_.fail("TypeID expected");
					}
					ObjectType& type = types.back();
					if (equalsIgnoreCase(key.identifier, "Name")) {
						type.name = lexer_.readString();
					} else if (equalsIgnoreCase(key.identifier, "Description")) {
						type.description = lexer_.readString();
					} else if (equalsIgnoreCase(key.identifier, "Flags")) {
						readFlags(type);
					} else if (equalsIgnoreCase(key.identifier, "Attributes")) {
						readAttributes(type);
					} else {
						lexer_.fail(std::format("unknown key '{}'", key.identifier));
					}
				}
				return types;
			}

		private:
			// Reads `{a, b, ...}`, calling `read_entry` with each entry's first token.
			template <typename ReadEntry>
			void readList(ReadEntry read_entry) {
				lexer_.expectSpecial('{');
				Token token = lexer_.next();
				if (token.isSpecial('}')) {
					return;
				}
				while (true) {
					if (token.kind != TokenKind::Identifier) {
						lexer_.fail("name expected");
					}
					read_entry(token.identifier);
					token = lexer_.next();
					if (token.isSpecial('}')) {
						return;
					}
					if (!token.isSpecial(',')) {
						lexer_.fail("',' or '}' expected");
					}
					token = lexer_.next();
				}
			}

			void readFlags(ObjectType& type) {
				readList([&](std::string_view flag) {
					type.flags.emplace_back(flag);
				});
			}

			void readAttributes(ObjectType& type) {
				readList([&](std::string_view name) {
					lexer_.expectSpecial('=');
					type.attributes.emplace_back(std::string(name), lexer_.readNumber());
				});
			}

			ScriptLexer lexer_;
		};

	} // namespace

	bool ObjectType::hasFlag(std::string_view flag) const {
		for (const std::string& candidate : flags) {
			if (equalsIgnoreCase(candidate, flag)) {
				return true;
			}
		}
		return false;
	}

	std::optional<int64_t> ObjectType::attribute(std::string_view name) const {
		for (const auto& [candidate, value] : attributes) {
			if (equalsIgnoreCase(candidate, name)) {
				return value;
			}
		}
		return std::nullopt;
	}

	std::expected<std::vector<ObjectType>, ReadError> readObjectTypes(std::string_view text) {
		try {
			return ObjectTypeReader(text).read();
		} catch (const ScriptError& e) {
			return std::unexpected(ReadError { .line = e.line, .message = e.message });
		}
	}

} // namespace rme::srv
