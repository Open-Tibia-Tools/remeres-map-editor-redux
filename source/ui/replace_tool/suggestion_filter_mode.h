#ifndef RME_SUGGESTION_FILTER_MODE_H_
#define RME_SUGGESTION_FILTER_MODE_H_

#include <cstdint>

enum class SuggestionFilterMode : uint8_t {
	None = 0,    // No filter (Visual only, Default)
	Similar = 1, // Compatible / Superset
	Exact = 2    // Identical attributes & flags
};

#endif // RME_SUGGESTION_FILTER_MODE_H_
