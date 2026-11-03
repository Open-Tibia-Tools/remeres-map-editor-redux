//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/main.h"

#include "io/sec/data/server_marker_files.h"

#include "game/creatures.h"
#include "game/srv_markers.h"
#include "io/sec/sec_file_io.h"
#include "io/sec/srv_creature_files.h"
#include "io/sec/srv_moveuse.h"
#include "io/sec/srv_script_lexer.h"
#include "item_definitions/core/item_definition_store.h"
#include "util/file_system.h"

#include <algorithm>
#include <format>
#include <optional>
#include <spdlog/spdlog.h>

namespace rme::sec::server_marker_files {

	namespace {

		// The read-only markers (see game/srv_markers.h): the start positions of
		// map.dat, the tiles dat/moveuse.dat rules name and the raid waves of
		// mon/*.evt.
		class MarkerLoader {
		public:
			MarkerLoader(std::vector<std::string>& warnings, const rme::sec::MapInfo* info, const std::map<int, CreatureType*>& races) :
				warnings_(warnings), info_(info), races_(races) { }

			std::unique_ptr<rme::srv::ServerMarkers> load(const ServerDataFolder& folder) {
				if (info_) {
					addStart(info_->newbie_start, "Newbie start (NewbieStart in map.dat)");
					addStart(info_->veteran_start, "Veteran start (VeteranStart in map.dat)");
				}
				loadRules(folder.dat() / "moveuse.dat");
				loadRaids(folder.mon());
				markers_->finish();
				return std::move(markers_);
			}

		private:
			using MarkerKind = rme::srv::MarkerKind;
			using ScriptCall = rme::sec::ScriptCall;
			using ScriptValue = rme::sec::ScriptValue;

			static std::optional<Position> toPosition(const rme::sec::AbsolutePosition& position) {
				if (position.z < 0 || position.z >= MAP_LAYERS || position.x <= 0 || position.y <= 0 || position.x >= MAP_MAX_WIDTH || position.y >= MAP_MAX_HEIGHT) {
					return std::nullopt;
				}
				return Position(position.x, position.y, position.z);
			}

			void addStart(const std::optional<rme::sec::AbsolutePosition>& start, std::string text) {
				if (const auto position = start ? toPosition(*start) : std::nullopt) {
					markers_->add(*position, MarkerKind::Start, std::move(text));
				}
			}

			std::string depotName(int depot) const {
				if (info_) {
					if (const auto it = std::ranges::find(info_->depots, depot, &rme::sec::MapDepot::id); it != info_->depots.end()) {
						return std::format("{} (depot {})", it->name, depot);
					}
				}
				return std::format("depot {}", depot);
			}

			static const ScriptCall* findCall(const std::vector<ScriptCall>& calls, std::string_view name) {
				const auto it = std::ranges::find_if(calls, [&](const ScriptCall& call) { return rme::srv::equalsIgnoreCase(call.name, name); });
				return it != calls.end() ? &*it : nullptr;
			}

			static std::optional<int64_t> numberAt(const ScriptCall& call, size_t index) {
				if (index < call.arguments.size() && call.arguments[index].kind == ScriptValue::Kind::Number) {
					return call.arguments[index].number;
				}
				return std::nullopt;
			}

			// What a rule does with the position in argument `index` of `call`.
			static std::string_view roleOf(const ScriptCall& call, size_t index) {
				const auto is = [&](std::string_view name) { return rme::srv::equalsIgnoreCase(call.name, name); };
				if (is("IsPosition")) {
					return "triggers here";
				}
				if (is("IsObjectThere") || is("IsPlayerThere") || is("CountObjectsOnMap")) {
					return call.negated ? "needs this tile without it" : "checks this tile";
				}
				if (is("ChangeOnMap") || is("CreateOnMap") || is("DeleteOnMap") || is("DeleteTopOnMap")) {
					return "changes this tile";
				}
				if (is("MoveTopOnMap")) {
					return index == 0 ? "moves things off this tile" : "moves things here";
				}
				if (is("MoveTop") || is("Move")) {
					return "moves things here";
				}
				if (is("MonsterOnMap")) {
					return "summons a monster here";
				}
				if (is("SetStart")) {
					return "sets a player's start here";
				}
				return "names this tile";
			}

			// Calls that name positions the map does not need to keep: effects
			// only show an animation, and "Rel" calls use offsets, not positions.
			static bool skipsPositions(const ScriptCall& call) {
				return rme::srv::equalsIgnoreCase(call.name, "EffectOnMap") || (call.name.size() > 3 && rme::srv::equalsIgnoreCase(std::string_view(call.name).substr(call.name.size() - 3), "Rel"));
			}

			void loadRules(const std::filesystem::path& path) {
				std::string text;
				if (!rme::sec::readFile(path, text)) {
					spdlog::info("IOMapSec: no dat/moveuse.dat, so no script tiles are marked");
					return;
				}
				const auto rules = rme::sec::readMoveUseRules(text);
				if (!rules) {
					warnings_.push_back(std::format("moveuse.dat:{}: {}; the tiles its rules use are not marked.", rules.error().line, rules.error().message));
					return;
				}
				for (const rme::sec::MoveUseRule& rule : *rules) {
					addRule(rule);
				}
			}

