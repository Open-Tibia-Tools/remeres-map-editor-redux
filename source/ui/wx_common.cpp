//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "ui/wx_common.h"
#include <wx/clipbrd.h>
#include <regex>

wxString i2ws(const int _i) {
	wxString str;
	str << _i;
	return str;
}

wxString f2ws(const double _d) {
	wxString str;
	str << _d;
	return str;
}

int ws2i(const wxString& s) {
	long _i;
	if (s.ToLong(&_i)) {
		return static_cast<int>(_i);
	}
	return 0;
}

double ws2f(const wxString& s) {
	double _d;
	if (s.ToDouble(&_d)) {
		return _d;
	}
	return 0.0;
}

wxString b2yn(bool value) {
	return value ? "Yes" : "No";
}

wxColor colorFromEightBit(int color) {
	if (color <= 0 || color >= 216) {
		return wxColor(0, 0, 0);
	}
	const uint8_t red = static_cast<uint8_t>((color / 36) % 6 * 51);
	const uint8_t green = static_cast<uint8_t>((color / 6) % 6 * 51);
	const uint8_t blue = static_cast<uint8_t>(color % 6 * 51);
	return wxColor(red, green, blue);
}

bool posFromClipboard(Position& position, const int mapWidth, const int mapHeight) {
	wxClipboardLocker locker(wxTheClipboard);
	if (!locker) {
		return false;
	}

	if (!wxTheClipboard->IsSupported(wxDF_TEXT)) {
		return false;
	}

	wxTextDataObject data;
	wxTheClipboard->GetData(data);

	std::string input = data.GetText().ToStdString();
	if (input.empty()) {
		return false;
	}

	bool done = false;
	std::smatch matches;
	static const std::regex expression = std::regex(R"(.*?(\d+).*?(\d+).*?(\d+).*?)", std::regex_constants::ECMAScript);
	if (std::regex_match(input, matches, expression)) {
		try {
			const int tmpX = std::stoi(matches.str(1));
			const int tmpY = std::stoi(matches.str(2));
			const int tmpZ = std::stoi(matches.str(3));

			const Position pastedPos = Position(tmpX, tmpY, tmpZ);
			if (pastedPos.isValid() && tmpX <= mapWidth && tmpY <= mapHeight) {
				position.x = tmpX;
				position.y = tmpY;
				position.z = tmpZ;
				done = true;
			}
		} catch (const std::out_of_range&) { }
	}

	return done;
}
