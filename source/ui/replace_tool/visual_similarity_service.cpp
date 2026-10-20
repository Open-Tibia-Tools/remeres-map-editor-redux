#include "app/main.h"
#include "ui/replace_tool/visual_similarity_service.h"
#include "util/nvg_utils.h"
#include "ui/gui.h"
#include "item_definitions/core/item_definition_store.h"
#include "rendering/core/graphics.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <vector>
#include <memory>
#include <span>
#include <ranges>

VisualSimilarityService& VisualSimilarityService::Get() {
	static VisualSimilarityService instance;
	return instance;
}

namespace {
	// Suggestions below this score are visual noise: the aHash/Dice scoring
	// maps a perfect match to 1.0, so keep only convincing candidates.
	constexpr double kMinimumSimilarityScore = 0.55;
}

VisualSimilarityService::VisualSimilarityService() : m_timer(this), m_nextIdToIndex(1), isIndexed(false) {
	Bind(wxEVT_TIMER, &VisualSimilarityService::OnTimer, this, m_timer.GetId());
}

VisualSimilarityService::~VisualSimilarityService() {
	m_timer.Stop();
}

void VisualSimilarityService::StartIndexing() {
	if (isIndexed || m_timer.IsRunning()) {
		return;
	}
	m_timer.Start(50); // Faster background indexing
}

// ============================================================================
// CORE ALGORITHM HELPERS (Mappingtool 1:1)
// ============================================================================

static bool IsFullyOpaqueRGBA(std::span<const uint8_t> rgba, uint8_t threshold = 10) {
	for (size_t i = 3; i < rgba.size(); i += 4) {
		if (rgba[i] <= threshold) {
			return false;
		}
	}
	return true;
}

static uint64_t CalculateAHashRGBA(std::span<const uint8_t> rgba, int w, int h) {
	// mappingtool: 1. Resize to 8x8 using box sampling
	// grayscale = 0.299*R + 0.587*G + 0.114*B
	uint8_t gray_8x8[64];
	double total_brightness = 0;

	float block_w = (float)w / 8.0f;
	float block_h = (float)h / 8.0f;

	for (int y = 0; y < 8; ++y) {
		for (int x = 0; x < 8; ++x) {
			double block_sum = 0;
			int pixel_count = 0;

			int start_x = (int)(x * block_w);
			int start_y = (int)(y * block_h);
			int end_x = (int)((x + 1) * block_w);
			int end_y = (int)((y + 1) * block_h);

			for (int py = start_y; py < end_y && py < h; ++py) {
				for (int px = start_x; px < end_x && px < w; ++px) {
					int idx = (py * w + px) * 4;
					double gray = 0.299 * rgba[idx] + 0.587 * rgba[idx + 1] + 0.114 * rgba[idx + 2];
					block_sum += gray;
					pixel_count++;
				}
			}

			uint8_t avg = (uint8_t)((pixel_count > 0) ? (block_sum / pixel_count) : 0);
			gray_8x8[y * 8 + x] = avg;
			total_brightness += avg;
		}
	}

	uint8_t global_avg = (uint8_t)(total_brightness / 64.0);
	uint64_t hash = 0;
	for (int i = 0; i < 64; ++i) {
		if (gray_8x8[i] >= global_avg) {
			hash |= (1ULL << i);
		}
	}
	return hash;
}

static std::vector<uint64_t> ExtractPackedMaskRGBA(std::span<const uint8_t> rgba, int& outTruePixels) {
	const size_t count = rgba.size() / 4;
	std::vector<uint64_t> mask((count + 63) / 64, 0);
	size_t truePixels = 0;
	for (size_t i = 0; i < count; ++i) {
		// Strictly > 10 per mappingtool docs
		if (rgba[i * 4 + 3] > 10) {
			mask[i >> 6] |= 1ULL << (i & 63);
			++truePixels;
		}
	}
	outTruePixels = static_cast<int>(truePixels);
	return mask;
}

