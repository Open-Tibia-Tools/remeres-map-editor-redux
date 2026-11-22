//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_UI_WX_COMMON_H_
#define RME_UI_WX_COMMON_H_

#include "app/definitions.h"
#include "map/position.h"
#include <string>
#include <string_view>
#include <wx/string.h>
#include <wx/colour.h>

#define __W_RME_APPLICATION_NAME__ wxString(__RME_APPLICATION_NAME__)
#define __W_RME_VERSION__ wxstr(__RME_VERSION__)

#define nstr(str) ((str).ToStdString())

inline wxString wxstr(std::string_view sv) {
	return wxString::FromUTF8(sv.data(), sv.length());
}

inline wxString wxstr(const std::string& str) {
	return wxString::FromUTF8(str.c_str(), str.length());
}

inline wxString wxstr(const char* str) {
	return wxString::FromUTF8(str);
}

wxString i2ws(int i);
wxString f2ws(double i);
int ws2i(const wxString& s);
double ws2f(const wxString& s);
wxString b2yn(bool v);
wxColor colorFromEightBit(int color);
bool posFromClipboard(Position& position, int mapWidth = MAP_MAX_WIDTH, int mapHeight = MAP_MAX_HEIGHT);

#endif // RME_UI_WX_COMMON_H_
