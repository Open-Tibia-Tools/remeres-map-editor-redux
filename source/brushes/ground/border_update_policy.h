//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_BRUSHES_GROUND_BORDER_UPDATE_POLICY_H_
#define RME_BRUSHES_GROUND_BORDER_UPDATE_POLICY_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>
#include <vector>

class GroundBrush;

// Decide side of ground border recalculation (issue #1108). Everything here is
// free of wxWidgets, globals and map types so it can be unit-tested standalone;
// GroundBorderCalculator is the Apply side.
//
// Item access goes through a Traits type with static members:
//   isBorder(const T&), isBorderLayer(const T&), id(const T&), setId(T&, uint32_t),
//   borderGroup(const T&), borderAlignment(const T&)
namespace BorderUpdate {

	// Which existing border items a recalculation may remove or rewrite.
	enum class Scope : uint8_t {
		AllBrushes, // Every brush border (Item::isBorder) on the tile. Classic behaviour.
		CurrentBrushOnly, // Only border items owned by the active ground brush.
	};

	// Where newly generated border items go inside the tile's border layer.
	enum class Placement : uint8_t {
		Bottom, // Beneath existing border-layer items. Classic behaviour.
		Top, // Above existing border-layer items, below every non-border item.
	};

	// Persisted encoding of Config::AUTOBORDER_PLACEMENT.
	[[nodiscard]] constexpr Placement PlacementFromSetting(int value) noexcept {
		return value == 1 ? Placement::Top : Placement::Bottom;
	}

	[[nodiscard]] constexpr int PlacementToSetting(Placement placement) noexcept {
		return placement == Placement::Top ? 1 : 0;
	}

	struct Options {
		Scope scope = Scope::AllBrushes;
		Placement placement = Placement::Bottom;
		const GroundBrush* active_brush = nullptr;

		// CurrentBrushOnly needs a brush to scope to; without one it means AllBrushes.
		[[nodiscard]] constexpr bool preservesOtherBrushes() const noexcept {
			return scope == Scope::CurrentBrushOnly && active_brush != nullptr;
		}

		// True when the recalculation must behave exactly like the classic one.
		[[nodiscard]] constexpr bool isClassic() const noexcept {
			return !preservesOtherBrushes() && placement == Placement::Bottom;
		}

		// The optional (mountain) border flag is tile state shared by every brush
		// whose optional border reaches the tile, so a scoped recalculation must
		// never clear it.
		[[nodiscard]] constexpr bool mayClearOptionalBorder() const noexcept {
			return !preservesOtherBrushes();
		}
	};

	// Sorted set of the border item ids a ground brush owns.
	class ItemIdSet {
	public:
		void insert(uint32_t id) {
			if (id == 0) {
				return;
			}
			const auto it = std::ranges::lower_bound(ids, id);
			if (it == ids.end() || *it != id) {
				ids.insert(it, id);
			}
		}

		template <typename Range>
		void insertAll(const Range& range) {
			for (const auto id : range) {
				insert(static_cast<uint32_t>(id));
			}
		}

		void clear() noexcept {
			ids.clear();
		}

		[[nodiscard]] bool contains(uint32_t id) const {
			return std::ranges::binary_search(ids, id);
		}

		[[nodiscard]] std::size_t size() const noexcept {
			return ids.size();
		}

		// Sorted ascending.
		[[nodiscard]] std::span<const uint32_t> values() const noexcept {
			return ids;
		}

	private:
		std::vector<uint32_t> ids;
	};

	// Result of a specific case: `result_id` replaced `replaced_id` while all of
	// `sources` were present, so it stands for those pieces. A replaced piece can
	// belong to another brush (snow fusing its edge with a sea edge), which makes
	// the result a joint border of both brushes.
	struct FusedBorder {
		uint32_t result_id = 0;
		uint32_t replaced_id = 0;
		std::vector<uint32_t> sources;
	};

	// Border items a ground brush owns.
	struct BorderOwnership {
		ItemIdSet items;
		std::vector<FusedBorder> fused;