static int HammingDistance(uint64_t h1, uint64_t h2) {
	return std::popcount(h1 ^ h2);
}

static bool MaskBit(const std::vector<uint64_t>& mask, int x, int y, int width) {
	const size_t index = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
	return (mask[index >> 6] >> (index & 63)) & 1ULL;
}

/** Population count of the intersection of two equally sized masks. */
static int IntersectionCount(const std::vector<uint64_t>& a, const std::vector<uint64_t>& b) {
	const size_t words = std::min(a.size(), b.size());
	int count = 0;
	for (size_t i = 0; i < words; ++i) {
		count += std::popcount(a[i] & b[i]);
	}
	return count;
}

static int PopCount(const std::vector<uint64_t>& mask) {
	int count = 0;
	for (const uint64_t word : mask) {
		count += std::popcount(word);
	}
	return count;
}

/** Nearest neighbour scaling on packed masks; empty when the input is unusable. */
static std::vector<uint64_t> ResizePackedMask(const std::vector<uint64_t>& src, int srcW, int srcH, int dstW, int dstH) {
	if (srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0) {
		return {};
	}
	if (src.size() * 64 < static_cast<size_t>(srcW) * static_cast<size_t>(srcH)) {
		return {};
	}

	std::vector<uint64_t> dst((static_cast<size_t>(dstW) * static_cast<size_t>(dstH) + 63) / 64, 0);
	for (int y = 0; y < dstH; ++y) {
		const int sy = std::clamp((y * srcH) / dstH, 0, srcH - 1);
		for (int x = 0; x < dstW; ++x) {
			const int sx = std::clamp((x * srcW) / dstW, 0, srcW - 1);
			if (MaskBit(src, sx, sy, srcW)) {
				const size_t index = static_cast<size_t>(y) * static_cast<size_t>(dstW) + static_cast<size_t>(x);
				dst[index >> 6] |= 1ULL << (index & 63);
			}
		}
	}
	return dst;
}

// ============================================================================
// SERVICE IMPLEMENTATION
// ============================================================================

VisualSimilarityService::VisualItemData VisualSimilarityService::CalculateData(uint16_t itemId) {
	VisualItemData data;
	data.id = itemId;
	data.isOpaque = false;
	data.aHash = 0;
	data.width = 0;
	data.height = 0;
	data.truePixels = 0;

	if (!g_version.getLoadedVersion()) {
		return data;
	}

	const auto it = g_item_definitions.get(itemId);
	if (!it || it.clientId() == 0) {
		return data;
	}

	Sprite* sprite = g_gui.gfx.getSprite(it.clientId());
	if (!sprite) {
		return data;
	}

	GameSprite* gs = dynamic_cast<GameSprite*>(sprite);
	if (!gs) {
		return data;
	}

	int w, h;
	auto composite = NvgUtils::CreateCompositeRGBA(*gs, w, h);
	if (!composite) {
		return data;
	}

	data.width = w;
	data.height = h;
	data.isOpaque = IsFullyOpaqueRGBA({composite.get(), static_cast<size_t>(w * h * 4)});
	data.hasLight = gs->hasLight();
	data.isAnimated = gs->isAnimated();
	data.drawHeight = static_cast<uint16_t>(std::max(0, gs->getDrawHeight()));
	data.tileWidth = static_cast<uint8_t>(std::max(1, w / 32));
	data.tileHeight = static_cast<uint8_t>(std::max(1, h / 32));

	// Store both for robustness
	data.aHash = CalculateAHashRGBA({composite.get(), static_cast<size_t>(w * h * 4)}, w, h);
	data.packedMask = ExtractPackedMaskRGBA({composite.get(), static_cast<size_t>(w * h * 4)}, data.truePixels);

	return data;
}

