#ifndef RME_CLIENT_MISMATCH_PRESENTATION_H_
#define RME_CLIENT_MISMATCH_PRESENTATION_H_

#include <cstdint>
#include <format>
#include <string>
#include <string_view>

struct ClientMismatchVersionSummary {
    std::string name;
    int otbm_id = -1;
    uint32_t otb_major = 0;
    int otb_minor = 0;
};

struct ClientMismatchPresentation {
    std::string loaded_header;
    std::string loaded_name;
    std::string loaded_meta;

    std::string map_header;
    std::string map_name;
    std::string map_meta;

    bool show_tabs_warning = false;
    std::string tabs_warning_text;

    bool switch_enabled = true;
    bool force_is_default = true;
};

[[nodiscard]] inline std::string FormatOtbmDisplay(int otbm_id)
{
    if (otbm_id >= 0 && otbm_id <= 4) {
        return std::format("OTBM {}", otbm_id + 1);
    }
    return "OTBM ?";
}

[[nodiscard]] inline std::string FormatOtbDisplay(uint32_t major, int minor)
{
    return std::format("OTB {}.{}", major, minor);
}

[[nodiscard]] inline std::string FormatMismatchVersionMeta(int otbm_id, uint32_t major, int minor)
{
    return std::format("{} \xC2\xB7 {}", FormatOtbmDisplay(otbm_id), FormatOtbDisplay(major, minor));
}

[[nodiscard]] inline ClientMismatchPresentation BuildClientMismatchPresentation(
    const ClientMismatchVersionSummary& loaded, const ClientMismatchVersionSummary& map, std::string_view map_filename,
    int real_open_map_count, bool can_switch
)
{
    const std::string resolved_map_header = map_filename.empty() ? std::string("Opening Map") : std::string(map_filename);
    const bool show_warning = can_switch && real_open_map_count > 0;

    return ClientMismatchPresentation {
        .loaded_header = "LOADED CLIENT",
        .loaded_name = loaded.name,
        .loaded_meta = FormatMismatchVersionMeta(loaded.otbm_id, loaded.otb_major, loaded.otb_minor),
        .map_header = resolved_map_header,
        .map_name = map.name,
        .map_meta = FormatMismatchVersionMeta(map.otbm_id, map.otb_major, map.otb_minor),
        .show_tabs_warning = show_warning,
        .tabs_warning_text = show_warning ? "Switching clients will close all map tabs." : "",
        .switch_enabled = can_switch,
        .force_is_default = true,
    };
}

#endif
