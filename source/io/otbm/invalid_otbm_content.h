#ifndef RME_IO_OTBM_INVALID_OTBM_CONTENT_H_
#define RME_IO_OTBM_INVALID_OTBM_CONTENT_H_

#include <cstdint>
#include <optional>
#include <vector>

struct PreservedOTBMNode {
	std::vector<uint8_t> rawPayload;
	std::vector<PreservedOTBMNode> children;

	bool operator==(const PreservedOTBMNode&) const = default;

	[[nodiscard]] bool empty() const {
		return rawPayload.empty() && children.empty();
	}
};

struct OpaqueTileAttributeRecord {
	std::vector<uint8_t> rawBytes;

	bool operator==(const OpaqueTileAttributeRecord&) const = default;
};

enum class InvalidOTBMItemMarkerColor : uint8_t {
	None,
	Red,
	Orange,
};

enum class InvalidOTBMItemKind : uint8_t {
	None,
	MissingGround,
	MissingItem,
};

struct InvalidOTBMItemData {
	InvalidOTBMItemKind kind = InvalidOTBMItemKind::None;
	std::vector<uint8_t> rawInlineBytes;
	std::optional<PreservedOTBMNode> rawNode;

	bool operator==(const InvalidOTBMItemData&) const = default;

	[[nodiscard]] bool isGroundLike() const {
		return kind == InvalidOTBMItemKind::MissingGround;
	}

	[[nodiscard]] bool hasRawInlineBytes() const {
		return !rawInlineBytes.empty();
	}

	[[nodiscard]] bool hasRawNode() const {
		return rawNode.has_value();
	}

	[[nodiscard]] bool hasPreservedContent() const {
		return hasRawInlineBytes() || hasRawNode();
	}

	[[nodiscard]] InvalidOTBMItemMarkerColor markerColor() const {
		switch (kind) {
			case InvalidOTBMItemKind::MissingGround:
				return InvalidOTBMItemMarkerColor::Red;
			case InvalidOTBMItemKind::MissingItem:
				return InvalidOTBMItemMarkerColor::Orange;
			case InvalidOTBMItemKind::None:
				return InvalidOTBMItemMarkerColor::None;
		}
		return InvalidOTBMItemMarkerColor::None;
	}
};

// Rare per-tile data kept off the hot Tile struct: preserved invalid OTBM content and Crystal Server zone ids.
struct TileExtraData {
	std::vector<OpaqueTileAttributeRecord> opaqueTileAttributes;
	std::vector<PreservedOTBMNode> opaqueChildNodes;
	std::vector<uint16_t> zoneIds; // sorted, unique, never 0
	uint32_t rawMapFlags = 0;
	uint32_t unknownMapFlagBits = 0;
	bool hasStructuralMismatch = false;

	bool operator==(const TileExtraData&) const = default;

	[[nodiscard]] bool hasInvalidContent() const {
		return !opaqueTileAttributes.empty() || !opaqueChildNodes.empty() || unknownMapFlagBits != 0 || hasStructuralMismatch;
	}

	[[nodiscard]] bool empty() const {
		return !hasInvalidContent() && zoneIds.empty();
	}
};

#endif
