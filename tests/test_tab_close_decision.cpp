#include "editor/tab_close_decision.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("DecideTabCloseAction ignores out-of-range tab indices", "[tab_close_decision]")
{
    const auto negative = DecideTabCloseAction(TabCloseDecisionInput {
        .tab_index = -1,
        .tab_count = 3,
        .kind = TabCloseKind::UniqueMap,
        .map_has_changed = true,
    });
    REQUIRE(negative.action == TabCloseAction::Ignore);
    REQUIRE_FALSE(negative.prompt_save);

    const auto past_end = DecideTabCloseAction(TabCloseDecisionInput {
        .tab_index = 3,
        .tab_count = 3,
        .kind = TabCloseKind::UniqueMap,
        .map_has_changed = false,
    });
    REQUIRE(past_end.action == TabCloseAction::Ignore);
    REQUIRE_FALSE(past_end.prompt_save);

    const auto empty = DecideTabCloseAction(TabCloseDecisionInput {
        .tab_index = 0,
        .tab_count = 0,
        .kind = TabCloseKind::UniqueMap,
    });
    REQUIRE(empty.action == TabCloseAction::Ignore);
}

TEST_CASE("DecideTabCloseAction routes unique MapTab through manager and preserves index targeting", "[tab_close_decision]")
{
    const auto clean_tab_2 = DecideTabCloseAction(TabCloseDecisionInput {
        .tab_index = 2,
        .tab_count = 3,
        .kind = TabCloseKind::UniqueMap,
        .map_has_changed = false,
    });
    REQUIRE(clean_tab_2.action == TabCloseAction::CloseThroughManager);
    REQUIRE_FALSE(clean_tab_2.prompt_save);

    const auto dirty_tab_1 = DecideTabCloseAction(TabCloseDecisionInput {
        .tab_index = 1,
        .tab_count = 3,
        .kind = TabCloseKind::UniqueMap,
        .map_has_changed = true,
    });
    REQUIRE(dirty_tab_1.action == TabCloseAction::CloseThroughManager);
    REQUIRE(dirty_tab_1.prompt_save);
}

TEST_CASE("DecideTabCloseAction handles shared map views and live log tabs", "[tab_close_decision]")
{
    const auto shared_view = DecideTabCloseAction(TabCloseDecisionInput {
        .tab_index = 1,
        .tab_count = 2,
        .kind = TabCloseKind::SharedMapView,
        .map_has_changed = true,
    });
    REQUIRE(shared_view.action == TabCloseAction::AllowNativeClose);
    REQUIRE_FALSE(shared_view.prompt_save);

    const auto live_connected = DecideTabCloseAction(TabCloseDecisionInput {
        .tab_index = 0,
        .tab_count = 2,
        .kind = TabCloseKind::LiveLog,
        .live_connected = true,
    });
    REQUIRE(live_connected.action == TabCloseAction::VetoAndLeaveOpen);

    const auto live_disconnected = DecideTabCloseAction(TabCloseDecisionInput {
        .tab_index = 0,
        .tab_count = 2,
        .kind = TabCloseKind::LiveLog,
        .live_connected = false,
    });
    REQUIRE(live_disconnected.action == TabCloseAction::AllowNativeClose);
}
