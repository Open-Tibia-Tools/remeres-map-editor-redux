//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#include "app/definitions.h"
#include "util/common.h"

#include <cmath>
#include <cstdio>
#include <format>
#include <random>
#include <algorithm>
#include <array>

#if defined(_WIN32)
	#define WIN32_LEAN_AND_MEAN
	#define NOMINMAX
	#include <windows.h>
#else
	#include <filesystem>
#endif

// random generator
std::mt19937& getRandomGenerator() {
	static std::random_device rd;
	thread_local std::mt19937 generator(rd());
	return generator;
}

int32_t uniform_random(int32_t minNumber, int32_t maxNumber) {
	thread_local std::uniform_int_distribution<int32_t> uniformRand;
	if (minNumber == maxNumber) {
		return minNumber;
	} else if (minNumber > maxNumber) {
		std::swap(minNumber, maxNumber);
	}
	return uniformRand(getRandomGenerator(), std::uniform_int_distribution<int32_t>::param_type(minNumber, maxNumber));
}

int32_t uniform_random(int32_t maxNumber) {
	return uniform_random(0, maxNumber);
}

//
std::string i2s(const int _i) {
	return std::to_string(_i);
}

std::string f2s(const double _d) {
	return std::format("{:.6g}", _d);
}

int s2i(const std::string& s) {
	try {
		return std::stoi(s);
	} catch (const std::exception&) {
		return 0;
	}
}

double s2f(const std::string& s) {
	try {
		return std::stod(s);
	} catch (const std::exception&) {
		return 0.0;
	}
}

void replaceString(std::string& str, std::string_view sought, std::string_view replacement) {
	if (sought.empty()) {
		return;
	}
	size_t pos = 0;
	size_t soughtLen = sought.length();
	size_t replaceLen = replacement.length();
	while ((pos = str.find(sought, pos)) != std::string::npos) {
		str.replace(pos, soughtLen, replacement);
		pos += replaceLen;
	}
}

void trim_right(std::string& source, std::string_view t) {
	source.erase(source.find_last_not_of(t) + 1);
}

void trim_left(std::string& source, std::string_view t) {
	source.erase(0, source.find_first_not_of(t));
}

void to_lower_str(std::string& source) {
	std::ranges::transform(source, source.begin(), [](unsigned char c) { return std::tolower(c); });
}

void to_upper_str(std::string& source) {
	std::transform(source.begin(), source.end(), source.begin(), [](unsigned char c) { return std::toupper(c); });
}

std::string as_lower_str(const std::string& other) {
	std::string ret = other;
	to_lower_str(ret);
	return ret;
}

std::string as_upper_str(const std::string& other) {
	std::string ret = other;
	to_upper_str(ret);
	return ret;
}

bool isFalseString(const std::string& str) {
	static constexpr std::array<std::string_view, 5> falseStrings = {"false", "0", "", "no", "not"};
	return std::ranges::any_of(falseStrings, [&](const auto& s) { return s == str; });
}

bool isTrueString(const std::string& str) {
	return !isFalseString(str);
}

int random(int low, int high) {
	return uniform_random(low, high);
}

int random(int high) {
	return random(0, high);
}

std::wstring string2wstring(const std::string& utf8string) {
	if (utf8string.empty()) {
		return {};
	}
#if defined(_WIN32)
	const int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, utf8string.data(), static_cast<int>(utf8string.size()), nullptr, 0);
	std::wstring result(sizeNeeded, 0);
	MultiByteToWideChar(CP_UTF8, 0, utf8string.data(), static_cast<int>(utf8string.size()), result.data(), sizeNeeded);
	return result;
#else
	return std::filesystem::path(utf8string).wstring();
#endif
}

std::string wstring2string(const std::wstring& widestring) {
	if (widestring.empty()) {
		return {};
	}
#if defined(_WIN32)
	const int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, widestring.data(), static_cast<int>(widestring.size()), nullptr, 0, nullptr, nullptr);
	std::string result(sizeNeeded, 0);
	WideCharToMultiByte(CP_UTF8, 0, widestring.data(), static_cast<int>(widestring.size()), result.data(), sizeNeeded, nullptr, nullptr);
	return result;
#else
	return std::filesystem::path(widestring).string();
#endif
}
