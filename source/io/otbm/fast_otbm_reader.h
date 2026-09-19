#ifndef RME_FAST_OTBM_READER_H_
#define RME_FAST_OTBM_READER_H_

#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <concepts>
#include "io/otbm/otbm_types.h"
#include "io/otbm/invalid_otbm_content.h"

enum OTBM_SpecialChar : uint8_t {
	OTBM_NODE_START = 0xFE,
	OTBM_NODE_END = 0xFF,
	OTBM_ESCAPE_CHAR = 0xFD,
};

class FastOTBMStream {
public:
	const uint8_t* p;
	const uint8_t* end;

	FastOTBMStream(const uint8_t* data, size_t size) noexcept :
		p(data), end(data + size) { }
	FastOTBMStream(const uint8_t* start, const uint8_t* end) noexcept :
		p(start), end(end) { }

	[[nodiscard]] inline bool hasMore() const noexcept {
		return p < end;
	}

	[[nodiscard]] inline size_t remaining() const noexcept {
		return p < end ? static_cast<size_t>(end - p) : 0;
	}

	[[nodiscard]] inline uint8_t peekByte() const noexcept {
		return p < end ? *p : 0;
	}

	inline uint8_t readByte() noexcept {
		if (p >= end) {
			return 0;
		}
		uint8_t b = *p++;
		if (b == OTBM_ESCAPE_CHAR && p < end) {
			b = *p++;
		}
		return b;
	}

	inline bool getByte(uint8_t& out) noexcept {
		if (p >= end || *p == OTBM_NODE_START || *p == OTBM_NODE_END) {
			return false;
		}
		out = readByte();
		return true;
	}

	inline bool getU8(uint8_t& out) noexcept {
		return getByte(out);
	}

	inline bool getU16(uint16_t& out) noexcept {
		if (p + 2 <= end &&
			p[0] != OTBM_ESCAPE_CHAR && p[1] != OTBM_ESCAPE_CHAR &&
			p[0] != OTBM_NODE_START && p[0] != OTBM_NODE_END &&
			p[1] != OTBM_NODE_START && p[1] != OTBM_NODE_END) {
			out = *reinterpret_cast<const uint16_t*>(p);
			p += 2;
			return true;
		}
		uint8_t b0, b1;
		if (!getByte(b0) || !getByte(b1)) {
			return false;
		}
		out = static_cast<uint16_t>(b0 | (static_cast<uint16_t>(b1) << 8));
		return true;
	}

	inline bool getU32(uint32_t& out) noexcept {
		if (p + 4 <= end &&
			p[0] != OTBM_ESCAPE_CHAR && p[1] != OTBM_ESCAPE_CHAR &&
			p[2] != OTBM_ESCAPE_CHAR && p[3] != OTBM_ESCAPE_CHAR &&
			p[0] != OTBM_NODE_START && p[0] != OTBM_NODE_END &&
			p[1] != OTBM_NODE_START && p[1] != OTBM_NODE_END &&
			p[2] != OTBM_NODE_START && p[2] != OTBM_NODE_END &&
			p[3] != OTBM_NODE_START && p[3] != OTBM_NODE_END) {
			out = *reinterpret_cast<const uint32_t*>(p);
			p += 4;
			return true;
		}
		uint8_t b0, b1, b2, b3;
		if (!getByte(b0) || !getByte(b1) || !getByte(b2) || !getByte(b3)) {
			return false;
		}
		out = static_cast<uint32_t>(b0) |
			(static_cast<uint32_t>(b1) << 8) |
			(static_cast<uint32_t>(b2) << 16) |
			(static_cast<uint32_t>(b3) << 24);
		return true;
	}

	inline bool getU64(uint64_t& out) noexcept {
		uint32_t low = 0, high = 0;
		if (!getU32(low) || !getU32(high)) {
			return false;
		}
		out = static_cast<uint64_t>(low) | (static_cast<uint64_t>(high) << 32);
		return true;
	}

	bool getString(std::string& out);
	bool getLongString(std::string& out);
	bool getRAW(uint8_t* dest, size_t count);
	bool skip(size_t count);

	[[nodiscard]] inline bool hasMoreProps() const noexcept {
		return p < end && *p != OTBM_NODE_START && *p != OTBM_NODE_END;
	}

	void skipRemainingProps() noexcept;
	void skipNode() noexcept;

	// Extracts raw unescaped bytes of the remaining props in the current node
	std::vector<uint8_t> readRemainingRawProps();
};

class FastOTBMNode {
public:
	uint8_t type = 0;
	FastOTBMStream stream;
	bool closed = false;

	FastOTBMNode(uint8_t t, const uint8_t* start, const uint8_t* end) noexcept :
		type(t), stream(start, end), closed(false) { }

	template <typename Func>
		requires std::invocable<Func, FastOTBMNode&>
	void forEachChild(Func&& func) {
		stream.skipRemainingProps();

		while (stream.p < stream.end && *stream.p == OTBM_NODE_START) {
			stream.p++; // skip OTBM_NODE_START
			if (stream.p >= stream.end) {
				break;
			}
			uint8_t child_type = stream.readByte();

			FastOTBMNode child(child_type, stream.p, stream.end);
			func(child);

			if (!child.closed) {
				child.stream.skipNode();
			}
			stream.p = child.stream.p;
		}

		if (stream.p < stream.end && *stream.p == OTBM_NODE_END) {
			stream.p++; // consume closing OTBM_NODE_END
			closed = true;
		}
	}

	// Preserves this node and its children into a PreservedOTBMNode
	PreservedOTBMNode capturePreserved();
};

#endif // RME_FAST_OTBM_READER_H_
