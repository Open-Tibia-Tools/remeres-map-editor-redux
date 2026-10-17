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

#ifndef RME_COMMONS_H_
#define RME_COMMONS_H_

#include "app/main.h"
#include "map/position.h"
#include <stdio.h>
#include <stdlib.h>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <string>
#include <string_view>
#include <concepts>
#include <type_traits>

//
template <typename T1, typename T2>
	requires(std::integral<T1> || std::is_enum_v<T1>) && (std::integral<T2> || std::is_enum_v<T2>)
inline bool testFlags(T1 flags, T2 test) {
	return (static_cast<uint64_t>(flags) & static_cast<uint64_t>(test)) != 0;
}

int32_t uniform_random(int32_t minNumber, int32_t maxNumber);
int32_t uniform_random(int32_t maxNumber);

// Function-like convertions between float, int and doubles
std::string i2s(int i);
std::string f2s(double i);
int s2i(const std::string& s);
double s2f(const std::string& s);
wxString i2ws(int i);
wxString f2ws(double i);
int ws2i(wxString s);
double ws2f(wxString s);

inline wxString wxstr(std::string_view sv) {
	return wxString::FromUTF8(sv.data(), sv.length());
}

inline wxString wxstr(const std::string& str) {
	return wxString::FromUTF8(str.c_str(), str.length());
}

inline wxString wxstr(const char* str) {
	return wxString::FromUTF8(str);
}

// replaces all instances of sought in str with replacement
void replaceString(std::string& str, std::string_view sought, std::string_view replacement);
// Removes all characters in t from source (from either start or beginning of the string)
void trim_right(std::string& source, std::string_view t);
void trim_left(std::string& source, std::string_view t);
// Converts the argument to lower/uppercase
void to_lower_str(std::string& source);
void to_upper_str(std::string& source);
std::string as_lower_str(const std::string& other);
std::string as_upper_str(const std::string& other);
// Case-insensitive char predicate, e.g. std::ranges::equal(a, b, iequal)
inline constexpr auto iequal = [](char a, char b) {
	return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
};

// isFalseString returns true if the string is either "0", "false", "no", "not" or blank
// isTrueString returns the opposite value of isFalseString
bool isFalseString(const std::string& str);
bool isTrueString(const std::string& str);

// Generates a random number between low and high using the Mersenne Twister algorithm (std::mt19937).
// Swaps low and high if low > high for a more intuitive behavior.
int random(int high);
int random(int low, int high);

// Unicode conversions
std::wstring string2wstring(const std::string& utf8string);
std::string wstring2string(const std::wstring& widestring);

// Gets position values from ClipBoard
bool posFromClipboard(Position& position, const int mapWidth = MAP_MAX_WIDTH, const int mapHeight = MAP_MAX_HEIGHT);

// Returns 'yes' if the defined value is true or 'no' if it is false.
wxString b2yn(bool v);

wxColor colorFromEightBit(int color);

// Formats an integer with thousand separators (e.g. 1000 -> "1,000", 100000000 -> "100,000,000")
template <std::integral T>
inline std::string format_thousands_separator(T value, char separator = ',') {
	std::string s = std::to_string(value);
	int insert_pos = static_cast<int>(s.length()) - 3;
	const int min_pos = (value < 0) ? 1 : 0;
	while (insert_pos > min_pos) {
		s.insert(insert_pos, 1, separator);
		insert_pos -= 3;
	}
	return s;
}

// Standard math functions are now used via <cmath> and <algorithm>
// std::abs, std::min, std::max should be used instead.

#endif
