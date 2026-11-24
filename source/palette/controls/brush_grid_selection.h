//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_PALETTE_CONTROLS_BRUSH_GRID_SELECTION_H_
#define RME_PALETTE_CONTROLS_BRUSH_GRID_SELECTION_H_

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <unordered_set>
#include <vector>

class Brush;

// Multi-selection of a brush grid, decided without wx. Members are brushes (not indices) so a
// re-sort, filter or reload keeps them. The primary brush is the last committed single pick and
// is what the grid reports as its selected (current) brush; Ctrl/Shift never change it.
class BrushGridSelection {
public:
	enum class Modifier {
		None,
		Toggle, // Ctrl
		Range, // Shift
	};

	// When a plain pick becomes current: right away, or on release (search results jump to their
	// page on release, so a drag or a release elsewhere must not change the primary brush).
	enum class Commit {
		OnPress,
		OnRelease,
	};

	struct PressResult {
		// Brush that became the single pick, or nullptr. With Commit::OnRelease it is not the
		// primary yet; the caller calls Reset(picked) when it commits.
		const Brush* picked = nullptr;
		// The press landed on a selected cell: the selection may be dragged.
		bool can_drag = false;
	};

	// Left button pressed on `display[*index]` (nullopt or out of range: empty space).
	PressResult Press(std::span<Brush* const> display, std::optional<size_t> index, Modifier modifier, Commit commit = Commit::OnPress) {
		pending_collapse_ = nullptr;
		const Brush* hit = At(display, index);
		if (!hit) {
			if (modifier == Modifier::None) {
				Reset(primary_);
			}
			return {};
		}

		switch (modifier) {
			case Modifier::Toggle:
				if (!members_.erase(hit)) {
					members_.insert(hit);
				}
				anchor_ = hit;
				return { .picked = nullptr, .can_drag = Contains(hit) };
			case Modifier::Range:
				SelectRange(display, hit);
				return { .picked = nullptr, .can_drag = true };
			case Modifier::None:
				break;
		}

		if (Contains(hit) && Selected(display).size() > 1) {
			// Keep the group for a drag; a release without a drag collapses to this cell.
			pending_collapse_ = hit;
			return { .picked = nullptr, .can_drag = true };
		}
		if (commit == Commit::OnRelease) {
			members_ = { hit };
			anchor_ = hit;
		} else {
			Reset(hit);
		}
		return { .picked = hit, .can_drag = true };
	}

	// Left button released. Returns the brush picked by collapsing a group, or nullptr.
	const Brush* Release(bool drag_started) {
		const Brush* collapse = pending_collapse_;
		pending_collapse_ = nullptr;
		if (!collapse || drag_started) {
			return nullptr;
		}
		Reset(collapse);
		return collapse;
	}

	// Right button pressed: a cell outside the selection becomes the only member (it does not
	// become the current brush), a cell inside keeps the group for the context menu.
	void PressContext(std::span<Brush* const> display, std::optional<size_t> index) {
		pending_collapse_ = nullptr;
		const Brush* hit = At(display, index);
		if (hit && !Contains(hit)) {
			members_ = { hit };
			anchor_ = hit;
		}
	}

	// Single selection of `brush` (or none), which also becomes the primary brush.
	void Reset(const Brush* brush) {
		members_.clear();
		if (brush) {
			members_.insert(brush);
		}
		primary_ = brush;
		anchor_ = brush;
		pending_collapse_ = nullptr;
	}

	// Drops members that are no longer displayed. The primary brush stays a member even while
	// hidden, so it is highlighted again when a filter shows it.
	void Remap(std::span<Brush* const> display) {
		const std::unordered_set<const Brush*> shown(display.begin(), display.end());
		std::erase_if(members_, [&](const Brush* member) { return member != primary_ && !shown.contains(member); });
		if (!shown.contains(anchor_)) {
			anchor_ = primary_;
		}
		if (pending_collapse_ && !Contains(pending_collapse_)) {
			pending_collapse_ = nullptr;
		}
	}

	[[nodiscard]] bool Contains(const Brush* brush) const {
		return brush && members_.contains(brush);
	}

	[[nodiscard]] const Brush* Primary() const {
		return primary_;
	}

	// Displayed members, in display order.
	[[nodiscard]] std::vector<Brush*> Selected(std::span<Brush* const> display) const {
		std::vector<Brush*> result;
		for (Brush* brush : display) {
			if (members_.contains(brush)) {
				result.push_back(brush);
			}
		}
		return result;
	}

private:
	[[nodiscard]] static const Brush* At(std::span<Brush* const> display, std::optional<size_t> index) {
		return (index && *index < display.size()) ? display[*index] : nullptr;
	}

	void SelectRange(std::span<Brush* const> display, const Brush* hit) {
		const auto hit_it = std::ranges::find(display, hit);
		auto anchor_it = std::ranges::find(display, anchor_);
		if (anchor_it == display.end()) {
			anchor_ = hit;
			anchor_it = hit_it;
		}
		const auto [first, last] = std::minmax(anchor_it, hit_it);
		members_ = std::unordered_set<const Brush*>(first, last + 1);
	}

	std::unordered_set<const Brush*> members_;
	const Brush* primary_ = nullptr;
	const Brush* anchor_ = nullptr;
	const Brush* pending_collapse_ = nullptr;
};

#endif
