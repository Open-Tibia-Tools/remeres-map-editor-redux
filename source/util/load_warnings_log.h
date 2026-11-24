//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_UTIL_LOAD_WARNINGS_LOG_H_
#define RME_UTIL_LOAD_WARNINGS_LOG_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

inline constexpr std::string_view kLoadWarningsLogFileName = "load-warnings.log";
inline constexpr std::uintmax_t kMaxLoadWarningsLogBytes = 5u * 1024u * 1024u;

enum class LoadWarningsLogStatus {
    Empty,
    Written,
    Failed,
};

struct LoadWarningsLogResult {
    LoadWarningsLogStatus status = LoadWarningsLogStatus::Failed;
    std::filesystem::path path;
    std::size_t line_count = 0;
};

// Splits raw warning entries on '\n', strips trailing '\r', and drops blank lines.
[[nodiscard]] std::vector<std::string> SplitNonEmptyWarningLines(std::span<const std::string> warnings);

// One block of load-warnings.log: a "[timestamp] title (N)" header and one indented line
// per non-empty warning line. Returns an empty string when no non-empty lines remain.
[[nodiscard]] std::string
FormatLoadWarningsBlock(std::string_view timestamp, std::string_view title, std::span<const std::string> warnings);

// Appends a formatted warning block to the first writable candidate path, rotating to "<path>.1"
// (or truncating if rename fails) when the log reaches max_bytes.
[[nodiscard]] LoadWarningsLogResult TryAppendLoadWarnings(
    std::span<const std::filesystem::path> candidate_paths, std::string_view timestamp, std::string_view title,
    std::span<const std::string> warnings, std::uintmax_t max_bytes = kMaxLoadWarningsLogBytes
);

#endif
