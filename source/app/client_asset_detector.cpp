#include "app/client_asset_detector.h"

#include <algorithm>
#include <array>
#include <exception>
#include <fstream>
#include <format>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <spdlog/spdlog.h>

#include "app/definitions.h"
#include "io/filehandle.h"
#include "item_definitions/core/item_definition_fragments.h"
#include "item_definitions/core/item_definition_types.h"
#include "item_definitions/formats/dat/dat_item_parser.h"
#include "rendering/core/sprite_archive.h"
#include "util/json.h"

namespace {
	constexpr size_t kMaxSampleOffsets = 24;

	struct ResolvedClientFile {
		wxFileName path;
		std::string filename;
		bool exists = false;
	};

	struct DatProbeCandidate {
		DatFormat format = DAT_FORMAT_UNKNOWN;
		bool extended = false;
		bool frame_durations = false;
		bool frame_groups = false;
		size_t warning_count = 0;
	};

	struct SpriteProbeCandidate {
		bool extended = false;
		uint32_t sprite_count = 0;
		int opaque_matches = 0;
		int alpha_matches = 0;
	};

	ResolvedClientFile resolveClientFile(const wxFileName& client_path, const std::string& configured_name, const std::string& fallback_name) {
		const auto isSafeRelativePath = [](const std::string& raw_name) {
			if (raw_name.empty()) {
				return false;
			}
			const wxFileName path(wxString::FromUTF8(raw_name));
			if (path.IsAbsolute()) {
				return false;
			}
			for (const auto& directory : path.GetDirs()) {
				if (directory == "..") {
					return false;
				}
			}
			return true;
		};

		for (const auto& candidate_name : { configured_name, fallback_name }) {
			if (!isSafeRelativePath(candidate_name)) {
				continue;
			}
			ResolvedClientFile resolved;
			resolved.filename = candidate_name;
			resolved.path = wxFileName(client_path.GetFullPath(), wxString::FromUTF8(candidate_name));
			resolved.exists = resolved.path.FileExists();
			if (resolved.exists) {
				return resolved;
			}
		}
		return {};
	}

	std::optional<uint32_t> readSignature(const wxFileName& path, std::string_view label, std::vector<std::string>& warnings) {
		FileReadHandle file(path.GetFullPath().ToStdString());
		if (!file.isOk()) {
			const auto message = std::format("{} signature detection failed: could not open {}", label, path.GetFullPath().ToStdString());
			spdlog::warn(message);
			warnings.push_back(message);
			return std::nullopt;
		}

		uint32_t signature = 0;
		if (!file.getU32(signature)) {
			const auto message = std::format("{} signature detection failed: could not read header from {}", label, path.GetFullPath().ToStdString());
			spdlog::warn(message);
			warnings.push_back(message);
			return std::nullopt;
		}

		return signature;
	}

	std::vector<DatProbeCandidate> probeDatCandidates(const wxFileName& dat_path, uint32_t dat_signature) {
		static constexpr std::array formats = {
			DAT_FORMAT_74,
			DAT_FORMAT_755,
			DAT_FORMAT_78,
			DAT_FORMAT_86,
			DAT_FORMAT_96,
			DAT_FORMAT_1010,
			DAT_FORMAT_1050,
			DAT_FORMAT_1057,
		};

		const DatItemParser parser;
		std::vector<DatProbeCandidate> successes;

		for (const auto format : formats) {
			for (const bool extended : { false, true }) {
				const std::array frame_duration_options = { false, true };
				const std::array frame_group_options = { false, true };

				for (const bool frame_durations : frame_duration_options) {
					for (const bool frame_groups : frame_group_options) {
						OtbVersion otb {};
						ClientVersion probe_client(otb, "detector-probe", "");
						probe_client.setExtended(extended);
						probe_client.setFrameDurations(frame_durations);
						probe_client.setFrameGroups(frame_groups);
						probe_client.setClientData(dat_signature, 0, format);

						ItemDefinitionLoadInput input;
						input.dat_path = dat_path;
						input.client_version = &probe_client;

						DatCatalog catalog;
						wxString error;
						std::vector<std::string> warnings;
						if (!parser.parseCatalog(input, catalog, error, warnings)) {
							continue;
						}

						successes.push_back(DatProbeCandidate {
							.format = format,
							.extended = extended,
							.frame_durations = frame_durations,
							.frame_groups = frame_groups,
							.warning_count = warnings.size(),
						});
					}
				}
			}
		}

		if (successes.empty()) {
			return successes;
		}

		const auto min_warning_count = std::ranges::min(successes, {}, &DatProbeCandidate::warning_count).warning_count;
		std::erase_if(successes, [min_warning_count](const DatProbeCandidate& candidate) {
			return candidate.warning_count != min_warning_count;
		});
		return successes;
	}

