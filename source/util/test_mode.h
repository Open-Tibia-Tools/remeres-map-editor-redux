//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_UTIL_TEST_MODE_H_
#define RME_UTIL_TEST_MODE_H_

#include <cstdlib>

namespace rme {

inline bool& GetAutomatedTestModeFlag() {
	static bool s_automated_test_mode = false;
	return s_automated_test_mode;
}

inline void SetAutomatedTestMode(bool enabled) {
	GetAutomatedTestModeFlag() = enabled;
}

inline bool IsAutomatedTestMode() {
	if (GetAutomatedTestModeFlag()) {
		return true;
	}
	static const bool s_env_test = []() {
		return std::getenv("RME_AUTOMATED_TESTS") != nullptr ||
		       std::getenv("RME_TEST") != nullptr ||
		       std::getenv("CI") != nullptr ||
		       std::getenv("AUTOTEST") != nullptr;
	}();
	return s_env_test;
}

inline bool ShouldSilenceBrushWarnings() {
	if (IsAutomatedTestMode()) {
		return true;
	}
	static const bool s_env_silent_brushes = []() {
		return std::getenv("RME_SILENT_BRUSH_WARNINGS") != nullptr ||
		       std::getenv("RME_SILENT_WARNINGS") != nullptr;
	}();
	return s_env_silent_brushes;
}

} // namespace rme

#endif
