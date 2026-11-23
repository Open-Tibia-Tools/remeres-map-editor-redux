//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_UTIL_LOAD_WARNINGS_LOG_H_
#define RME_UTIL_LOAD_WARNINGS_LOG_H_

#include <span>
#include <string>
#include <string_view>

inline constexpr std::string_view kLoadWarningsLogFileName = "load-warnings.log";

// One block of load-warnings.log: a "[timestamp] title (N)" header and one line per
// warning. Multi-line warnings are split so every line of the file is one message line.
[[nodiscard]] std::string FormatLoadWarningsBlock(std::string_view timestamp, std::string_view title, std::span<const std::string> warnings);

#endif
