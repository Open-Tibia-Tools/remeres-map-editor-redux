//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "util/load_warnings_log.h"

#include <format>

std::string FormatLoadWarningsBlock(std::string_view timestamp, std::string_view title, std::span<const std::string> warnings) {
	std::string block = std::format("[{}] {} ({})\n", timestamp, title, warnings.size());
	for (const std::string& warning : warnings) {
		std::string_view rest = warning;
		while (true) {
			const size_t eol = rest.find('\n');
			std::string_view line = rest.substr(0, eol);
			if (line.ends_with('\r')) {
				line.remove_suffix(1);
			}
			if (!line.empty()) {
				block += "  ";
				block += line;
				block += '\n';
			}
			if (eol == std::string_view::npos) {
				break;
			}
			rest.remove_prefix(eol + 1);
		}
	}
	return block;
}
