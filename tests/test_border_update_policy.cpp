#include "brushes/ground/border_update_policy.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

// The policy header only forward-declares GroundBrush; Options stores a pointer to it.
class GroundBrush { };

namespace {

	using BorderUpdate::BorderOwnership;
	using BorderUpdate::ItemIdSet;
	using BorderUpdate::Options;
	using BorderUpdate::Placement;
	using BorderUpdate::Scope;

	struct FakeItem {
		uint32_t id = 0;
		bool brush_border = false; // Item::isBorder
		bool clip = false; // border layer without being a brush border
		uint32_t group = 0;
		int alignment = 0;
	};

	using ItemPtr = std::unique_ptr<FakeItem>;

	struct FakeTraits {
		static bool isBorder(const ItemPtr& item) {
			return item->brush_border;
		}
		static bool isBorderLayer(const ItemPtr& item) {
			return item->brush_border || item->clip;
		}
		static uint32_t id(const ItemPtr& item) {
			return item->id;
		}
		static void setId(ItemPtr& item, uint32_t id) {
			item->id = id;
		}
		static uint32_t borderGroup(const ItemPtr& item) {
			return item->group;
		}
		static int borderAlignment(const ItemPtr& item) {
			return item->alignment;
		}
	};

	struct FakeSpecificCase {
		std::vector<uint16_t> items_to_match;
		uint32_t match_group = 0;
		int group_match_alignment = 0;
		uint16_t to_replace_id = 0;
		uint16_t with_id = 0;
		bool delete_all = false;
		bool keepBorder = false;
	};

	ItemPtr border(uint32_t id) {
		return std::make_unique<FakeItem>(FakeItem { .id = id, .brush_border = true });
	}

	ItemPtr clip(uint32_t id) {
		return std::make_unique<FakeItem>(FakeItem { .id = id, .clip = true });
	}

	ItemPtr normal(uint32_t id) {
		return std::make_unique<FakeItem>(FakeItem { .id = id });
	}

	std::vector<uint32_t> ids(const std::vector<ItemPtr>& items) {
		std::vector<uint32_t> result;
		for (const auto& item : items) {
			result.push_back(item->id);
		}
		return result;
	}

	template <typename... Items>
	std::vector<ItemPtr> tile(Items... items) {
		std::vector<ItemPtr> result;
		(result.push_back(std::move(items)), ...);
		return result;
	}

	// Brush A ("grass"): inline border 101-104, a <border id> shared with B (500),
	// an optional border (900) and a specific case result (701) of its own pieces.
	// Brush B ("stone"): inline border 201-202 and the shared border (500).
	struct Brushes {
		BorderOwnership a;
		BorderOwnership b;