	bool validateSpriteRunEncoding(std::span<const uint8_t> blob, uint8_t colored_bpp) {
		size_t read = 0;
		size_t written_pixels = 0;
		constexpr size_t kSpritePixels = SPRITE_PIXELS * SPRITE_PIXELS;

		while (read + 4 <= blob.size() && written_pixels < kSpritePixels) {
			const uint16_t transparent = static_cast<uint16_t>(blob[read] | (blob[read + 1] << 8));
			read += 2;
			const uint16_t colored = static_cast<uint16_t>(blob[read] | (blob[read + 1] << 8));
			read += 2;

			if (written_pixels + transparent + colored > kSpritePixels) {
				return false;
			}
			const size_t colored_bytes = static_cast<size_t>(colored) * colored_bpp;
			if (read + colored_bytes > blob.size()) {
				return false;
			}

			read += colored_bytes;
			written_pixels += transparent + colored;
		}

		return written_pixels == kSpritePixels && read == blob.size();
	}

	std::optional<std::vector<uint32_t>> collectSpriteSamples(FileReadHandle& file, bool extended, uint32_t& sprite_count) {
		if (!SpriteArchive::readSpriteCount(file, extended, sprite_count)) {
			return std::nullopt;
		}
		if (sprite_count == 0 || sprite_count > MAX_SPRITES) {
			return std::nullopt;
		}

		const size_t header_size = 4 + (extended ? sizeof(uint32_t) : sizeof(uint16_t)) + static_cast<size_t>(sprite_count) * sizeof(uint32_t);
		if (header_size > file.size()) {
			return std::nullopt;
		}

		std::vector<uint32_t> sample_offsets;
		sample_offsets.reserve(kMaxSampleOffsets);

		uint32_t previous_non_zero_offset = 0;
		for (uint32_t sprite_id = 1; sprite_id <= sprite_count; ++sprite_id) {
			uint32_t offset = 0;
			if (!file.getU32(offset)) {
				return std::nullopt;
			}
			if (offset == 0) {
				continue;
			}
			if (offset < header_size || offset > file.size() - 5) {
				return std::nullopt;
			}
			if (previous_non_zero_offset != 0 && offset < previous_non_zero_offset) {
				return std::nullopt;
			}
			previous_non_zero_offset = offset;
			if (sample_offsets.size() < kMaxSampleOffsets) {
				sample_offsets.push_back(offset);
			}
		}

		return sample_offsets;
	}

	std::optional<SpriteProbeCandidate> probeSpriteCandidate(const wxFileName& spr_path, bool extended) {
		FileReadHandle file(spr_path.GetFullPath().ToStdString());
		if (!file.isOk()) {
			return std::nullopt;
		}

		uint32_t signature = 0;
		if (!file.getU32(signature)) {
			return std::nullopt;
		}

		uint32_t sprite_count = 0;
		auto sample_offsets = collectSpriteSamples(file, extended, sprite_count);
		if (!sample_offsets.has_value()) {
			return std::nullopt;
		}

		SpriteProbeCandidate candidate;
		candidate.extended = extended;
		candidate.sprite_count = sprite_count;

		for (const uint32_t offset : *sample_offsets) {
			if (!file.seek(offset + 3)) {
				return std::nullopt;
			}

			uint16_t compressed_size = 0;
			if (!file.getU16(compressed_size)) {
				return std::nullopt;
			}
			if (file.tell() + compressed_size > file.size()) {
				return std::nullopt;
			}

			std::vector<uint8_t> blob(compressed_size);
			if (!file.getRAW(blob.data(), compressed_size)) {
				return std::nullopt;
			}

			if (validateSpriteRunEncoding(blob, 3)) {
				candidate.opaque_matches += 1;
			}
			if (validateSpriteRunEncoding(blob, 4)) {
				candidate.alpha_matches += 1;
			}
		}

		return candidate;
	}

