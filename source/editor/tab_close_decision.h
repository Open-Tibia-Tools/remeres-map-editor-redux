#ifndef RME_TAB_CLOSE_DECISION_H_
#define RME_TAB_CLOSE_DECISION_H_

enum class TabCloseKind {
    UniqueMap,
    SharedMapView,
    LiveLog,
    Other,
};

enum class TabCloseAction {
    Ignore,
    AllowNativeClose,
    VetoAndLeaveOpen,
    CloseThroughManager,
};

struct TabCloseDecisionInput {
    int tab_index = -1;
    int tab_count = 0;
    TabCloseKind kind = TabCloseKind::Other;
    bool map_has_changed = false;
    bool live_connected = false;
};

struct TabCloseDecision {
    TabCloseAction action = TabCloseAction::Ignore;
    bool prompt_save = false;
};

[[nodiscard]] constexpr TabCloseDecision DecideTabCloseAction(const TabCloseDecisionInput& input) noexcept
{
    if (input.tab_index < 0 || input.tab_index >= input.tab_count) {
        return {.action = TabCloseAction::Ignore, .prompt_save = false};
    }

    switch (input.kind) {
        case TabCloseKind::UniqueMap:
            return {
                .action = TabCloseAction::CloseThroughManager,
                .prompt_save = input.map_has_changed,
            };
        case TabCloseKind::SharedMapView:
            return {.action = TabCloseAction::AllowNativeClose, .prompt_save = false};
        case TabCloseKind::LiveLog:
            if (input.live_connected) {
                return {.action = TabCloseAction::VetoAndLeaveOpen, .prompt_save = false};
            }
            return {.action = TabCloseAction::AllowNativeClose, .prompt_save = false};
        case TabCloseKind::Other:
        default:
            return {.action = TabCloseAction::AllowNativeClose, .prompt_save = false};
    }
}

#endif
