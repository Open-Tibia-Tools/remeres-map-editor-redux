//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "util/load_warnings_log.h"

#include <format>
#include <fstream>
#include <system_error>

namespace {
    [[nodiscard]] std::string FormatBlockFromLines(std::string_view timestamp, std::string_view title, std::span<const std::string> lines)
    {
        if (lines.empty()) {
            return {};
        }

        std::string block = std::format("[{}] {} ({})\n", timestamp, title, lines.size());
        for (const std::string& line : lines) {
            block += "  ";
            block += line;
            block += '\n';
        }
        return block;
    }
} // namespace

std::vector<std::string> SplitNonEmptyWarningLines(std::span<const std::string> warnings)
{
    std::vector<std::string> lines;
    for (const std::string& warning : warnings) {
        std::string_view rest = warning;
        while (true) {
            const size_t eol = rest.find('\n');
            std::string_view line = rest.substr(0, eol);
            if (line.ends_with('\r')) {
                line.remove_suffix(1);
            }
            if (!line.empty()) {
                lines.emplace_back(line);
            }
            if (eol == std::string_view::npos) {
                break;
            }
            rest.remove_prefix(eol + 1);
        }
    }
    return lines;
}

std::string FormatLoadWarningsBlock(std::string_view timestamp, std::string_view title, std::span<const std::string> warnings)
{
    const std::vector<std::string> lines = SplitNonEmptyWarningLines(warnings);
    return FormatBlockFromLines(timestamp, title, lines);
}

LoadWarningsLogResult TryAppendLoadWarnings(
    std::span<const std::filesystem::path> candidate_paths, std::string_view timestamp, std::string_view title,
    std::span<const std::string> warnings, std::uintmax_t max_bytes
)
{
    const std::vector<std::string> lines = SplitNonEmptyWarningLines(warnings);
    if (lines.empty()) {
        return {LoadWarningsLogStatus::Empty, {}, 0};
    }

    const std::string block = FormatBlockFromLines(timestamp, title, lines);
    std::filesystem::path last_attempted;

    for (const std::filesystem::path& path : candidate_paths) {
        if (path.empty()) {
            continue;
        }
        last_attempted = path;

        std::error_code ec;
        if (const std::filesystem::path parent = path.parent_path(); !parent.empty()) {
            std::filesystem::create_directories(parent, ec);
            if (ec) {
                continue;
            }
        }

        std::ios_base::openmode open_mode = std::ios::app | std::ios::binary;
        const std::uintmax_t existing_size = std::filesystem::file_size(path, ec);
        if (!ec && existing_size >= max_bytes) {
            std::filesystem::path rotated = path;
            rotated += ".1";
            std::error_code rotate_ec;
            std::filesystem::remove(rotated, rotate_ec);
            rotate_ec.clear();
            std::filesystem::rename(path, rotated, rotate_ec);
            if (rotate_ec) {
                open_mode = std::ios::trunc | std::ios::binary;
            }
        }

        std::ofstream log(path, open_mode);
        if (!log) {
            continue;
        }
        log.write(block.data(), static_cast<std::streamsize>(block.size()));
        log.flush();
        if (!log) {
            continue;
        }

        return {LoadWarningsLogStatus::Written, path, lines.size()};
    }

    return {LoadWarningsLogStatus::Failed, last_attempted, lines.size()};
}
