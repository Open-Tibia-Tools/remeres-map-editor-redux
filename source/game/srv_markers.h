//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_GAME_SRV_MARKERS_H_
#define RME_GAME_SRV_MARKERS_H_

// Tiles that the server files around a sector map refer to: positions named
// by moveuse.dat rules (levers, depots, quest doors, ...), raid waves and the
// start positions of map.dat. The map shows them so that moving or deleting
// something there does not break a quest without notice. They are read when
// the map is opened and are never saved.

#include "map/position.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rme::srv {

	// In badge priority: a tile shows the first kind it has.
	enum class MarkerKind : uint8_t {
		Start, // map.dat NewbieStart or VeteranStart
		Depot, // a depot chest, or the booth tile that opens it
		Raid, // monsters of a raid wave appear around the tile
		Script, // a moveuse.dat rule triggers on, checks or changes the tile
	};

	inline constexpr size_t kMarkerKindCount = 4;

	// A set of marker kinds, one bit per kind (the View menu shows each kind on its own).
	using MarkerKindSet = uint8_t;

	[[nodiscard]] constexpr MarkerKindSet kindBit(MarkerKind kind) {
		return static_cast<MarkerKindSet>(1u << static_cast<unsigned>(kind));
	}
	inline constexpr MarkerKindSet kAllMarkerKinds = (1u << kMarkerKindCount) - 1;

	[[nodiscard]] std::string_view markerKindLabel(MarkerKind kind);

	struct MarkerLine {
		MarkerKind kind = MarkerKind::Script;
		std::string text;
	};

	// A quest value that a rule on this tile sets or tests.
	struct QuestReference {
		int quest = 0;
		std::string text;
	};

	struct Marker {
		Position position;
		MarkerKind kind = MarkerKind::Script; // the badge's kind
		MarkerKindSet kinds = 0; // the kinds of its lines
		std::vector<MarkerLine> lines; // ordered by kind
		std::vector<QuestReference> quests;
		int raid_spread = 0; // largest spread of the raid waves here

		// The first of its kinds that `shown` holds, in badge priority.
		[[nodiscard]] std::optional<MarkerKind> shownKind(MarkerKindSet shown) const;
	};

	class ServerMarkers {
	public:
		// Adds a tooltip line to the tile's marker; a line it already has is skipped.
		void add(const Position& position, MarkerKind kind, std::string text);
		void addQuest(const Position& position, int quest, std::string text);
		void addRaidSpread(const Position& position, int spread);

		// Orders the markers for drawing; call once after adding them.
		void finish();

		[[nodiscard]] bool empty() const {
			return markers_.empty();
		}
		[[nodiscard]] std::span<const Marker> all() const {
			return markers_;
		}
		// Markers on floor `z`, ordered by y, then x.
		[[nodiscard]] std::span<const Marker> onFloor(int z) const;
		[[nodiscard]] const Marker* find(const Position& position) const;

	private:
		Marker& at(const Position& position);

		std::map<Position, Marker> pending_;
		std::vector<Marker> markers_; // ordered by z, y, x
	};

} // namespace rme::srv

#endif
