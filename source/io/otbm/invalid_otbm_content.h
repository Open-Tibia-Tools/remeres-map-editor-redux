#ifndef RME_IO_OTBM_INVALID_OTBM_CONTENT_H_
#define RME_IO_OTBM_INVALID_OTBM_CONTENT_H_

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
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

// Preserved invalid OTBM content (only present on corrupted or unsupported maps)
struct TileInvalidContent {
	std::vector<OpaqueTileAttributeRecord> opaqueTileAttributes;
	std::vector<PreservedOTBMNode> opaqueChildNodes;
	uint32_t rawMapFlags = 0;
	uint32_t unknownMapFlagBits = 0;
	bool hasStructuralMismatch = false;

	bool operator==(const TileInvalidContent&) const = default;

	[[nodiscard]] bool empty() const noexcept {
		return opaqueTileAttributes.empty() && opaqueChildNodes.empty() &&
		       unknownMapFlagBits == 0 && !hasStructuralMismatch;
	}
};

// Compact inline zone list: stores up to 3 zone IDs inline without dynamic heap allocations.
// Falls back to std::vector<uint16_t> on the heap only if a tile belongs to >= 4 zones.
class CompactZoneList {
public:
	static constexpr size_t kInlineCapacity = 3;

	CompactZoneList() noexcept : size_(0) {}
	~CompactZoneList() {
		if (isHeap()) {
			delete heap_ptr_;
		}
	}

	CompactZoneList(const CompactZoneList& other) : size_(other.size_) {
		if (other.isHeap()) {
			heap_ptr_ = new std::vector<uint16_t>(*other.heap_ptr_);
		} else {
			for (size_t i = 0; i < other.size_; ++i) {
				inline_buffer_[i] = other.inline_buffer_[i];
			}
		}
	}

	CompactZoneList& operator=(const CompactZoneList& other) {
		if (this != &other) {
			if (isHeap()) {
				delete heap_ptr_;
			}
			size_ = other.size_;
			if (other.isHeap()) {
				heap_ptr_ = new std::vector<uint16_t>(*other.heap_ptr_);
			} else {
				for (size_t i = 0; i < other.size_; ++i) {
					inline_buffer_[i] = other.inline_buffer_[i];
				}
			}
		}
		return *this;
	}

	CompactZoneList(CompactZoneList&& other) noexcept : size_(other.size_) {
		if (other.isHeap()) {
			heap_ptr_ = other.heap_ptr_;
		} else {
			for (size_t i = 0; i < other.size_; ++i) {
				inline_buffer_[i] = other.inline_buffer_[i];
			}
		}
		other.size_ = 0;
	}

	CompactZoneList& operator=(CompactZoneList&& other) noexcept {
		if (this != &other) {
			if (isHeap()) {
				delete heap_ptr_;
			}
			size_ = other.size_;
			if (other.isHeap()) {
				heap_ptr_ = other.heap_ptr_;
			} else {
				for (size_t i = 0; i < other.size_; ++i) {
					inline_buffer_[i] = other.inline_buffer_[i];
				}
			}
			other.size_ = 0;
		}
		return *this;
	}

	[[nodiscard]] std::span<const uint16_t> span() const noexcept {
		if (isHeap()) {
			return *heap_ptr_;
		}
		return { inline_buffer_, size_ };
	}

	[[nodiscard]] auto begin() const noexcept { return span().begin(); }
	[[nodiscard]] auto end() const noexcept { return span().end(); }

	[[nodiscard]] bool empty() const noexcept {
		return isHeap() ? heap_ptr_->empty() : (size_ == 0);
	}

	[[nodiscard]] size_t size() const noexcept {
		return isHeap() ? heap_ptr_->size() : size_;
	}

	[[nodiscard]] size_t heapSize() const noexcept {
		return isHeap() ? (sizeof(std::vector<uint16_t>) + heap_ptr_->capacity() * sizeof(uint16_t)) : 0;
	}

	void insert(uint16_t id) {
		if (isHeap()) {
			auto it = std::ranges::lower_bound(*heap_ptr_, id);
			if (it == heap_ptr_->end() || *it != id) {
				heap_ptr_->insert(it, id);
			}
			return;
		}

		auto current = span();
		auto it = std::ranges::lower_bound(current, id);
		if (it != current.end() && *it == id) {
			return;
		}

		if (size_ < kInlineCapacity) {
			const size_t idx = static_cast<size_t>(std::distance(current.begin(), it));
			for (size_t i = size_; i > idx; --i) {
				inline_buffer_[i] = inline_buffer_[i - 1];
			}
			inline_buffer_[idx] = id;
			++size_;
		} else {
			auto* v = new std::vector<uint16_t>(inline_buffer_, inline_buffer_ + size_);
			auto it_v = std::ranges::lower_bound(*v, id);
			v->insert(it_v, id);
			heap_ptr_ = v;
			size_ = kHeapMarker;
		}
	}