		void clear() {
			items.clear();
			fused.clear();
		}

		[[nodiscard]] bool contains(uint32_t id) const {
			return items.contains(id);
		}

		// True when `result_id` is one of this brush's specific-case results that
		// stands for `source_id`.
		[[nodiscard]] bool fuses(uint32_t result_id, uint32_t source_id) const {
			return std::ranges::any_of(fused, [&](const FusedBorder& entry) {
				return entry.result_id == result_id && (entry.replaced_id == source_id || std::ranges::find(entry.sources, source_id) != entry.sources.end());
			});
		}
	};

	// CurrentBrushOnly removal rule. An existing border item may be removed or
	// rewritten by the active brush when the active brush owns its id and no
	// other ground brush present in the tile's context (its own ground and its
	// 8 neighbours, i.e. every brush that can justify a border on that tile) owns
	// the same id. Item ids shared with such a brush cannot be attributed, so
	// they are preserved instead of risking another brush's only visual border.
	// A specific-case result whose replaced piece or any matched source is owned
	// by a context brush is a joint border and is preserved for the same reason.
	enum class RewriteVerdict : uint8_t {
		Rewritable,
		NotOwnedByActive, // The active brush never generates this id.
		SharedWithContext, // A context brush owns the same id.
		JointBorder, // A specific-case result fused with a context brush's piece.
	};

	// The removal rule with its reason; IsRewritableByActive is its yes/no form.
	[[nodiscard]] inline RewriteVerdict ExplainRewrite(uint32_t id, const BorderOwnership& active, std::span<const BorderOwnership* const> context) {
		if (!active.contains(id)) {
			return RewriteVerdict::NotOwnedByActive;
		}
		const auto owned_by_context = [&context](uint32_t item_id) {
			return std::ranges::any_of(context, [item_id](const BorderOwnership* other) {
				return other && other->contains(item_id);
			});
		};
		if (owned_by_context(id)) {
			return RewriteVerdict::SharedWithContext;
		}
		const bool joint = std::ranges::any_of(active.fused, [&](const FusedBorder& entry) {
			return entry.result_id == id && (owned_by_context(entry.replaced_id) || std::ranges::any_of(entry.sources, owned_by_context));
		});
		return joint ? RewriteVerdict::JointBorder : RewriteVerdict::Rewritable;
	}

	[[nodiscard]] inline bool IsRewritableByActive(uint32_t id, const BorderOwnership& active, std::span<const BorderOwnership* const> context) {
		return ExplainRewrite(id, active, context) == RewriteVerdict::Rewritable;
	}

	// True when a generated piece `id` is already shown on the tile: as a brush
	// border with the same id, or through a specific-case result of one of
	// `owners` (the active brush and its context) that fused it.
	template <typename Traits, typename T>
	[[nodiscard]] bool IsRepresentedOnTile(uint32_t id, const std::vector<T>& items, std::span<const BorderOwnership* const> owners) {
		return std::ranges::any_of(items, [&](const T& item) {
			if (!Traits::isBorder(item)) {
				return false;
			}
			const uint32_t item_id = Traits::id(item);
			if (item_id == id) {
				return true;
			}
			return std::ranges::any_of(owners, [&](const BorderOwnership* owner) {
				return owner && owner->fuses(item_id, id);
			});
		});
	}

	// Removes every brush border item whose id `can_rewrite` accepts, keeping the
	// relative order of all remaining items.
	template <typename Traits, typename T, typename CanRewrite>
	void RemoveRewritableBorders(std::vector<T>& items, CanRewrite&& can_rewrite) {
		std::erase_if(items, [&](const T& item) {
			return Traits::isBorder(item) && can_rewrite(Traits::id(item));
		});
	}

