#ifndef RME_SWITCH_CLIENT_PREFLIGHT_H_
#define RME_SWITCH_CLIENT_PREFLIGHT_H_

enum class SwitchPreconditionStatus {
    Ready,
    SameClientConflict,
    MissingModularData,
    ClientPathsNotConfigured,
};

struct SwitchPreconditionInput {
    bool is_same_client = false;
    bool has_modular_data = false;
    bool has_configured_client_paths = false;
};

[[nodiscard]] constexpr SwitchPreconditionStatus EvaluateSwitchPrecondition(const SwitchPreconditionInput& input) noexcept
{
    if (input.is_same_client) {
        return SwitchPreconditionStatus::SameClientConflict;
    }
    if (!input.has_modular_data) {
        return SwitchPreconditionStatus::MissingModularData;
    }
    if (!input.has_configured_client_paths) {
        return SwitchPreconditionStatus::ClientPathsNotConfigured;
    }
    return SwitchPreconditionStatus::Ready;
}

#endif // RME_SWITCH_CLIENT_PREFLIGHT_H_
