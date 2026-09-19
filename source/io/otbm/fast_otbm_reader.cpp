#include "io/otbm/fast_otbm_reader.h"
#include <algorithm>

bool FastOTBMStream::getString(std::string& out) {
	uint16_t len = 0;
	if (!getU16(len)) {
		return false;
	}
	out.clear();
	out.reserve(len);
	for (uint16_t i = 0; i < len; ++i) {
		uint8_t b = 0;
		if (!getByte(b)) {
			return false;
		}
		out.push_back(static_cast<char>(b));
	}
	return true;
}

bool FastOTBMStream::getLongString(std::string& out) {
	uint32_t len = 0;
	if (!getU32(len)) {
		return false;
	}
	out.clear();
	out.reserve(len);
	for (uint32_t i = 0; i < len; ++i) {
		uint8_t b = 0;
		if (!getByte(b)) {
			return false;
		}
		out.push_back(static_cast<char>(b));
	}
	return true;
}

bool FastOTBMStream::getRAW(uint8_t* dest, size_t count) {
	for (size_t i = 0; i < count; ++i) {
		uint8_t b = 0;
		if (!getByte(b)) {
			return false;
		}
		dest[i] = b;
	}
	return true;
}

bool FastOTBMStream::skip(size_t count) {
	for (size_t i = 0; i < count; ++i) {
		uint8_t b = 0;
		if (!getByte(b)) {
			return false;
		}
	}
	return true;
}

void FastOTBMStream::skipRemainingProps() noexcept {
	while (p < end && *p != OTBM_NODE_START && *p != OTBM_NODE_END) {
		if (*p == OTBM_ESCAPE_CHAR) {
			p += (p + 1 < end) ? 2 : 1;
		} else {
			p++;
		}
	}
}

void FastOTBMStream::skipNode() noexcept {
	int depth = 1;
	while (p < end && depth > 0) {
		uint8_t c = *p++;
		if (c == OTBM_ESCAPE_CHAR) {
			if (p < end) {
				p++;
			}
		} else if (c == OTBM_NODE_START) {
			depth++;
		} else if (c == OTBM_NODE_END) {
			depth--;
		}
	}
}

std::vector<uint8_t> FastOTBMStream::readRemainingRawProps() {
	std::vector<uint8_t> raw;
	while (hasMoreProps()) {
		uint8_t b = 0;
		if (getByte(b)) {
			raw.push_back(b);
		} else {
			break;
		}
	}
	return raw;
}

PreservedOTBMNode FastOTBMNode::capturePreserved() {
	PreservedOTBMNode preserved;
	preserved.rawPayload.push_back(type);

	// Read all remaining props
	while (stream.hasMoreProps()) {
		uint8_t b = 0;
		if (stream.getByte(b)) {
			preserved.rawPayload.push_back(b);
		} else {
			break;
		}
	}

	forEachChild([&](FastOTBMNode& child) {
		preserved.children.push_back(child.capturePreserved());
	});

	return preserved;
}