	// Index at which a generated border set is spliced into `items`.
	// Bottom: the front of the tile, as the classic front insertion does.
	// Top: the end of the leading border-layer run, i.e. after the last border-layer
	// item and before the first non-border item. Border-layer items are always on
	// bottom with top order 1, so they lead the item vector.
	template <typename Traits, typename T>
	[[nodiscard]] std::size_t InsertionIndex(const std::vector<T>& items, Placement placement) {
		if (placement == Placement::Bottom) {
			return 0;
		}
		const auto it = std::ranges::find_if_not(items, [](const T& item) { return Traits::isBorderLayer(item); });
		return static_cast<std::size_t>(std::distance(items.begin(), it));
	}

	// Splices `generated` (already in final stack order) into `items` at the
	// placement boundary as one block, so the set keeps its internal order.
	// A generated item whose id `skip` accepts is dropped first; `skip` sees the
	// tile before the splice.
	template <typename Traits, typename T, typename Skip>
	void InsertGenerated(std::vector<T>& items, std::vector<T>& generated, Placement placement, Skip&& skip) {
		std::erase_if(generated, [&skip](const T& candidate) {
			return skip(Traits::id(candidate));
		});
		const std::size_t index = InsertionIndex<Traits>(items, placement);
		items.insert(items.begin() + static_cast<std::ptrdiff_t>(index), std::make_move_iterator(generated.begin()), std::make_move_iterator(generated.end()));
		generated.clear();
	}

	enum class SpecificCaseOutcome : uint8_t {
		Applied, // Every condition matched and the case ran.
		NotMatched, // Fewer matches than conditions; the tile is unchanged.
		Protected, // A matched item may not be rewritten; the tile is unchanged.
	};

	// Applies one specific case block (borders.xml <specific>) to `items`.
	// Matching reads every brush border in the leading border-layer run, whoever
	// owns it. The case is applied whole or not at all: when any matched item is
	// refused by `can_rewrite`, nothing is replaced or deleted, so a case can
	// neither fuse another brush's piece nor drop the active brush's piece that
	// the fused result would have stood for.
	template <typename Traits, typename T, typename Block, typename CanRewrite>
	SpecificCaseOutcome ApplySpecificCase(std::vector<T>& items, const Block& block, CanRewrite&& can_rewrite) {
		std::size_t matches = 0;
		for (const T& item : items) {
			if (!Traits::isBorderLayer(item)) {
				break;
			}
			if (!Traits::isBorder(item)) {
				continue;
			}

			if (block.match_group > 0) {
				if (Traits::borderGroup(item) == block.match_group && Traits::borderAlignment(item) == block.group_match_alignment) {
					++matches;
					continue;
				}
			}

			for (const auto match_id : block.items_to_match) {
				if (Traits::id(item) == match_id) {
					++matches;
				}
			}
		}

		if (matches < block.items_to_match.size()) {
			return SpecificCaseOutcome::NotMatched;
		}

		const bool touches_protected_item = std::ranges::any_of(items, [&](const T& item) {
			return Traits::isBorder(item) && !can_rewrite(Traits::id(item)) && std::ranges::find(block.items_to_match, Traits::id(item)) != block.items_to_match.end();
		});
		if (touches_protected_item) {
			return SpecificCaseOutcome::Protected;
		}

		// In delete_all mode the border counts as replaced already.
		bool replaced = block.delete_all;
		auto it = items.begin();
		while (it != items.end()) {
			if (!Traits::isBorder(*it)) {
				++it;
				continue;
			}

			bool erased = false;
			for (const auto match_id : block.items_to_match) {
				if (Traits::id(*it) != match_id) {
					continue;
				}
				if (!replaced && Traits::id(*it) == block.to_replace_id) {
					// Replace the matching border, delete everything else.
					Traits::setId(*it, block.with_id);
					replaced = true;
				} else if (block.delete_all || !block.keepBorder) {
					it = items.erase(it);
					erased = true;
					break;
				}
			}

			if (!erased) {
				++it;
			}
		}
		return SpecificCaseOutcome::Applied;
	}

} // namespace BorderUpdate

#endif // RME_BRUSHES_GROUND_BORDER_UPDATE_POLICY_H_
