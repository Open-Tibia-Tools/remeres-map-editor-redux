#ifndef RME_VISUAL_SIMILARITY_SERVICE_H_
#define RME_VISUAL_SIMILARITY_SERVICE_H_

#include "app/main.h"
#include <wx/event.h>
#include <wx/timer.h>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <cstdint>
#include <atomic>

#include "ui/replace_tool/suggestion_filter_mode.h"

class VisualSimilarityService : public wxEvtHandler {
public:
	static VisualSimilarityService& Get();

	struct VisualItemData {
		uint16_t id = 0;
		bool isOpaque = false;
		uint64_t aHash = 0;
		int width = 0;
		int height = 0;
		// Bit i is set when pixel i (y * width + x) is opaque. Packed so a
		// comparison is a handful of popcounts instead of one vector<bool> proxy
		// access per pixel.
		std::vector<uint64_t> packedMask;
		int truePixels = 0; // Count of 1s

		// Client properties (tibia.dat / GameSprite)
		bool hasLight = false;
		bool isAnimated = false;
		uint16_t drawHeight = 0;
		uint8_t tileWidth = 1;
		uint8_t tileHeight = 1;

		size_t MaskWords() const {
			return (static_cast<size_t>(width) * static_cast<size_t>(height) + 63) / 64;
		}

		/** A usable entry has a size and a mask that matches it. */
		bool IsComplete() const {
			return width > 0 && height > 0 && packedMask.size() == MaskWords();
		}
	};

	// Find top N similar items with specified filter mode
	std::vector<uint16_t> FindSimilar(uint16_t itemId, size_t count = 300, SuggestionFilterMode filterMode = SuggestionFilterMode::None);
	// Start background indexing
	void StartIndexing();

	// Calculate data for a single item (useful for preview/debug)
	VisualItemData CalculateData(uint16_t itemId);

private:
	VisualSimilarityService();
	~VisualSimilarityService();

	void OnTimer(wxTimerEvent& event);

	std::unordered_map<uint16_t, VisualItemData> itemDataCache;
	std::mutex dataMutex;

	// Ranking a source is deterministic once the index is complete, and the
	// editor asks for the same source over and over while a rule is edited.
	static constexpr size_t kMaxCachedResults = 300;
	std::unordered_map<uint32_t, std::vector<uint16_t>> resultCache;

	wxTimer m_timer;
	uint16_t m_nextIdToIndex;
	bool isIndexed;
};

#endif