void VisualSimilarityService::OnTimer(wxTimerEvent&) {
	int processed = 0;
	int maxId = g_item_definitions.getMaxID();

	// Process fewer items per tick to prevent UI lag and handle spikes
	while (processed < 100 && m_nextIdToIndex <= maxId) {
		uint16_t id = m_nextIdToIndex++;
		const auto it = g_item_definitions.get(id);
		if (it && it.clientId() != 0) {
			VisualItemData data = CalculateData(id);
			if (data.IsComplete()) {
				std::lock_guard<std::mutex> lock(dataMutex);
				itemDataCache[id] = std::move(data);
			}
		}
		processed++;
	}

	if (m_nextIdToIndex > maxId) {
		m_timer.Stop();
		std::lock_guard<std::mutex> lock(dataMutex);
		isIndexed = true;
		resultCache.clear();
	}
}

namespace {

constexpr uint64_t kSemanticFlags =
	flagMask(ItemFlag::IsWall) |
	flagMask(ItemFlag::IsBorder) |
	flagMask(ItemFlag::IsTable) |
	flagMask(ItemFlag::IsCarpet) |
	flagMask(ItemFlag::IsBrushDoor) |
	flagMask(ItemFlag::Pickupable) |
	flagMask(ItemFlag::Moveable) |
	flagMask(ItemFlag::Unpassable) |
	flagMask(ItemFlag::BlockMissiles) |
	flagMask(ItemFlag::BlockPathfinder) |
	flagMask(ItemFlag::HasElevation) |
	flagMask(ItemFlag::Stackable) |
	flagMask(ItemFlag::IsHangable) |
	flagMask(ItemFlag::Corpse) |
	flagMask(ItemFlag::FloorChange) |
	flagMask(ItemFlag::FullTile);

} // namespace

