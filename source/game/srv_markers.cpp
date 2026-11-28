//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "game/srv_markers.h"

#include <algorithm>

namespace rme::srv {

	std::string_view markerKindLabel(MarkerKind kind) {
		switch (kind) {
			case MarkerKind::Start:
				return "Start";
			case MarkerKind::Depot:
				return "Depot";
			case MarkerKind::Raid:
				return "Raid";
			case MarkerKind::Script:
				return "Script";
		}
		return "Script";
	}

	std::optional<MarkerKind> Marker::shownKind(MarkerKindSet shown) const {
		for (size_t i = 0; i < kMarkerKindCount; ++i) {
			const auto candidate = static_cast<MarkerKind>(i);
			if ((kinds & shown & kindBit(candidate)) != 0) {
				return candidate;
			}
		}
		return std::nullopt;
	}

	Marker& ServerMarkers::at(const Position& position) {
		Marker& marker = pending_[position];
		marker.position = position;
		return marker;
	}

	void ServerMarkers::add(const Position& position, MarkerKind kind, std::string text) {
		Marker& marker = at(position);
		const bool known = std::ranges::any_of(marker.lines, [&](const MarkerLine& line) { return line.kind == kind && line.text == text; });
		if (!known) {
			marker.lines.push_back(MarkerLine { .kind = kind, .text = std::move(text) });
		}
	}

	void ServerMarkers::addQuest(const Position& position, int quest, std::string text) {
		Marker& marker = at(position);
		const bool known = std::ranges::any_of(marker.quests, [&](const QuestReference& reference) { return reference.quest == quest && reference.text == text; });
		if (!known) {
			marker.quests.push_back(QuestReference { .quest = quest, .text = std::move(text) });
		}
	}

	void ServerMarkers::addRaidSpread(const Position& position, int spread) {
		Marker& marker = at(position);
		marker.raid_spread = std::max(marker.raid_spread, spread);
	}

	void ServerMarkers::finish() {
		markers_.reserve(markers_.size() + pending_.size());
		for (auto& [position, marker] : pending_) {
			if (marker.lines.empty()) {
				continue;
			}
			std::ranges::stable_sort(marker.lines, {}, &MarkerLine::kind);
			marker.kind = marker.lines.front().kind;
			for (const MarkerLine& line : marker.lines) {
				marker.kinds |= kindBit(line.kind);
			}
			markers_.push_back(std::move(marker));
		}
		pending_.clear();
		std::ranges::sort(markers_, {}, &Marker::position);
	}

	std::span<const Marker> ServerMarkers::onFloor(int z) const {
		const auto first = std::ranges::lower_bound(markers_, z, {}, [](const Marker& marker) { return marker.position.z; });
		const auto last = std::ranges::upper_bound(first, markers_.end(), z, {}, [](const Marker& marker) { return marker.position.z; });
		return { first, last };
	}

	const Marker* ServerMarkers::find(const Position& position) const {
		const auto it = std::ranges::lower_bound(markers_, position, {}, &Marker::position);
		return it != markers_.end() && it->position == position ? &*it : nullptr;
	}

} // namespace rme::srv