	void assignConsensus(const std::vector<DatProbeCandidate>& candidates, ClientAssetDetectionResult& result) {
		if (candidates.empty()) {
			return;
		}

		const auto same_extended = std::ranges::all_of(candidates, [&](const DatProbeCandidate& candidate) {
			return candidate.extended == candidates.front().extended;
		});
		if (same_extended) {
			result.extended = candidates.front().extended;
		}

		const auto same_frame_durations = std::ranges::all_of(candidates, [&](const DatProbeCandidate& candidate) {
			return candidate.frame_durations == candidates.front().frame_durations;
		});
		if (same_frame_durations) {
			result.frame_durations = candidates.front().frame_durations;
		}

		const auto same_frame_groups = std::ranges::all_of(candidates, [&](const DatProbeCandidate& candidate) {
			return candidate.frame_groups == candidates.front().frame_groups;
		});
		if (same_frame_groups) {
			result.frame_groups = candidates.front().frame_groups;
		}

		const auto same_format = std::ranges::all_of(candidates, [&](const DatProbeCandidate& candidate) {
			return candidate.format == candidates.front().format;
		});
		if (same_format) {
			result.dat_format = candidates.front().format;
		}
	}
}

ClientAssetDetectionResult ClientAssetDetector::detect(const ClientVersion& client) {
	// common code: check if path exists
	ClientAssetDetectionResult result;
	if (!client.getClientPath().DirExists()) {
		const auto message = "Client asset detection skipped: selected client path does not exist.";
		spdlog::warn(message);
		result.warnings.emplace_back(message);
		return result;
	}

	// protobuf
	if (client.isProtobuf()) {
		internalDetectProtobuf(client, result);
        return result;
	}

	// spr/dat
	internalDetectSprDat(client, result);
    return result;
}

void ClientAssetDetector::internalDetectSprDat(const ClientVersion& client, ClientAssetDetectionResult& result) {
	const auto client_path = client.getClientPath();
	const auto dat_file = resolveClientFile(client_path, client.getMetadataFile(), std::string { ASSETS_NAME } + ".dat");
	const auto spr_file = resolveClientFile(client_path, client.getSpritesFile(), std::string { ASSETS_NAME } + ".spr");

	if (!dat_file.exists) {
		const auto message = "Client asset detection failed: DAT file was not found in the selected client path.";
		spdlog::warn(message);
		result.warnings.emplace_back(message);
	} else {
		result.metadata_file_name = dat_file.filename;
		result.dat_signature = readSignature(dat_file.path, "DAT", result.warnings);
	}

	if (!spr_file.exists) {
		const auto message = "Client asset detection failed: SPR file was not found in the selected client path.";
		spdlog::warn(message);
		result.warnings.emplace_back(message);
	} else {
		result.sprites_file_name = spr_file.filename;
		result.spr_signature = readSignature(spr_file.path, "SPR", result.warnings);
	}

	if (result.dat_signature.has_value()) {
		const auto dat_candidates = probeDatCandidates(dat_file.path, *result.dat_signature);
		if (dat_candidates.empty()) {
			const auto message = "Client asset detection could not derive DAT feature flags from the DAT binary.";
			spdlog::warn("{} DAT={:X}", message, *result.dat_signature);
			result.warnings.emplace_back(message);
		} else {
			assignConsensus(dat_candidates, result);
			spdlog::info("Client asset detection found {} structurally valid DAT candidate(s) for DAT={:X}.", dat_candidates.size(), *result.dat_signature);
		}
	}

	if (spr_file.exists) {
		std::vector<SpriteProbeCandidate> sprite_candidates;
		for (const bool extended : { false, true }) {
			if (auto candidate = probeSpriteCandidate(spr_file.path, extended)) {
				sprite_candidates.push_back(*candidate);
			}
		}

		if (sprite_candidates.empty()) {
			const auto message = "Client asset detection could not derive SPR structure from the SPR binary.";
			spdlog::warn("{}", message);
			result.warnings.emplace_back(message);
		} else {
			const auto best_candidate = std::ranges::max_element(sprite_candidates, {}, [](const SpriteProbeCandidate& candidate) {
				return candidate.opaque_matches + candidate.alpha_matches;
			});

			if (best_candidate != sprite_candidates.end() && (best_candidate->opaque_matches > 0 || best_candidate->alpha_matches > 0)) {
				if (!result.extended.has_value()) {
					result.extended = best_candidate->extended;
				} else if (result.extended != best_candidate->extended) {
					const auto message = "DAT and SPR probing disagree on the extended flag.";
					spdlog::warn("{}", message);
					result.warnings.emplace_back(message);
					result.extended.reset();
				}
			}

			if (best_candidate != sprite_candidates.end()) {
				if (best_candidate->alpha_matches > best_candidate->opaque_matches) {
					result.transparency = true;
				} else if (best_candidate->opaque_matches > best_candidate->alpha_matches) {
					result.transparency = false;
				}
			}
		}
	}
}