static bool MatchesSuggestionFilter(
	uint16_t sourceId,
	uint16_t targetId,
	SuggestionFilterMode mode,
	const VisualSimilarityService::VisualItemData& srcData,
	const VisualSimilarityService::VisualItemData& tgtData) {
	if (mode == SuggestionFilterMode::None || sourceId == targetId) {
		return true;
	}
	const auto srcView = g_item_definitions.get(sourceId);
	const auto tgtView = g_item_definitions.get(targetId);
	if (!srcView || !tgtView) {
		return false;
	}

	// 1. Physical boundaries: ground tiles, walls, doors, tables, carpets, borders
	if (srcView.isGroundTile() != tgtView.isGroundTile()) {
		return false;
	}
	if (srcView.hasFlag(ItemFlag::IsWall) != tgtView.hasFlag(ItemFlag::IsWall)) {
		return false;
	}

	const bool srcDoor = srcView.isDoor() || srcView.hasFlag(ItemFlag::IsBrushDoor);
	const bool tgtDoor = tgtView.isDoor() || tgtView.hasFlag(ItemFlag::IsBrushDoor);
	if (srcDoor != tgtDoor) {
		return false;
	}

	if (srcView.hasFlag(ItemFlag::IsTable) != tgtView.hasFlag(ItemFlag::IsTable)) {
		return false;
	}
	if (srcView.hasFlag(ItemFlag::IsCarpet) != tgtView.hasFlag(ItemFlag::IsCarpet)) {
		return false;
	}
	if (srcView.hasFlag(ItemFlag::IsBorder) != tgtView.hasFlag(ItemFlag::IsBorder)) {
		return false;
	}

	// 2. Client Properties: Multi-tile Dimensions (tibia.dat)
	const bool srcMultiTile = srcData.tileWidth > 1 || srcData.tileHeight > 1;
	const bool tgtMultiTile = tgtData.tileWidth > 1 || tgtData.tileHeight > 1;
	if (mode == SuggestionFilterMode::Exact) {
		if (srcData.tileWidth != tgtData.tileWidth || srcData.tileHeight != tgtData.tileHeight) {
			return false;
		}
	} else { // Similar
		if (srcMultiTile != tgtMultiTile) {
			return false;
		}
	}

	// 3. Client Properties: Light Emission (tibia.dat)
	if (mode == SuggestionFilterMode::Exact) {
		if (srcData.hasLight != tgtData.hasLight) {
			return false;
		}
	}

	// 4. Client Properties: Animation State (tibia.dat)
	if (mode == SuggestionFilterMode::Exact) {
		if (srcData.isAnimated != tgtData.isAnimated) {
			return false;
		}
	}

	// 5. Client & Server: Surface Elevation / Draw Height
	const bool srcElevation = srcView.hasFlag(ItemFlag::HasElevation) || srcData.drawHeight > 0;
	const bool tgtElevation = tgtView.hasFlag(ItemFlag::HasElevation) || tgtData.drawHeight > 0;
	if (mode == SuggestionFilterMode::Exact) {
		if (srcElevation != tgtElevation) {
			return false;
		}
	}

	// 6. Server Attributes: Containers & Storage (items.xml / OTB)
	const bool srcContainer = srcView.isContainer() || srcView.group() == ITEM_GROUP_CONTAINER || srcView.type() == ITEM_TYPE_CONTAINER;
	const bool tgtContainer = tgtView.isContainer() || tgtView.group() == ITEM_GROUP_CONTAINER || tgtView.type() == ITEM_TYPE_CONTAINER;
	if (srcContainer != tgtContainer) {
		return false;
	}
	if (srcContainer && mode == SuggestionFilterMode::Exact) {
		const auto srcSlot = static_cast<uint16_t>(srcView.attribute(ItemAttributeKey::SlotPosition));
		const auto tgtSlot = static_cast<uint16_t>(tgtView.attribute(ItemAttributeKey::SlotPosition));
		const bool srcBackpack = (srcSlot & SLOTP_BACKPACK) != 0;
		const bool tgtBackpack = (tgtSlot & SLOTP_BACKPACK) != 0;
		if (srcBackpack != tgtBackpack) {
			return false;
		}
	}

	// 7. Server Attributes: Weapons & Combat Gear (items.xml / OTB)
	const auto srcWeapon = static_cast<uint8_t>(srcView.attribute(ItemAttributeKey::WeaponType));
	const auto tgtWeapon = static_cast<uint8_t>(tgtView.attribute(ItemAttributeKey::WeaponType));
	const bool srcIsWeapon = srcWeapon != WEAPON_NONE || srcView.group() == ITEM_GROUP_WEAPON;
	const bool tgtIsWeapon = tgtWeapon != WEAPON_NONE || tgtView.group() == ITEM_GROUP_WEAPON;

	if (mode == SuggestionFilterMode::Exact) {
		if (srcIsWeapon != tgtIsWeapon) {
			return false;
		}
		if (srcWeapon != WEAPON_NONE && tgtWeapon != WEAPON_NONE && srcWeapon != tgtWeapon) {
			return false;
		}
		// Handedness check
		const auto srcSlot = static_cast<uint16_t>(srcView.attribute(ItemAttributeKey::SlotPosition));
		const auto tgtSlot = static_cast<uint16_t>(tgtView.attribute(ItemAttributeKey::SlotPosition));
		if ((srcSlot & SLOTP_TWO_HAND) != (tgtSlot & SLOTP_TWO_HAND)) {
			return false;
		}
	} else { // Similar
		if (srcIsWeapon && !tgtIsWeapon) {
			return false;
		}
		if (srcWeapon != WEAPON_NONE && tgtWeapon != WEAPON_NONE) {
			// Do not cross-mix distance weapons (bows/crossbows/ammo) with melee weapons
			const bool srcDistance = srcWeapon == WEAPON_DISTANCE || srcWeapon == WEAPON_AMMO;
			const bool tgtDistance = tgtWeapon == WEAPON_DISTANCE || tgtWeapon == WEAPON_AMMO;
			if (srcDistance != tgtDistance) {
				return false;
			}
		}
	}

	// 8. Server Attributes: Equipment Slots (Helmets, Armor, Legs, Boots, Rings, Amulets)
	const auto srcSlot = static_cast<uint16_t>(srcView.attribute(ItemAttributeKey::SlotPosition));
	const auto tgtSlot = static_cast<uint16_t>(tgtView.attribute(ItemAttributeKey::SlotPosition));
	if (srcSlot != 0 && tgtSlot != 0) {
		if (mode == SuggestionFilterMode::Exact) {
			constexpr uint16_t kEquipSlotMask = SLOTP_HEAD | SLOTP_ARMOR | SLOTP_LEGS | SLOTP_FEET | SLOTP_RING | SLOTP_NECKLACE;
			if (((srcSlot & kEquipSlotMask) != 0 || (tgtSlot & kEquipSlotMask) != 0) &&
				((srcSlot & kEquipSlotMask) != (tgtSlot & kEquipSlotMask))) {
				return false;
			}
		} else { // Similar
			constexpr uint16_t kArmorSlots = SLOTP_HEAD | SLOTP_ARMOR | SLOTP_LEGS | SLOTP_FEET;
			constexpr uint16_t kJewelrySlots = SLOTP_RING | SLOTP_NECKLACE;
			if ((srcSlot & kArmorSlots) != 0 && (tgtSlot & kArmorSlots) == 0) {
				return false;
			}
			if ((srcSlot & kJewelrySlots) != 0 && (tgtSlot & kJewelrySlots) == 0) {
				return false;
			}
		}
	}

	// 9. Server Attributes: Runes & Magic Fields
	const bool srcRune = srcView.group() == ITEM_GROUP_RUNE;
	const bool tgtRune = tgtView.group() == ITEM_GROUP_RUNE;
	if (srcRune != tgtRune) {
		if (mode == SuggestionFilterMode::Exact || srcRune) {
			return false;
		}
	}
	const bool srcField = srcView.group() == ITEM_GROUP_MAGICFIELD || srcView.type() == ITEM_TYPE_MAGICFIELD;
	const bool tgtField = tgtView.group() == ITEM_GROUP_MAGICFIELD || tgtView.type() == ITEM_TYPE_MAGICFIELD;
	if (srcField != tgtField) {
		return false;
	}

	// 10. Semantic Flags (kSemanticFlags)
	const uint64_t srcSemantic = srcView.flagMask() & kSemanticFlags;
	const uint64_t tgtSemantic = tgtView.flagMask() & kSemanticFlags;

	if (mode == SuggestionFilterMode::Exact) {
		if (srcSemantic != tgtSemantic) {
			return false;
		}
		if (srcView.group() != ITEM_GROUP_NONE && tgtView.group() != ITEM_GROUP_NONE) {
			if (srcView.group() != tgtView.group()) {
				return false;
			}
		}
		return true;
	}

	// SuggestionFilterMode::Similar:
	// Target must have at least all semantic flags that source has
	if ((tgtSemantic & srcSemantic) != srcSemantic) {
		return false;
	}

	return true;
}

