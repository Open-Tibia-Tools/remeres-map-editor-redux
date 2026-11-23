//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/otfi_descriptor.h"

#include <algorithm>
#include <cctype>
#include <format>

namespace {

	constexpr std::string_view kWhitespace = " \t\r";

	std::string_view Trim(std::string_view s) {
		const size_t begin = s.find_first_not_of(kWhitespace);
		if (begin == std::string_view::npos) {
			return {};
		}
		const size_t end = s.find_last_not_of(kWhitespace);
		return s.substr(begin, end - begin + 1);
	}

	std::string_view Unquote(std::string_view s) {
		if (s.size() >= 2 && (s.front() == '"' || s.front() == '\'') && s.back() == s.front()) {
			return s.substr(1, s.size() - 2);
		}
		return s;
	}

	std::string Lower(std::string_view s) {
		std::string out(s);
		std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return out;
	}

	std::optional<bool> ParseBool(std::string_view value) {
		const std::string v = Lower(value);
		if (v == "true" || v == "1" || v == "yes") {
			return true;
		}
		if (v == "false" || v == "0" || v == "no") {
			return false;
		}
		return std::nullopt;
	}

	struct KeyValue {
		std::string_view key;
		std::string_view value;
	};

	KeyValue SplitKeyValue(std::string_view line) {
		const size_t colon = line.find(':');
		if (colon == std::string_view::npos) {
			return { Trim(line), {} };
		}
		return { Trim(line.substr(0, colon)), Unquote(Trim(line.substr(colon + 1))) };
	}

} // namespace

std::optional<OtfiDescriptor> ParseOtfi(std::string_view text, std::string& error) {
	if (text.starts_with("\xEF\xBB\xBF")) {
		text.remove_prefix(3);
	}

	OtfiDescriptor descriptor;
	bool found_node = false;
	bool in_node = false;

	while (!text.empty()) {
		const size_t eol = text.find('\n');
		const std::string_view raw = text.substr(0, eol);
		text = eol == std::string_view::npos ? std::string_view {} : text.substr(eol + 1);

		const std::string_view line = Trim(raw);
		if (line.empty() || line.starts_with("//")) {
			continue;
		}

		const bool indented = raw.front() == ' ' || raw.front() == '\t';
		const auto [key, value] = SplitKeyValue(line);

		if (!indented) {
			if (found_node) {
				break;
			}
			in_node = key == "DatSpr";
			found_node = in_node;
			continue;
		}
		if (!in_node) {
			continue;
		}

		bool* flag = nullptr;
		if (key == "extended") {
			flag = &descriptor.extended;
		} else if (key == "transparency") {
			flag = &descriptor.transparency;
		} else if (key == "frame-durations") {
			flag = &descriptor.frame_durations;
		} else if (key == "frame-groups") {
			flag = &descriptor.frame_groups;
		} else if (key == "metadata-file") {
			if (!value.empty()) {
				descriptor.metadata_file = std::string(value);
			}
			continue;
		} else if (key == "sprites-file") {
			if (!value.empty()) {
				descriptor.sprites_file = std::string(value);
			}
			continue;
		} else {
			continue;
		}

		const auto parsed = ParseBool(value);
		if (!parsed) {
			error = std::format("Invalid boolean value '{}' for '{}'", value, key);
			return std::nullopt;
		}
		*flag = *parsed;
	}

	if (!found_node) {
		error = "'DatSpr' tag not found";
		return std::nullopt;
	}
	return descriptor;
}