	void erase(uint16_t id) {
		if (isHeap()) {
			auto it = std::ranges::lower_bound(*heap_ptr_, id);
			if (it != heap_ptr_->end() && *it == id) {
				heap_ptr_->erase(it);
				if (heap_ptr_->size() <= kInlineCapacity) {
					auto* v = heap_ptr_;
					const size_t n = v->size();
					for (size_t i = 0; i < n; ++i) {
						inline_buffer_[i] = (*v)[i];
					}
					delete v;
					size_ = static_cast<uint8_t>(n);
				}
			}
			return;
		}

		auto current = span();
		auto it = std::ranges::lower_bound(current, id);
		if (it != current.end() && *it == id) {
			const size_t idx = static_cast<size_t>(std::distance(current.begin(), it));
			for (size_t i = idx; i + 1 < size_; ++i) {
				inline_buffer_[i] = inline_buffer_[i + 1];
			}
			--size_;
		}
	}

	void assign(std::span<const uint16_t> ids) {
		if (ids.size() <= kInlineCapacity) {
			std::array<uint16_t, kInlineCapacity> tmp;
			for (size_t i = 0; i < ids.size(); ++i) {
				tmp[i] = ids[i];
			}
			if (isHeap()) {
				delete heap_ptr_;
			}
			size_ = static_cast<uint8_t>(ids.size());
			for (size_t i = 0; i < ids.size(); ++i) {
				inline_buffer_[i] = tmp[i];
			}
		} else {
			if (!isHeap()) {
				heap_ptr_ = new std::vector<uint16_t>(ids.begin(), ids.end());
			} else {
				if (ids.data() >= heap_ptr_->data() && ids.data() < heap_ptr_->data() + heap_ptr_->size()) {
					std::vector<uint16_t> tmp(ids.begin(), ids.end());
					heap_ptr_->assign(tmp.begin(), tmp.end());
				} else {
					heap_ptr_->assign(ids.begin(), ids.end());
				}
			}
			size_ = kHeapMarker;
		}
	}

	void clear() noexcept {
		if (isHeap()) {
			delete heap_ptr_;
		}
		size_ = 0;
	}

	bool operator==(const CompactZoneList& other) const noexcept {
		return std::ranges::equal(span(), other.span());
	}

private:
	static constexpr uint8_t kHeapMarker = 0xFF;
	[[nodiscard]] bool isHeap() const noexcept { return size_ == kHeapMarker; }

	union {
		uint16_t inline_buffer_[kInlineCapacity];
		std::vector<uint16_t>* heap_ptr_;
	};
	uint8_t size_;
};

// Rare per-tile data kept off the hot Tile struct: preserved invalid OTBM content and Crystal Server zone ids.
struct TileExtraData {
	CompactZoneList zoneIds;
	std::unique_ptr<TileInvalidContent> invalidContent;

	TileExtraData() = default;

	TileExtraData(const TileExtraData& other)
		: zoneIds(other.zoneIds)
		, invalidContent(other.invalidContent ? std::make_unique<TileInvalidContent>(*other.invalidContent) : nullptr) {}

	TileExtraData& operator=(const TileExtraData& other) {
		if (this != &other) {
			zoneIds = other.zoneIds;
			invalidContent = other.invalidContent ? std::make_unique<TileInvalidContent>(*other.invalidContent) : nullptr;
		}
		return *this;
	}

	TileExtraData(TileExtraData&&) noexcept = default;
	TileExtraData& operator=(TileExtraData&&) noexcept = default;

	bool operator==(const TileExtraData& other) const noexcept {
		if (zoneIds != other.zoneIds) {
			return false;
		}
		const bool has_this_inv = invalidContent && !invalidContent->empty();
		const bool has_other_inv = other.invalidContent && !other.invalidContent->empty();
		if (has_this_inv != has_other_inv) {
			return false;
		}
		if (has_this_inv && *invalidContent != *other.invalidContent) {
			return false;
		}
		return true;
	}

	[[nodiscard]] bool hasInvalidContent() const noexcept {
		return invalidContent && !invalidContent->empty();
	}

	[[nodiscard]] bool empty() const noexcept {
		return zoneIds.empty() && !hasInvalidContent();
	}
};

#endif