std::vector<uint16_t> VisualSimilarityService::FindSimilar(uint16_t itemId, size_t count, SuggestionFilterMode filterMode) {
	const uint32_t cacheKey = (static_cast<uint32_t>(itemId) << 2) | static_cast<uint32_t>(filterMode);
	{
		std::lock_guard<std::mutex> lock(dataMutex);
		if (const auto cached = resultCache.find(cacheKey); cached != resultCache.end()) {
			const std::vector<uint16_t>& ranked = cached->second;
			return count < ranked.size() ? std::vector<uint16_t>(ranked.begin(), ranked.begin() + count) : ranked;
		}
	}

	VisualItemData sourceData;
	{
		std::lock_guard<std::mutex> lock(dataMutex);
		auto it = itemDataCache.find(itemId);
		if (it != itemDataCache.end()) {
			sourceData = it->second;
		}
	}

	if (!isIndexed) {
		return {};
	}

	// A miss leaves sourceData empty, and an item with no sprite is never
	// indexed, so the completeness check has to cover the size and the mask.
	if (!sourceData.IsComplete()) {
		sourceData = CalculateData(itemId);
		if (!sourceData.IsComplete()) {
			return {};
		}
	}

	struct ScoredItem {
		uint16_t id;
		double score;
		bool operator<(const ScoredItem& other) const {
			// Sort primary by similarity_score DESC, secondary by ID ASC
			const int64_t s1 = static_cast<int64_t>(score * 100000.0 + 0.5);
			const int64_t s2 = static_cast<int64_t>(other.score * 100000.0 + 0.5);
			if (s1 != s2) {
				return s1 > s2;
			}
			return id < other.id;
		}
	};
	std::vector<ScoredItem> candidates;

	{
		std::lock_guard<std::mutex> lock(dataMutex);
		for (const auto& pair : itemDataCache) {
			const auto& target = pair.second;
			if (target.id == itemId) {
				continue;
			}
			if (!target.IsComplete()) {
				continue;
			}
			if (!MatchesSuggestionFilter(itemId, target.id, filterMode, sourceData, target)) {
				continue;
			}

			double score = 0.0;

			if (sourceData.isOpaque) {
				// Strategy A: aHash for opaque sprites
				// Mappingtool: We only compare against other opaque sprites to avoid matching walls with items
				if (target.isOpaque) {
					int dist = HammingDistance(sourceData.aHash, target.aHash);
					// Score: 1.0 is perfect match (dist 0), 0.0 is max distance (64)
					score = 1.0 - (static_cast<double>(dist) / 64.0);
				} else {
					continue; // Strict 1:1 behavior
				}
			} else {
				// Strategy B: Standard Binary Mask for transparent sprites
				if (target.isOpaque) {
					continue; // Mappingtool: Transparent target does not match opaque candidate
				}

				// Dice can never beat 2 * min(|A|, |B|) / (|A| + |B|), so most
				// candidates are rejected without touching a mask at all.
				const int sourceOnes = sourceData.truePixels;
				const int targetOnes = target.truePixels;
				const int totalOnes = sourceOnes + targetOnes;
				if (totalOnes <= 0) {
					continue;
				}
				if ((2.0 * std::min(sourceOnes, targetOnes)) / totalOnes < kMinimumSimilarityScore) {
					continue;
				}

				int tp = 0;
				if (sourceData.width == target.width && sourceData.height == target.height) {
					tp = IntersectionCount(sourceData.packedMask, target.packedMask);
					if (tp > 0) {
						// Dice: (2.0 * TP) / (|A| + |B|)
						score = (2.0 * tp) / static_cast<double>(totalOnes);
					}
				} else {
					// 1:1 Resize Logic (Nearest Neighbor)
					const int w = std::max(sourceData.width, target.width);
					const int h = std::max(sourceData.height, target.height);
					const auto m1 = ResizePackedMask(sourceData.packedMask, sourceData.width, sourceData.height, w, h);
					const auto m2 = ResizePackedMask(target.packedMask, target.width, target.height, w, h);
					if (m1.empty() || m2.empty() || m1.size() != m2.size()) {
						continue;
					}
					tp = IntersectionCount(m1, m2);
					if (tp > 0) {
						const int resizedOnes = PopCount(m1) + PopCount(m2);
						score = resizedOnes > 0 ? (2.0 * tp) / static_cast<double>(resizedOnes) : 0.0;
					}
				}
			}

			if (score >= kMinimumSimilarityScore) {
				candidates.push_back({ target.id, score });
			}
		}
	}

	if (count > candidates.size()) {
		count = candidates.size();
	}
	std::partial_sort(candidates.begin(), candidates.begin() + count, candidates.end());

	std::vector<uint16_t> results;
	results.reserve(count);
	for (const auto& candidate : candidates | std::views::take(count)) {
		results.push_back(candidate.id);
	}

	// The index is immutable once complete, so a ranking can be reused.
	std::lock_guard<std::mutex> lock(dataMutex);
	if (isIndexed && results.size() <= kMaxCachedResults) {
		resultCache[cacheKey] = results;
	}
	return results;
}