		Brushes() {
			const std::array<uint32_t, 13> a_inline { 101, 102, 0, 103, 0, 0, 104, 0, 0, 0, 0, 0, 0 };
			const std::array<uint32_t, 13> b_inline { 0, 201, 0, 0, 202, 0, 0, 0, 0, 0, 0, 0, 0 };
			const std::array<uint32_t, 13> shared { 500, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
			const std::array<uint32_t, 13> optional { 0, 0, 900, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

			a.items.insertAll(a_inline);
			a.items.insertAll(shared);
			a.items.insertAll(optional);
			a.items.insert(701);
			a.fused.push_back({ .result_id = 701, .replaced_id = 101, .sources = { 101, 102 } });

			b.items.insertAll(b_inline);
			b.items.insertAll(shared);
		}
	};

	// borders.xml pattern (snow vs. sea): the snow outer border's specific case
	// fuses its south edge (61) with the sea's east edge (52) into 6652.
	struct SnowAndSea {
		BorderOwnership snow;
		BorderOwnership sea;

		SnowAndSea() {
			snow.items.insertAll(std::array<uint32_t, 2> { 61, 62 });
			snow.items.insert(6652);
			snow.fused.push_back({ .result_id = 6652, .replaced_id = 52, .sources = { 61, 52 } });

			sea.items.insertAll(std::array<uint32_t, 2> { 51, 52 });
		}
	};

} // namespace

TEST_CASE("placement setting encoding round-trips and defaults to bottom", "[border_update]") {
	REQUIRE(BorderUpdate::PlacementFromSetting(0) == Placement::Bottom);
	REQUIRE(BorderUpdate::PlacementFromSetting(1) == Placement::Top);
	REQUIRE(BorderUpdate::PlacementFromSetting(-1) == Placement::Bottom);
	REQUIRE(BorderUpdate::PlacementFromSetting(7) == Placement::Bottom);
	REQUIRE(BorderUpdate::PlacementToSetting(Placement::Bottom) == 0);
	REQUIRE(BorderUpdate::PlacementToSetting(Placement::Top) == 1);
}

TEST_CASE("default options select the classic border update", "[border_update]") {
	const Options defaults;
	REQUIRE(defaults.isClassic());
	REQUIRE_FALSE(defaults.preservesOtherBrushes());
	REQUIRE(defaults.mayClearOptionalBorder());

	// Without an active brush the scope cannot be applied and falls back to AllBrushes.
	const Options scoped_without_brush { .scope = Scope::CurrentBrushOnly };
	REQUIRE(scoped_without_brush.isClassic());

	const Options top { .placement = Placement::Top };
	REQUIRE_FALSE(top.isClassic());
	REQUIRE_FALSE(top.preservesOtherBrushes());
	REQUIRE(top.mayClearOptionalBorder());
}

TEST_CASE("a scoped update never clears the shared optional border flag", "[border_update]") {
	const GroundBrush active;
	const Options scoped { .scope = Scope::CurrentBrushOnly, .active_brush = &active };
	REQUIRE(scoped.preservesOtherBrushes());
	REQUIRE_FALSE(scoped.isClassic());
	REQUIRE_FALSE(scoped.mayClearOptionalBorder());
}

TEST_CASE("ownership covers inline, shared, optional and specific-case items", "[border_update]") {
	const Brushes brushes;

	REQUIRE(brushes.a.items.size() == 7);
	REQUIRE(brushes.a.contains(101));
	REQUIRE(brushes.a.contains(104));
	REQUIRE(brushes.a.contains(500));
	REQUIRE(brushes.a.contains(900));
	REQUIRE(brushes.a.contains(701));
	REQUIRE_FALSE(brushes.a.contains(0));
	REQUIRE_FALSE(brushes.a.contains(201));

	REQUIRE(brushes.b.items.size() == 3);
	REQUIRE(brushes.b.contains(500));
	REQUIRE_FALSE(brushes.b.contains(101));

	ItemIdSet duplicate;
	duplicate.insert(5);
	duplicate.insert(5);
	duplicate.insert(0);
	REQUIRE(duplicate.size() == 1);

	REQUIRE(brushes.a.fuses(701, 101));
	REQUIRE(brushes.a.fuses(701, 102));
	REQUIRE_FALSE(brushes.a.fuses(701, 103));
	REQUIRE_FALSE(brushes.a.fuses(101, 101));
}

TEST_CASE("rewrite rule: exclusive ids are the active brush's, shared ids follow the context", "[border_update]") {
	const Brushes brushes;
	const std::array<const BorderOwnership*, 1> b_in_context { &brushes.b };
	const std::array<const BorderOwnership*, 2> null_and_b { nullptr, &brushes.b };

	REQUIRE(BorderUpdate::IsRewritableByActive(101, brushes.a, {}));
	REQUIRE(BorderUpdate::IsRewritableByActive(101, brushes.a, b_in_context));
	// Never another brush's exclusive item.
	REQUIRE_FALSE(BorderUpdate::IsRewritableByActive(201, brushes.a, {}));
	REQUIRE_FALSE(BorderUpdate::IsRewritableByActive(201, brushes.a, b_in_context));
	// Shared id: preserved while the co-owner is present around the tile.
	REQUIRE_FALSE(BorderUpdate::IsRewritableByActive(500, brushes.a, b_in_context));
	REQUIRE_FALSE(BorderUpdate::IsRewritableByActive(500, brushes.a, null_and_b));
	REQUIRE(BorderUpdate::IsRewritableByActive(500, brushes.a, {}));
	// A result fused from the brush's own pieces is its own.
	REQUIRE(BorderUpdate::IsRewritableByActive(701, brushes.a, b_in_context));
}

TEST_CASE("a specific-case result fused with another brush's piece is a joint border", "[border_update]") {
	const SnowAndSea brushes;
	const std::array<const BorderOwnership*, 1> sea_in_context { &brushes.sea };

	REQUIRE_FALSE(BorderUpdate::IsRewritableByActive(6652, brushes.snow, sea_in_context));
	// Without the sea around the tile the fused piece is a stale snow border.
	REQUIRE(BorderUpdate::IsRewritableByActive(6652, brushes.snow, {}));
	// The sea never rewrites it.
	REQUIRE_FALSE(BorderUpdate::IsRewritableByActive(6652, brushes.sea, {}));
}

TEST_CASE("a fused result that only matched another brush's piece is a joint border too", "[border_update]") {
	// The specific case replaces the snow's own edge (61) and matches the sea's edge (52) as a
	// condition, so the sea piece is only a source, not the replaced item.
	BorderOwnership snow;
	snow.items.insertAll(std::array<uint32_t, 2> { 61, 62 });
	snow.items.insert(6652);
	snow.fused.push_back({ .result_id = 6652, .replaced_id = 61, .sources = { 52 } });
	BorderOwnership sea;
	sea.items.insertAll(std::array<uint32_t, 2> { 51, 52 });
	const std::array<const BorderOwnership*, 1> sea_in_context { &sea };

	REQUIRE_FALSE(BorderUpdate::IsRewritableByActive(6652, snow, sea_in_context));
	REQUIRE(BorderUpdate::IsRewritableByActive(6652, snow, {}));
}

TEST_CASE("current brush only removes the active brush's items and keeps the rest in place", "[border_update]") {
	const Brushes brushes;
	const std::array<const BorderOwnership*, 1> context { &brushes.b };
	const auto can_rewrite = [&](uint32_t id) { return BorderUpdate::IsRewritableByActive(id, brushes.a, context); };

	auto items = tile(border(101), border(201), clip(7000), border(500), border(103), border(202), normal(3000));
	BorderUpdate::RemoveRewritableBorders<FakeTraits>(items, can_rewrite);

	REQUIRE(ids(items) == std::vector<uint32_t> { 201, 7000, 500, 202, 3000 });
}

TEST_CASE("all brushes removes every brush border but keeps hand-placed clip items", "[border_update]") {
	auto items = tile(border(101), border(201), clip(7000), border(500), normal(3000));
	BorderUpdate::RemoveRewritableBorders<FakeTraits>(items, [](uint32_t) { return true; });

	REQUIRE(ids(items) == std::vector<uint32_t> { 7000, 3000 });
}

TEST_CASE("bottom placement reproduces the classic front insertion", "[border_update]") {
	const std::vector<uint32_t> generation_order { 101, 102, 103 };

	// Classic: every piece is inserted at items.begin() as it is generated.
	auto classic = tile(clip(7000), normal(3000));
	for (const uint32_t id : generation_order) {
		classic.insert(classic.begin(), border(id));
	}

	// Policy path: collect, reverse into stack order, splice once.
	auto spliced = tile(clip(7000), normal(3000));
	std::vector<ItemPtr> generated;
	for (const uint32_t id : generation_order) {
		generated.push_back(border(id));
	}
	std::ranges::reverse(generated);
	BorderUpdate::InsertGenerated<FakeTraits>(spliced, generated, Placement::Bottom, [](uint32_t) { return false; });

	REQUIRE(ids(spliced) == ids(classic));
	REQUIRE(ids(spliced) == std::vector<uint32_t> { 103, 102, 101, 7000, 3000 });
	REQUIRE(generated.empty());
}

TEST_CASE("top placement goes above the border layer and below normal items", "[border_update]") {
	auto items = tile(border(201), clip(7000), border(202), normal(3000), normal(3001));
	REQUIRE(BorderUpdate::InsertionIndex<FakeTraits>(items, Placement::Top) == 3);
	REQUIRE(BorderUpdate::InsertionIndex<FakeTraits>(items, Placement::Bottom) == 0);

	auto generated = tile(border(103), border(102), border(101));
	BorderUpdate::InsertGenerated<FakeTraits>(items, generated, Placement::Top, [](uint32_t) { return false; });

	// The generated set keeps its internal order as one block.
	REQUIRE(ids(items) == std::vector<uint32_t> { 201, 7000, 202, 103, 102, 101, 3000, 3001 });
}

TEST_CASE("top placement on tiles without a border layer", "[border_update]") {
	auto only_normal = tile(normal(3000));
	auto generated = tile(border(101));
	BorderUpdate::InsertGenerated<FakeTraits>(only_normal, generated, Placement::Top, [](uint32_t) { return false; });
	REQUIRE(ids(only_normal) == std::vector<uint32_t> { 101, 3000 });

	std::vector<ItemPtr> empty;
	auto more = tile(border(101), border(102));
	BorderUpdate::InsertGenerated<FakeTraits>(empty, more, Placement::Top, [](uint32_t) { return false; });
	REQUIRE(ids(empty) == std::vector<uint32_t> { 101, 102 });
}

TEST_CASE("generated ids already shown by preserved borders are not stacked twice", "[border_update]") {
	const SnowAndSea brushes;
	const std::array<const BorderOwnership*, 2> owners { &brushes.snow, &brushes.sea };

	auto items = tile(border(500), clip(101), border(6652), normal(3000));
	const auto represented = [&](uint32_t id) { return BorderUpdate::IsRepresentedOnTile<FakeTraits>(id, items, owners); };
	REQUIRE(represented(500));
	REQUIRE_FALSE(represented(101));
	// 6652 is the fused snow/sea piece: it already shows both 61 and 52.
	REQUIRE(represented(61));
	REQUIRE(represented(52));
	REQUIRE_FALSE(represented(62));

	auto generated = tile(border(500), border(101), border(61), border(52), border(102));
	BorderUpdate::InsertGenerated<FakeTraits>(items, generated, Placement::Bottom, represented);

	// 101 only exists as a clip item, so it is still added.
	REQUIRE(ids(items) == std::vector<uint32_t> { 101, 102, 500, 101, 6652, 3000 });
}

TEST_CASE("specific cases never mutate another brush's items", "[border_update]") {
	const Brushes brushes;
	const std::array<const BorderOwnership*, 1> context { &brushes.b };
	const auto can_rewrite = [&](uint32_t id) { return BorderUpdate::IsRewritableByActive(id, brushes.a, context); };

	SECTION("a delete_all case touching another brush's item is skipped whole") {
		auto items = tile(border(101), border(201), border(102), normal(3000));
		const FakeSpecificCase block { .items_to_match = { 101, 201 }, .delete_all = true };
		BorderUpdate::ApplySpecificCase<FakeTraits>(items, block, can_rewrite);
		REQUIRE(ids(items) == std::vector<uint32_t> { 101, 201, 102, 3000 });
	}

	SECTION("replacing another brush's item is refused and the active piece is kept") {
		auto items = tile(border(201), border(101), normal(3000));
		const FakeSpecificCase block { .items_to_match = { 201, 101 }, .to_replace_id = 201, .with_id = 701 };
		BorderUpdate::ApplySpecificCase<FakeTraits>(items, block, can_rewrite);
		REQUIRE(ids(items) == std::vector<uint32_t> { 201, 101, 3000 });
	}

	SECTION("shared items next to their co-owner block the case") {
		auto items = tile(border(500), border(101), normal(3000));
		const FakeSpecificCase block { .items_to_match = { 500, 101 } };
		BorderUpdate::ApplySpecificCase<FakeTraits>(items, block, can_rewrite);
		REQUIRE(ids(items) == std::vector<uint32_t> { 500, 101, 3000 });
	}

	SECTION("a case on the active brush's own pieces still applies") {
		auto items = tile(border(201), border(101), border(102), normal(3000));
		const FakeSpecificCase block { .items_to_match = { 101, 102 }, .to_replace_id = 101, .with_id = 701 };
		BorderUpdate::ApplySpecificCase<FakeTraits>(items, block, can_rewrite);
		REQUIRE(ids(items) == std::vector<uint32_t> { 201, 701, 3000 });
	}
}

TEST_CASE("snow/sea fusion survives a scoped update of either brush", "[border_update]") {
	const SnowAndSea brushes;
	const FakeSpecificCase fuse { .items_to_match = { 61, 52 }, .to_replace_id = 52, .with_id = 6652 };

	SECTION("scoped snow keeps the fused piece and does not add its own edge again") {
		const std::array<const BorderOwnership*, 2> owners { &brushes.snow, &brushes.sea };
		const std::span<const BorderOwnership* const> context = std::span(owners).subspan(1);
		const auto can_rewrite = [&](uint32_t id) { return BorderUpdate::IsRewritableByActive(id, brushes.snow, context); };

		auto items = tile(border(6652), normal(3000));
		BorderUpdate::RemoveRewritableBorders<FakeTraits>(items, can_rewrite);
		auto generated = tile(border(61));
		BorderUpdate::InsertGenerated<FakeTraits>(items, generated, Placement::Bottom, [&](uint32_t id) {
			return BorderUpdate::IsRepresentedOnTile<FakeTraits>(id, items, owners);
		});
		BorderUpdate::ApplySpecificCase<FakeTraits>(items, fuse, can_rewrite);
		REQUIRE(ids(items) == std::vector<uint32_t> { 6652, 3000 });
	}

	SECTION("scoped snow does not fuse a separate sea edge or drop its own edge") {
		const std::array<const BorderOwnership*, 1> context { &brushes.sea };
		const auto can_rewrite = [&](uint32_t id) { return BorderUpdate::IsRewritableByActive(id, brushes.snow, context); };

		auto items = tile(border(61), border(52), normal(3000));
		BorderUpdate::ApplySpecificCase<FakeTraits>(items, fuse, can_rewrite);
		REQUIRE(ids(items) == std::vector<uint32_t> { 61, 52, 3000 });
	}

	SECTION("scoped sea keeps the fused piece and does not stack its edge on it") {
		const std::array<const BorderOwnership*, 2> owners { &brushes.sea, &brushes.snow };
		const std::span<const BorderOwnership* const> context = std::span(owners).subspan(1);
		const auto can_rewrite = [&](uint32_t id) { return BorderUpdate::IsRewritableByActive(id, brushes.sea, context); };

		auto items = tile(border(6652), normal(3000));
		BorderUpdate::RemoveRewritableBorders<FakeTraits>(items, can_rewrite);
		auto generated = tile(border(52));
		BorderUpdate::InsertGenerated<FakeTraits>(items, generated, Placement::Top, [&](uint32_t id) {
			return BorderUpdate::IsRepresentedOnTile<FakeTraits>(id, items, owners);
		});
		REQUIRE(ids(items) == std::vector<uint32_t> { 6652, 3000 });
	}

	SECTION("the classic rules still fuse the pieces") {
		auto items = tile(border(52), border(61), normal(3000));
		BorderUpdate::ApplySpecificCase<FakeTraits>(items, fuse, [](uint32_t) { return true; });
		REQUIRE(ids(items) == std::vector<uint32_t> { 6652, 3000 });
	}
}

TEST_CASE("specific case replaces the active brush's own border and drops the rest", "[border_update]") {
	auto items = tile(border(101), border(102), normal(3000));
	const FakeSpecificCase block { .items_to_match = { 101, 102 }, .to_replace_id = 101, .with_id = 701 };
	BorderUpdate::ApplySpecificCase<FakeTraits>(items, block, [](uint32_t) { return true; });

	REQUIRE(ids(items) == std::vector<uint32_t> { 701, 3000 });
}

TEST_CASE("specific case matching looks past clip items and needs every condition", "[border_update]") {
	SECTION("clip items in the border layer do not stop matching") {
		auto items = tile(clip(7000), border(101), border(102), normal(3000));
		const FakeSpecificCase block { .items_to_match = { 101, 102 }, .delete_all = true };
		BorderUpdate::ApplySpecificCase<FakeTraits>(items, block, [](uint32_t) { return true; });
		REQUIRE(ids(items) == std::vector<uint32_t> { 7000, 3000 });
	}

	SECTION("a missing condition leaves the tile unchanged") {
		auto items = tile(border(101), normal(3000));
		const FakeSpecificCase block { .items_to_match = { 101, 102 }, .delete_all = true };
		BorderUpdate::ApplySpecificCase<FakeTraits>(items, block, [](uint32_t) { return true; });
		REQUIRE(ids(items) == std::vector<uint32_t> { 101, 3000 });
	}

	SECTION("normal items are never matched") {
		auto items = tile(border(101), normal(102));
		const FakeSpecificCase block { .items_to_match = { 101, 102 }, .delete_all = true };
		BorderUpdate::ApplySpecificCase<FakeTraits>(items, block, [](uint32_t) { return true; });
		REQUIRE(ids(items) == std::vector<uint32_t> { 101, 102 });
	}

	SECTION("a group condition counts as a match but is not an item id to delete") {
		auto items = tile(border(101), normal(3000));
		items.front()->group = 4;
		items.front()->alignment = 2;
		const FakeSpecificCase block { .items_to_match = { 4 }, .match_group = 4, .group_match_alignment = 2, .delete_all = true };
		BorderUpdate::ApplySpecificCase<FakeTraits>(items, block, [](uint32_t) { return true; });
		// Same as the classic loop: delete_all only erases items whose id is listed in items_to_match.
		REQUIRE(ids(items) == std::vector<uint32_t> { 101, 3000 });
	}
}

TEST_CASE("rewrite verdicts name the reason a scoped update keeps an item", "[border_update]") {
	using BorderUpdate::RewriteVerdict;
	const Brushes brushes;
	const std::array<const BorderOwnership*, 1> b_in_context { &brushes.b };
	const SnowAndSea snow_and_sea;
	const std::array<const BorderOwnership*, 1> sea_in_context { &snow_and_sea.sea };

	REQUIRE(BorderUpdate::ExplainRewrite(101, brushes.a, b_in_context) == RewriteVerdict::Rewritable);
	REQUIRE(BorderUpdate::ExplainRewrite(201, brushes.a, b_in_context) == RewriteVerdict::NotOwnedByActive);
	REQUIRE(BorderUpdate::ExplainRewrite(500, brushes.a, b_in_context) == RewriteVerdict::SharedWithContext);
	REQUIRE(BorderUpdate::ExplainRewrite(500, brushes.a, {}) == RewriteVerdict::Rewritable);
	REQUIRE(BorderUpdate::ExplainRewrite(6652, snow_and_sea.snow, sea_in_context) == RewriteVerdict::JointBorder);

	// The yes/no form agrees with the verdict for every id.
	for (const uint32_t id : { 101u, 102u, 201u, 500u, 701u, 900u, 9999u }) {
		REQUIRE(BorderUpdate::IsRewritableByActive(id, brushes.a, b_in_context) == (BorderUpdate::ExplainRewrite(id, brushes.a, b_in_context) == RewriteVerdict::Rewritable));
	}
}

TEST_CASE("specific case outcome tells applied, unmatched and protected apart", "[border_update]") {
	using BorderUpdate::SpecificCaseOutcome;
	const Brushes brushes;
	const std::array<const BorderOwnership*, 1> context { &brushes.b };
	const auto can_rewrite = [&](uint32_t id) { return BorderUpdate::IsRewritableByActive(id, brushes.a, context); };
	const FakeSpecificCase fuse_own { .items_to_match = { 101, 102 }, .to_replace_id = 101, .with_id = 701 };

	auto applied = tile(border(101), border(102), normal(3000));
	REQUIRE(BorderUpdate::ApplySpecificCase<FakeTraits>(applied, fuse_own, can_rewrite) == SpecificCaseOutcome::Applied);
	REQUIRE(ids(applied) == std::vector<uint32_t> { 701, 3000 });

	auto unmatched = tile(border(101), normal(3000));
	REQUIRE(BorderUpdate::ApplySpecificCase<FakeTraits>(unmatched, fuse_own, can_rewrite) == SpecificCaseOutcome::NotMatched);
	REQUIRE(ids(unmatched) == std::vector<uint32_t> { 101, 3000 });

	auto protected_items = tile(border(500), border(101), normal(3000));
	const FakeSpecificCase touches_shared { .items_to_match = { 500, 101 }, .delete_all = true };
	REQUIRE(BorderUpdate::ApplySpecificCase<FakeTraits>(protected_items, touches_shared, can_rewrite) == SpecificCaseOutcome::Protected);
	REQUIRE(ids(protected_items) == std::vector<uint32_t> { 500, 101, 3000 });
}

TEST_CASE("ownership ids are listed in ascending order without duplicates", "[border_update]") {
	ItemIdSet set;
	set.insertAll(std::array<uint32_t, 5> { 300, 0, 100, 300, 200 });
	const auto values = set.values();
	REQUIRE(std::vector<uint32_t>(values.begin(), values.end()) == std::vector<uint32_t> { 100, 200, 300 });
}
