//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "io/sec/srv_moveuse.h"

#include "io/sec/srv_script_lexer.h"

#include <utility>

namespace rme::sec {

	namespace {

		using srv::equalsIgnoreCase;
		using srv::ScriptError;
		using srv::ScriptLexer;
		using srv::Token;
		using srv::TokenKind;

		constexpr std::string_view kOperatorCharacters = "<>=!";

		bool isOperator(const Token& token) {
			return token.kind == TokenKind::Special && kOperatorCharacters.find(token.special) != std::string_view::npos;
		}

		// Reads the rules with one token of lookahead in `token_`.
		class RuleReader {
		public:
			explicit RuleReader(std::string_view text) :
				lexer_(text) {
				advance();
			}

			std::vector<MoveUseRule> read() {
				std::vector<MoveUseRule> rules;
				std::vector<std::string> sections;
				while (token_.kind != TokenKind::End) {
					if (token_.kind != TokenKind::Identifier) {
						lexer_.fail("rule, BEGIN or END expected");
					}
					if (equalsIgnoreCase(token_.identifier, "BEGIN")) {
						advance();
						if (token_.kind != TokenKind::String) {
							lexer_.fail("section name expected");
						}
						sections.push_back(std::move(token_.string));
						advance();
					} else if (equalsIgnoreCase(token_.identifier, "END")) {
						if (sections.empty()) {
							lexer_.fail("END without BEGIN");
						}
						sections.pop_back();
						advance();
					} else {
						rules.push_back(readRule(sections));
					}
				}
				if (!sections.empty()) {
					lexer_.fail("BEGIN without END");
				}
				return rules;
			}

		private:
			void advance() {
				token_ = lexer_.next();
				line_ = lexer_.line();
			}

			void expect(char c) {
				if (!token_.isSpecial(c)) {
					lexer_.fail(std::string("'") + c + "' expected");
				}
				advance();
			}

			MoveUseRule readRule(const std::vector<std::string>& sections) {
				MoveUseRule rule { .line = line_, .event = std::string(token_.identifier) };
				for (const std::string& section : sections) {
					if (!rule.section.empty()) {
						rule.section += " / ";
					}
					rule.section += section;
				}
				advance();
				while (token_.isSpecial(',')) {
					advance();
					const bool negated = token_.isSpecial('!');
					if (negated) {
						advance();
					}
					rule.conditions.push_back(readCall());
					rule.conditions.back().negated = negated;
				}
				expect('-');
				expect('>');
				rule.actions.push_back(readCall());
				while (token_.isSpecial(',')) {
					advance();
					rule.actions.push_back(readCall());
				}
				if (token_.kind == TokenKind::String) {
					rule.name = std::move(token_.string);
					advance();
				}
				return rule;
			}

			ScriptCall readCall() {
				if (token_.kind != TokenKind::Identifier) {
					lexer_.fail("condition or action expected");
				}
				ScriptCall call { .name = std::string(token_.identifier) };
				advance();
				if (!token_.isSpecial('(')) {
					return call;
				}
				advance();
				if (!token_.isSpecial(')')) {
					call.arguments.push_back(readValue());
					while (token_.isSpecial(',')) {
						advance();
						call.arguments.push_back(readValue());
					}
				}
				expect(')');
				return call;
			}

			int64_t readSigned() {
				bool negative = false;
				if (token_.isSpecial('-')) {
					negative = true;
					advance();
				}
				if (token_.kind != TokenKind::Number) {
					lexer_.fail("number expected");
				}
				const int64_t number = token_.number;
				advance();
				return negative ? -number : number;
			}

			ScriptValue readValue() {
				ScriptValue value;
				if (token_.kind == TokenKind::Number || token_.isSpecial('-')) {
					value.number = readSigned();
				} else if (token_.isSpecial('[')) {
					advance();
					value.kind = ScriptValue::Kind::Coordinate;
					value.position.x = static_cast<int>(readSigned());
					expect(',');
					value.position.y = static_cast<int>(readSigned());
					expect(',');
					value.position.z = static_cast<int>(readSigned());
					expect(']');
				} else if (token_.kind == TokenKind::String) {
					value.kind = ScriptValue::Kind::String;
					value.text = std::move(token_.string);
					advance();
				} else if (token_.kind == TokenKind::Identifier) {
					value.kind = ScriptValue::Kind::Identifier;
					value.text = std::string(token_.identifier);
					advance();
				} else if (isOperator(token_)) {
					value.kind = ScriptValue::Kind::Operator;
					while (isOperator(token_)) {
						value.text.push_back(token_.special);
						advance();
					}
				} else {
					lexer_.fail("value expected");
				}
				return value;
			}

			ScriptLexer lexer_;
			Token token_;
			int line_ = 1;
		};

	} // namespace

	std::expected<std::vector<MoveUseRule>, ParseError> readMoveUseRules(std::string_view text) {
		try {
			return RuleReader(text).read();
		} catch (const ScriptError& e) {
			return std::unexpected(ParseError { .line = e.line, .message = e.message });
		}
	}

} // namespace rme::sec
