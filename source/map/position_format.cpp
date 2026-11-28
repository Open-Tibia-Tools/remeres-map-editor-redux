//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "map/position_format.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <format>
#include <system_error>
#include <vector>

namespace {

	bool IsSpace(char ch) {
		return std::isspace(static_cast<unsigned char>(ch)) != 0;
	}

	std::string_view Trim(std::string_view text) {
		while (!text.empty() && IsSpace(text.front())) {
			text.remove_prefix(1);
		}
		while (!text.empty() && IsSpace(text.back())) {
			text.remove_suffix(1);
		}
		return text;
	}

	std::string Lower(std::string_view text) {
		std::string lower(text);
		std::ranges::transform(lower, lower.begin(), [](char ch) { return static_cast<char>(std::tolower(static_cast<unsigned char>(ch))); });
		return lower;
	}

	bool StripEnclosing(std::string_view& text, char open, char close) {
		if (text.size() >= 2 && text.front() == open && text.back() == close) {
			text = Trim(text.substr(1, text.size() - 2));
			return true;
		}
		return false;
	}

	std::optional<int> ParseCoordinate(std::string_view text) {
		text = Trim(text);
		if (text.empty()) {
			return std::nullopt;
		}
		int value = 0;
		const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
		if (error != std::errc() || end != text.data() + text.size()) {
			return std::nullopt;
		}
		return value;
	}

