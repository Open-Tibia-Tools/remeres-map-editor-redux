#include "util/load_warnings_log.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {
    struct TempDirGuard {
        std::filesystem::path path;

        explicit TempDirGuard(std::string_view suffix)
        {
            const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
            path = std::filesystem::temp_directory_path()
                / std::string("rme_test_load_warnings_").append(suffix).append("_").append(std::to_string(ticks));
            std::filesystem::create_directories(path);
        }

        ~TempDirGuard()
        {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }
    };

    std::string ReadFile(const std::filesystem::path& p)
    {
        std::ifstream in(p, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
} // namespace

TEST_CASE("a block has a header and one indented line per warning", "[load_warnings_log]")
{
    const std::vector<std::string> warnings = {"Item 100 has no sprite", "Unknown flag 0x20"};
    CHECK(
        FormatLoadWarningsBlock("2026-11-23 18:00:00", "Warnings", warnings)
        == "[2026-11-23 18:00:00] Warnings (2)\n"
           "  Item 100 has no sprite\n"
           "  Unknown flag 0x20\n"
    );
}

TEST_CASE("multi-line warnings are split, blank lines dropped, and header counts emitted lines", "[load_warnings_log]")
{
    const std::vector<std::string> warnings = {"first\r\nsecond\n\nthird\n", ""};
    CHECK(
        FormatLoadWarningsBlock("t", "Map loader errors", warnings)
        == "[t] Map loader errors (3)\n"
           "  first\n"
           "  second\n"
           "  third\n"
    );
}

TEST_CASE("empty and whitespace-newline-only warning lists produce no block or lines", "[load_warnings_log]")
{
    CHECK(SplitNonEmptyWarningLines({}).empty());
    CHECK(FormatLoadWarningsBlock("t", "Warnings", {}).empty());

    const std::vector<std::string> blank_entries = {"", "\n", "\r\n", "\n\r\n"};
    CHECK(SplitNonEmptyWarningLines(blank_entries).empty());
    CHECK(FormatLoadWarningsBlock("t", "Warnings", blank_entries).empty());
}

TEST_CASE("TryAppendLoadWarnings skips file creation on empty or blank-only warnings", "[load_warnings_log]")
{
    TempDirGuard tmp("empty");
    const std::filesystem::path log_path = tmp.path / kLoadWarningsLogFileName;
    const std::vector<std::filesystem::path> candidates = {log_path};
    const std::vector<std::string> blank_entries = {"", "\n"};

    const LoadWarningsLogResult res = TryAppendLoadWarnings(candidates, "t", "Warnings", blank_entries);
    CHECK(res.status == LoadWarningsLogStatus::Empty);
    CHECK(res.line_count == 0);
    CHECK_FALSE(std::filesystem::exists(log_path));
}

TEST_CASE("TryAppendLoadWarnings appends sequentially and falls back across candidates", "[load_warnings_log]")
{
    TempDirGuard tmp("fallback");

    // A regular file as the parent path makes candidate 1 unwritable on both Windows and Linux.
    const std::filesystem::path blocker_file = tmp.path / "not_a_dir";
    {
        std::ofstream f(blocker_file, std::ios::binary);
        f << "block";
    }
    const std::filesystem::path bad_candidate = blocker_file / kLoadWarningsLogFileName;
    const std::filesystem::path good_candidate = tmp.path / "writable" / kLoadWarningsLogFileName;
    const std::vector<std::filesystem::path> candidates = {bad_candidate, good_candidate};

    const std::vector<std::string> first = {"First warning"};
    const LoadWarningsLogResult res1 = TryAppendLoadWarnings(candidates, "t1", "Warnings", first);
    REQUIRE(res1.status == LoadWarningsLogStatus::Written);
    CHECK(res1.path == good_candidate);
    CHECK(res1.line_count == 1);

    const std::vector<std::string> second = {"Second warning"};
    const LoadWarningsLogResult res2 = TryAppendLoadWarnings(candidates, "t2", "Map loader errors", second);
    REQUIRE(res2.status == LoadWarningsLogStatus::Written);
    CHECK(res2.path == good_candidate);
    CHECK(
        ReadFile(good_candidate)
        == "[t1] Warnings (1)\n"
           "  First warning\n"
           "[t2] Map loader errors (1)\n"
           "  Second warning\n"
    );
}

TEST_CASE("TryAppendLoadWarnings rotates log file to .1 when size reaches max_bytes", "[load_warnings_log]")
{
    TempDirGuard tmp("rotate");
    const std::filesystem::path log_path = tmp.path / kLoadWarningsLogFileName;
    std::filesystem::path rotated_path = log_path;
    rotated_path += ".1";
    const std::vector<std::filesystem::path> candidates = {log_path};

    const std::vector<std::string> first = {"1234567890"};
    REQUIRE(TryAppendLoadWarnings(candidates, "t1", "Warnings", first, 20).status == LoadWarningsLogStatus::Written);
    REQUIRE(std::filesystem::file_size(log_path) >= 20);

    const std::vector<std::string> second = {"after-rotation"};
    const LoadWarningsLogResult res2 = TryAppendLoadWarnings(candidates, "t2", "Warnings", second, 20);
    REQUIRE(res2.status == LoadWarningsLogStatus::Written);
    REQUIRE(std::filesystem::exists(rotated_path));
    CHECK(
        ReadFile(rotated_path)
        == "[t1] Warnings (1)\n"
           "  1234567890\n"
    );
    CHECK(
        ReadFile(log_path)
        == "[t2] Warnings (1)\n"
           "  after-rotation\n"
    );
}

TEST_CASE("TryAppendLoadWarnings returns Failed when no candidate is writable", "[load_warnings_log]")
{
    TempDirGuard tmp("fail");
    const std::filesystem::path blocker_file = tmp.path / "not_a_dir";
    {
        std::ofstream f(blocker_file, std::ios::binary);
        f << "block";
    }
    const std::filesystem::path bad_candidate = blocker_file / kLoadWarningsLogFileName;
    const std::vector<std::filesystem::path> candidates = {bad_candidate};
    const std::vector<std::string> warnings = {"Some warning"};

    const LoadWarningsLogResult res = TryAppendLoadWarnings(candidates, "t", "Warnings", warnings);
    CHECK(res.status == LoadWarningsLogStatus::Failed);
    CHECK(res.path == bad_candidate);
    CHECK(res.line_count == 1);
}
