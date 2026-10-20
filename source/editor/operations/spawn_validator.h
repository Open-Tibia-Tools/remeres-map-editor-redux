#ifndef RME_EDITOR_OPERATIONS_SPAWN_VALIDATOR_H_
#define RME_EDITOR_OPERATIONS_SPAWN_VALIDATOR_H_

#include <cstdint>
#include <string>
#include <vector>
#include <span>
#include <string_view>
#include "map/position.h"

class Editor;
class Map;

namespace EditorOperations {

enum class SpawnIssueType : uint8_t {
	InvalidSpawntime = 0,
	UnknownCreature,
	KindMismatch,
	BlockedTile,
	MissingTile,
	EmptyTile,
	UnwalkableZone
};

struct SpawnValidationSettings {
	bool check_invalid_spawntime = true;
	bool check_unknown_creature = true;
	bool check_blocked_tiles = true;
	bool check_missing_or_empty_tiles = true;
	bool check_unwalkable_zone = true;
	int32_t min_spawntime = 1;
	int32_t max_spawntime = 86400;
};

struct SpawnIssue {
	uint32_t id = 0;
	std::string name;
	Position pos;
	Position spawn_center;
	int32_t spawntime = 0;
	bool is_npc = false;
	bool is_alternative = false;
	uint32_t alternative_index = 0;
	SpawnIssueType type = SpawnIssueType::InvalidSpawntime;
	std::string description;

	[[nodiscard]] std::string_view GetTypeString() const noexcept;
	[[nodiscard]] std::string_view GetKindString() const noexcept;
};

class SpawnValidator {
public:
	static std::vector<SpawnIssue> ScanMap(const Map& map, const SpawnValidationSettings& settings);
	static bool RemoveIssue(Editor& editor, const SpawnIssue& issue);
	static size_t RemoveAllIssues(Editor& editor, std::span<const SpawnIssue> issues);
};

} // namespace EditorOperations

#endif // RME_EDITOR_OPERATIONS_SPAWN_VALIDATOR_H_