	std::vector<std::string_view> Split(std::string_view text, char separator) {
		std::vector<std::string_view> parts;
		size_t start = 0;
		while (true) {
			const size_t end = text.find(separator, start);
			parts.push_back(text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
			if (end == std::string_view::npos) {
				return parts;
			}
			start = end + 1;
		}
	}

	std::vector<std::string_view> SplitOnSpaces(std::string_view text) {
		std::vector<std::string_view> parts;
		size_t i = 0;
		while (i < text.size()) {
			while (i < text.size() && IsSpace(text[i])) {
				++i;
			}
			const size_t start = i;
			while (i < text.size() && !IsSpace(text[i])) {
				++i;
			}
			if (i > start) {
				parts.push_back(text.substr(start, i - start));
			}
		}
		return parts;
	}

	// "x, y, z" or "x y z": exactly three integers.
	std::optional<Position> ParseTriple(std::string_view text) {
		const std::vector<std::string_view> parts = text.find(',') != std::string_view::npos ? Split(text, ',') : SplitOnSpaces(text);
		if (parts.size() != 3) {
			return std::nullopt;
		}
		const auto x = ParseCoordinate(parts[0]);
		const auto y = ParseCoordinate(parts[1]);
		const auto z = ParseCoordinate(parts[2]);
		if (!x || !y || !z) {
			return std::nullopt;
		}
		return Position(*x, *y, *z);
	}

	struct KeyedValues {
		std::optional<int> x, y, z, fromx, tox, fromy, toy, fromz, toz;

		std::optional<int>* slot(std::string_view key) {
			const std::string lower = Lower(key);
			if (lower == "x") {
				return &x;
			}
			if (lower == "y") {
				return &y;
			}
			if (lower == "z") {
				return &z;
			}
			if (lower == "fromx") {
				return &fromx;
			}
			if (lower == "tox") {
				return &tox;
			}
			if (lower == "fromy") {
				return &fromy;
			}
			if (lower == "toy") {
				return &toy;
			}
			if (lower == "fromz") {
				return &fromz;
			}
			if (lower == "toz") {
				return &toz;
			}
			return nullptr;
		}
	};

    struct ParsedPosition {
        Position position;
        std::optional<Position> range_end;
    };

    std::optional<ParsedPosition> AsSinglePosition(std::optional<Position> position)
    {
        if (!position) {
            return std::nullopt;
        }
        return ParsedPosition {*position, std::nullopt};
    }

    // The inside of a Lua table or JSON object: "key = value" or "key: value" pairs, where
    // the key may be quoted. A trailing separator is allowed, as Lua allows it.
    std::optional<ParsedPosition> ParseKeyed(std::string_view body)
    {
        std::vector<std::string_view> parts = Split(body, ',');
        if (parts.size() > 1 && Trim(parts.back()).empty()) {
            parts.pop_back();
        }

        KeyedValues values;
        for (std::string_view part : parts) {
            part = Trim(part);
			const size_t separator = part.find_first_of("=:");
			if (separator == std::string_view::npos) {
				return std::nullopt;
			}
			std::string_view key = Trim(part.substr(0, separator));
			if (!StripEnclosing(key, '"', '"')) {
				StripEnclosing(key, '\'', '\'');
			}
			std::optional<int>* slot = values.slot(key);
			const auto value = ParseCoordinate(part.substr(separator + 1));
			if (!slot || slot->has_value() || !value) {
				return std::nullopt;
			}
			*slot = value;
        }

        const bool any_range = values.fromx || values.tox || values.fromy || values.toy || values.fromz || values.toz;
		if (values.x && values.y && values.z && !any_range) {
            return ParsedPosition {Position(*values.x, *values.y, *values.z), std::nullopt};
        }

        if (values.x || values.y || !values.fromx || !values.tox || !values.fromy || !values.toy) {
            return std::nullopt;
        }
        const bool single_floor = values.z && !values.fromz && !values.toz;
        const bool floor_range = !values.z && values.fromz && values.toz;
        if (!single_floor && !floor_range) {
			return std::nullopt;
		}
        const int from_z = single_floor ? *values.z : std::min(*values.fromz, *values.toz);
        const int to_z = single_floor ? *values.z : std::max(*values.fromz, *values.toz);
        return ParsedPosition {
            Position(std::min(*values.fromx, *values.tox), std::min(*values.fromy, *values.toy), from_z),
            Position(std::max(*values.fromx, *values.tox), std::max(*values.fromy, *values.toy), to_z),
        };
    }

    std::optional<ParsedPosition> ParseUnbounded(std::string_view text)
    {
        text = Trim(text);
        if (text.empty()) {
            return std::nullopt;
        }

        std::string_view inner = text;
        if (StripEnclosing(inner, '{', '}')) {
            if (inner.find_first_of("=:") != std::string_view::npos) {
				return ParseKeyed(inner);
			}
            return AsSinglePosition(ParseTriple(inner));
        }

        constexpr std::string_view kConstructor = "position";
        if (text.size() > kConstructor.size() && Lower(text.substr(0, kConstructor.size())) == kConstructor) {
            inner = Trim(text.substr(kConstructor.size()));
            if (!StripEnclosing(inner, '(', ')')) {
				return std::nullopt;
			}
            return AsSinglePosition(ParseTriple(inner));
        }

        if (StripEnclosing(inner, '(', ')') || StripEnclosing(inner, '[', ']')) {
            return AsSinglePosition(ParseTriple(inner));
        }
        return AsSinglePosition(ParseTriple(text));
    }

} // namespace

PositionFormat PositionFormatFromSetting(int value) {
	const int clamped = std::clamp(value, 0, static_cast<int>(kPositionFormats.size()) - 1);
	return kPositionFormats[static_cast<size_t>(clamped)].format;
}

std::string FormatPosition(const Position& position, PositionFormat format) {
	switch (format) {
		case PositionFormat::LuaTable:
			return std::format("{{x = {}, y = {}, z = {}}}", position.x, position.y, position.z);
		case PositionFormat::CompactJson:
			return std::format(R"({{"x":{},"y":{},"z":{}}})", position.x, position.y, position.z);
		case PositionFormat::Csv:
			return std::format("{}, {}, {}", position.x, position.y, position.z);
		case PositionFormat::Tuple:
			return std::format("({}, {}, {})", position.x, position.y, position.z);
		case PositionFormat::ConstructorCall:
		default:
			return std::format("Position({}, {}, {})", position.x, position.y, position.z);
	}
}

std::string FormatPositionRange(const Position& from, const Position& to) {
	if (from.z != to.z) {
		return std::format("{{fromx = {}, tox = {}, fromy = {}, toy = {}, fromz = {}, toz = {}}}", from.x, to.x, from.y, to.y, from.z, to.z);
	}
	return std::format("{{fromx = {}, tox = {}, fromy = {}, toy = {}, z = {}}}", from.x, to.x, from.y, to.y, from.z);
}

std::optional<Position> ParsePosition(std::string_view text, const PositionBounds& bounds) {
    const std::optional<ParsedPosition> parsed = ParseUnbounded(text);
    if (!parsed || !bounds.contains(parsed->position) || (parsed->range_end && !bounds.contains(*parsed->range_end))) {
        return std::nullopt;
    }
    return parsed->position;
}