void ClientAssetDetector::internalDetectProtobuf(const ClientVersion& client, ClientAssetDetectionResult& result) {
	// in protobuf mode, these features are always supported
	result.transparency = true;
	result.extended = true;
	result.frame_durations = true;
	result.frame_groups = true;

	// search for catalog-content.json first
	const auto client_path = client.getClientPath();
	const auto catalog_file = resolveClientFile(client_path, "catalog-content.json", "assets/catalog-content.json");

	// determine the catalog file
	// catalog-content contains protobuf-encoded dat filename and indexing for spritesheets
	if (!catalog_file.exists) {
		const auto message = "Client asset detection failed: catalog-content.json was not found in the selected client path.";
		spdlog::warn(message);
		result.warnings.emplace_back(message);
		return;
	}

	// open catalog-content.json
	std::ifstream catalog_stream(catalog_file.path.GetFullPath().ToStdString());
	if (!catalog_stream.is_open()) {
		const auto message = std::format("Client asset detection failed: could not open {}", catalog_file.path.GetFullPath().ToStdString());
		spdlog::warn(message);
		result.warnings.emplace_back(message);
		return;
	}

	// unserialize catalog-content.json
	ResolvedClientFile appearances_file;
	try {
		json::json root;
		catalog_stream >> root;

		const auto scanCatalogEntry = [&](const json::json& entry) {
			if (!entry.is_object()) {
				return;
			}

			if (!entry.contains("type") || !entry["type"].is_string()) {
				return;
			}

			const auto entry_type = entry["type"].get<std::string>();
			if (entry_type != "appearances") {
				return;
			}

			if (entry.contains("file") && entry["file"].is_string()) {
                auto appearancesFileName = entry["file"].get<std::string>();
				appearances_file = resolveClientFile(client_path, appearancesFileName, "assets/" + appearancesFileName);
			}
		};

		if (root.is_array()) {
			for (const auto& entry : root) {
				scanCatalogEntry(entry);
			}
		} else {
			scanCatalogEntry(root);
		}
	} catch (const std::exception& e) {
		const auto message = std::format("Client asset detection failed: could not read catalog-content.json: {}", e.what());
		spdlog::warn(message);
		result.warnings.emplace_back(message);
		return;
	}

	// check if appearances exists
	if (!appearances_file.exists) {
		const auto message = "Client asset detection failed: appearances DAT file from catalog-content was not found.";
		spdlog::warn(message);
		result.warnings.emplace_back(message);
        return;
	}

	// apply file names
	result.metadata_file_name = appearances_file.filename;
	result.sprites_file_name = catalog_file.filename;

	// file signatures are not applicable to protobuf
	result.dat_signature = 0;
    result.spr_signature = 0;
}