			void addRule(const rme::sec::MoveUseRule& rule) {
				// "Use lever": the event and the object type its IsType condition names.
				std::string subject = rule.event;
				if (const ScriptCall* is_type = findCall(rule.conditions, "IsType")) {
					if (const auto type = numberAt(*is_type, 1)) {
						const auto definition = g_item_definitions.get(static_cast<uint16_t>(*type));
						const std::string_view name = definition ? definition.name() : std::string_view {};
						subject += name.empty() ? std::format(" object {}", *type) : std::format(" {}", name);
					}
				}
				const std::string context = rule.name.empty() ? rule.section : std::format("\"{}\"", rule.name);

				std::vector<std::pair<int, std::string>> quests;
				for (const ScriptCall& condition : rule.conditions) {
					// HasQuestValue (Obj2,3,= ,0)
					if (rme::srv::equalsIgnoreCase(condition.name, "HasQuestValue") && condition.arguments.size() >= 4) {
						if (const auto quest = numberAt(condition, 1)) {
							const auto value = numberAt(condition, 3).value_or(0);
							quests.emplace_back(static_cast<int>(*quest), std::format("{}needs quest {} {} {}", condition.negated ? "not " : "", *quest, condition.arguments[2].text, value));
						}
					}
				}
				for (const ScriptCall& action : rule.actions) {
					// SetQuestValue(Obj2,3,1)
					if (rme::srv::equalsIgnoreCase(action.name, "SetQuestValue")) {
						if (const auto quest = numberAt(action, 1)) {
							quests.emplace_back(static_cast<int>(*quest), std::format("sets quest {} to {}", *quest, numberAt(action, 2).value_or(0)));
						}
					}
				}
				std::string quest_text;
				for (const auto& [quest, description] : quests) {
					quest_text += "; " + description;
				}

				// LoadDepot([chest],type,Obj2,depot) and SaveDepot: a depot booth.
				const ScriptCall* depot = findCall(rule.actions, "LoadDepot");
				if (!depot) {
					depot = findCall(rule.actions, "SaveDepot");
				}
				const std::optional<int64_t> depot_id = depot ? numberAt(*depot, 3) : std::nullopt;

				for (const auto* calls : { &rule.conditions, &rule.actions }) {
					for (const ScriptCall& call : *calls) {
						if (skipsPositions(call)) {
							continue;
						}
						for (size_t index = 0; index < call.arguments.size(); ++index) {
							if (call.arguments[index].kind != ScriptValue::Kind::Coordinate) {
								continue;
							}
							const auto position = toPosition(call.arguments[index].position);
							if (!position) {
								continue;
							}
							if (depot_id) {
								const bool chest = rme::srv::equalsIgnoreCase(call.name, "LoadDepot") || rme::srv::equalsIgnoreCase(call.name, "SaveDepot");
								markers_->add(*position, MarkerKind::Depot, std::format("{} {}", chest ? "Depot chest of" : "Opens the depot chest of", depotName(static_cast<int>(*depot_id))));
								continue;
							}
							markers_->add(*position, MarkerKind::Script, std::format("{}: {}{} ({})", subject, roleOf(call, index), quest_text, context));
							for (const auto& [quest, description] : quests) {
								markers_->addQuest(*position, quest, std::format("{}: {} ({})", subject, description, context));
							}
						}
					}
				}
			}

			std::string raceName(int race) const {
				const auto it = races_.find(race);
				return it != races_.end() && it->second ? it->second->name : std::format("race {}", race);
			}

			void loadRaids(const std::filesystem::path& folder) {
				forEachServerFile(folder, ".evt", warnings_, [&](const std::filesystem::path& path, std::string_view text) {
					const auto raid = rme::sec::readRaid(text);
					if (!raid) {
						warnings_.push_back(std::format("{}:{}: {}; raid skipped.", FileSystem::PathToUtf8(path.filename()), raid.error().line, raid.error().message));
						return;
					}
					addRaid(FileSystem::PathToUtf8(path.stem()), *raid);
				});
			}

			// One line per raid and position: its waves there, how many monsters
			// they bring and which.
			void addRaid(const std::string& name, const rme::sec::Raid& raid) {
				struct Spot {
					size_t waves = 0;
					int min_count = 0;
					int max_count = 0;
					int spread = 0;
					std::vector<std::string> races;
				};
				std::map<Position, Spot> spots;
				for (const rme::sec::RaidWave& wave : raid.waves) {
					const auto position = toPosition(wave.position);
					if (!position) {
						continue;
					}
					Spot& spot = spots[*position];
					++spot.waves;
					spot.min_count += wave.min_count;
					spot.max_count += wave.max_count;
					spot.spread = std::max(spot.spread, wave.spread);
					std::string race = raceName(wave.race);
					if (std::ranges::find(spot.races, race) == spot.races.end()) {
						spot.races.push_back(std::move(race));
					}
				}

				constexpr int64_t kSecondsPerDay = 24 * 60 * 60;
				const std::string when = raid.interval > 0 ? std::format("about every {} days", std::max<int64_t>(1, raid.interval / kSecondsPerDay)) : "on a set date";
				for (const auto& [position, spot] : spots) {
					std::string races;
					for (const std::string& race : spot.races) {
						races += (races.empty() ? "" : ", ") + race;
					}
					markers_->add(position, MarkerKind::Raid, std::format("{}: {} {}, {}-{} {} within {} ({}, {})", name, spot.waves, spot.waves == 1 ? "wave" : "waves", spot.min_count, spot.max_count, races, spot.spread, raid.type, when));
					markers_->addRaidSpread(position, spot.spread);
				}
			}

			std::vector<std::string>& warnings_;
			const rme::sec::MapInfo* info_;
			const std::map<int, CreatureType*>& races_;
			std::unique_ptr<rme::srv::ServerMarkers> markers_ = std::make_unique<rme::srv::ServerMarkers>();
		};

	} // namespace

	std::unique_ptr<rme::srv::ServerMarkers> load(const ServerDataFolder& folder, const MapInfo* info, const std::map<int, CreatureType*>& races, std::vector<std::string>& warnings) {
		return MarkerLoader(warnings, info, races).load(folder);
	}

} // namespace rme::sec::server_marker_files
