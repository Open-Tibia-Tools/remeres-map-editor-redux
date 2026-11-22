#ifndef RME_CLIENT_ASSET_DETECTOR_H_
#define RME_CLIENT_ASSET_DETECTOR_H_

#include <optional>
#include <string>
#include <vector>

#include <filesystem>

#include "app/client_version.h"

struct ClientAssetDetectionResult {
	std::optional<std::string> metadata_file_name;
	std::optional<std::string> sprites_file_name;
	std::optional<uint32_t> dat_signature;
	std::optional<uint32_t> spr_signature;
	std::optional<DatFormat> dat_format;
	std::optional<bool> transparency;
	std::optional<bool> extended;
	std::optional<bool> frame_durations;
	std::optional<bool> frame_groups;
	std::vector<std::string> warnings;
};

class ClientAssetDetector {
public:
	[[nodiscard]] static ClientAssetDetectionResult detect(const ClientVersion& client);

private:
    static void internalDetectSprDat(const ClientVersion& client, ClientAssetDetectionResult& result);
    static void internalDetectProtobuf(const ClientVersion& client, ClientAssetDetectionResult& result);
};

#endif
