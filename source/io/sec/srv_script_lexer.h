//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_IO_SEC_SRV_SCRIPT_LEXER_H_
#define RME_IO_SEC_SRV_SCRIPT_LEXER_H_

// Tokenizer for the text files of the original game server (sector
// files, objects.srv, creature files, moveuse.dat). It follows the server's
// TReadScriptFile for the tokens those files use: numbers, byte sequences
// such as `12-34` (up to four), identifiers, quoted strings with `\` escapes,
// single-character specials and `#` comments.

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace rme::srv {

	struct ScriptError {
		int line = 0;
		std::string message;
	};

	constexpr bool isDigit(char c) {
		return c >= '0' && c <= '9';
	}

	constexpr bool isAlpha(char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
	}

	constexpr char toLower(char c) {
		return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
	}

	inline bool equalsIgnoreCase(std::string_view a, std::string_view b) {
		if (a.size() != b.size()) {
			return false;
		}
		for (size_t i = 0; i < a.size(); ++i) {
			if (toLower(a[i]) != toLower(b[i])) {
				return false;
			}
		}
		return true;
	}

	enum class TokenKind : uint8_t {
		End,
		Number,
		Bytes,
		Identifier,
		String,
		Special,
	};

	struct Token {
		TokenKind kind = TokenKind::End;
		int64_t number = 0;
		std::array<int64_t, 4> bytes {}; // tile offsets use 2, outfit colours 4
		size_t byte_count = 0;
		std::string_view identifier;
		std::string string;
		char special = 0;

		[[nodiscard]] bool isSpecial(char c) const {
			return kind == TokenKind::Special && special == c;
		}
	};

	// Errors are thrown as ScriptError; parsers catch them at their entry point.
	class ScriptLexer {
	public:
		explicit ScriptLexer(std::string_view text) :
			text_(text) { }

		[[noreturn]] void fail(std::string message) const {
			throw ScriptError { .line = line_, .message = std::move(message) };
		}

		// The line the last token ended on.
		[[nodiscard]] int line() const {
			return line_;
		}

		Token next() {
			Token token;
			skipWhitespaceAndComments();
			if (pos_ >= text_.size()) {
				return token;
			}

			const char c = text_[pos_++];
			if (c == '"') {
				readString(token);
			} else if (isAlpha(c)) {
				const size_t start = pos_ - 1;
				while (pos_ < text_.size() && (isAlpha(text_[pos_]) || isDigit(text_[pos_]) || text_[pos_] == '_')) {
					++pos_;
				}
				token.kind = TokenKind::Identifier;
				token.identifier = text_.substr(start, pos_ - start);
			} else if (isDigit(c)) {
				readNumberOrBytes(c, token);
			} else {
				token.kind = TokenKind::Special;
				token.special = c;
			}
			return token;
		}

		void expectSpecial(char c) {
			const Token token = next();
			if (!token.isSpecial(c)) {
				fail(std::string("'") + c + "' expected");
			}
		}

		// A number with an optional leading '-'.
		int64_t readNumber() {
			Token token = next();
			bool negative = false;
			if (token.isSpecial('-')) {
				negative = true;
				token = next();
			}
			if (token.kind != TokenKind::Number) {
				fail("number expected");
			}
			return negative ? -token.number : token.number;
		}

		// `[x,y,z]`
		std::array<int64_t, 3> readCoordinate() {
			expectSpecial('[');
			std::array<int64_t, 3> coordinate {};
			coordinate[0] = readNumber();
			expectSpecial(',');
			coordinate[1] = readNumber();
			expectSpecial(',');
			coordinate[2] = readNumber();
			expectSpecial(']');
			return coordinate;
		}

		std::string readString() {
			Token token = next();
			if (token.kind != TokenKind::String) {
				fail("string expected");
			}
			return std::move(token.string);
		}

	private:
		void skipWhitespaceAndComments() {
			while (pos_ < text_.size()) {
				const char c = text_[pos_];
				if (c == '\n') {
					++line_;
					++pos_;
				} else if (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v') {
					++pos_;
				} else if (c == '#') {
					while (pos_ < text_.size() && text_[pos_] != '\n') {
						++pos_;
					}
				} else {
					break;
				}
			}
		}

		void readString(Token& token) {
			token.kind = TokenKind::String;
			while (true) {
				if (pos_ >= text_.size()) {
					fail("unexpected end of file in string");
				}
				char c = text_[pos_++];
				if (c == '"') {
					return;
				}
				if (c == '\\') {
					if (pos_ >= text_.size()) {
						fail("unexpected end of file in string");
					}
					c = text_[pos_++];
					if (c == 'n') {
						c = '\n';
					}
				}
				if (c == '\n') {
					++line_;
				}
				token.string.push_back(c);
			}
		}

		int64_t readDigits(char first) {
			constexpr int64_t kLimit = (std::numeric_limits<int64_t>::max() - 9) / 10;
			int64_t value = first - '0';
			while (pos_ < text_.size() && isDigit(text_[pos_])) {
				if (value > kLimit) {
					fail("number too large");
				}
				value = value * 10 + (text_[pos_++] - '0');
			}
			return value;
		}

		// `12` is a number, `12-34` a byte sequence (a tile offset).
		void readNumberOrBytes(char first, Token& token) {
			int64_t value = readDigits(first);
			while (pos_ < text_.size() && text_[pos_] == '-') {
				++pos_;
				if (pos_ >= text_.size() || !isDigit(text_[pos_])) {
					fail("syntax error in byte sequence");
				}
				if (token.byte_count >= token.bytes.size()) {
					fail("too many bytes in sequence");
				}
				token.bytes[token.byte_count++] = value;
				value = readDigits(text_[pos_++]);
			}

			if (token.byte_count == 0) {
				token.kind = TokenKind::Number;
				token.number = value;
			} else {
				if (token.byte_count >= token.bytes.size()) {
					fail("too many bytes in sequence");
				}
				token.bytes[token.byte_count++] = value;
				token.kind = TokenKind::Bytes;
			}
		}

		std::string_view text_;
		size_t pos_ = 0;
		int line_ = 1;
	};

	// Appends `text` as a quoted string the lexer reads back unchanged:
	// newlines as \n, quotes and backslashes escaped, other bytes as they are.
	inline void appendQuoted(std::string& out, std::string_view text) {
		out.push_back('"');
		for (const char c : text) {
			if (c == '\n') {
				out += "\\n";
			} else if (c == '"' || c == '\\') {
				out.push_back('\\');
				out.push_back(c);
			} else {
				out.push_back(c);
			}
		}
		out.push_back('"');
	}

	// A `Key = value` line of a server data file (creature files, map.dat).
	struct Field {
		std::string_view key;
		std::string_view value;
		int line = 0;
	};

	// Calls `on_field` for each `Key = value` line until it returns false.
	// Lines that are not of that shape (comments, behaviour rules) are skipped.
	template <typename OnField>
	inline void forEachField(std::string_view text, OnField on_field) {
		int line_number = 0;
		size_t pos = 0;
		while (pos < text.size()) {
			++line_number;
			size_t end = text.find('\n', pos);
			if (end == std::string_view::npos) {
				end = text.size();
			}
			std::string_view line = text.substr(pos, end - pos);
			pos = end + 1;

			size_t i = line.find_first_not_of(" \t\r");
			if (i == std::string_view::npos || !isAlpha(line[i])) {
				continue;
			}
			const size_t key_start = i;
			while (i < line.size() && (isAlpha(line[i]) || isDigit(line[i]) || line[i] == '_')) {
				++i;
			}
			const std::string_view key = line.substr(key_start, i - key_start);
			i = line.find_first_not_of(" \t", i);
			if (i == std::string_view::npos || line[i] != '=') {
				continue;
			}
			if (!on_field(Field { .key = key, .value = line.substr(i + 1), .line = line_number })) {
				return;
			}
		}
	}

} // namespace rme::srv

#endif
