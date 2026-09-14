#include "rendering/core/sprite_archive.h"

#include "app/definitions.h"
#include "io/filehandle.h"
#include "rendering/core/sprite_decoder.h"
#include "rendering/core/image.h"
#include "util/json.h"

#include <format>
#include <fstream>
#include <limits>
#include <algorithm>
#include <cstring>
#include <spdlog/spdlog.h>
#include <array>
#include <span>
#include <utility>
#include <lzma.h>
#include <wx/filename.h>
#include <wx/string.h>

namespace {
	constexpr uint32_t kLegacySpriteDataOffset = 3;
	constexpr int kSheetDimension = 384;
	constexpr int kSheetBytes = kSheetDimension * kSheetDimension * 4;
	constexpr int kBmpHeaderPadding = 122;
	constexpr size_t kDecodedSheetCacheLimit = 64;
	constexpr std::array<uint8_t, 5> kProtobufSheetMagic { 0x70, 0x0A, 0xFA, 0x80, 0x24 };

	bool readSpriteCount(FileReadHandle& file, bool is_extended, uint32_t& sprite_count) {
		if (is_extended) {
			return file.getU32(sprite_count);
		}

		uint16_t compact_count = 0;
		if (!file.getU16(compact_count)) {
			return false;
		}
		sprite_count = compact_count;
		return true;
	}

	bool readSpriteOffsets(FileReadHandle& file, uint32_t sprite_count, std::vector<uint32_t>& offsets) {
		offsets.assign(static_cast<size_t>(sprite_count) + 1, 0);
		for (uint32_t sprite_id = 1; sprite_id <= sprite_count; ++sprite_id) {
			if (!file.getU32(offsets[sprite_id])) {
				return false;
			}
		}
		return true;
	}

	std::pair<int, int> protobufSourceDimensions(SpriteArchive::ProtobufSpriteLayout layout) {
		switch (layout) {
			case SpriteArchive::ProtobufSpriteLayout::OneByOne:
				return { 32, 32 };
			case SpriteArchive::ProtobufSpriteLayout::OneByTwo:
				return { 32, 64 };
			case SpriteArchive::ProtobufSpriteLayout::TwoByOne:
				return { 64, 32 };
			case SpriteArchive::ProtobufSpriteLayout::TwoByTwo:
				return { 64, 64 };
		}
		return { 32, 32 };
	}
}

SpriteArchive::SpriteArchive(std::string filename, bool is_extended, uint32_t sprite_count, std::vector<uint32_t> sprite_offsets) :
	filename_(std::move(filename)),
	is_extended_(is_extended),
	sprite_count_(sprite_count),
	sprite_offsets_(std::move(sprite_offsets)),
	backend_(Backend::Legacy) {
}

SpriteArchive::SpriteArchive(std::string filename, uint32_t sprite_count, std::vector<ProtobufSheet> sheets, std::vector<int32_t> sheet_lookup) :
	filename_(std::move(filename)),
	sprite_count_(sprite_count),
	backend_(Backend::Protobuf),
	protobuf_sheets_(std::move(sheets)),
	protobuf_sheet_lookup_(std::move(sheet_lookup)) {
}

std::shared_ptr<SpriteArchive> SpriteArchive::load(const wxFileName& path, bool is_extended, wxString& error, std::vector<std::string>& warnings) {
	FileReadHandle file(path.GetFullPath().ToStdString());
	if (!file.isOk()) {
		error = wxString::FromUTF8(std::format("Failed to open {} for reading: {}", path.GetFullPath().utf8_string(), file.getErrorMessage()));
		return nullptr;
	}

	uint32_t signature = 0;
	uint32_t sprite_count = 0;
	if (!file.getU32(signature) || !readSpriteCount(file, is_extended, sprite_count)) {
		error = "Failed to read sprites header.";
		return nullptr;
	}
	(void)signature;
	if (sprite_count > MAX_SPRITES) {
		error = wxString::FromUTF8(std::format("Sprite count {} exceeds MAX_SPRITES={}.", sprite_count, MAX_SPRITES));
		return nullptr;
	}

	std::vector<uint32_t> offsets;
	if (!readSpriteOffsets(file, sprite_count, offsets)) {
		error = "Failed to read sprites index table.";
		return nullptr;
	}

	if (sprite_count == 0) {
		warnings.push_back("Sprite archive contains zero sprites.");
	}

	return std::shared_ptr<SpriteArchive>(new SpriteArchive(path.GetFullPath().ToStdString(), is_extended, sprite_count, std::move(offsets)));
}

std::shared_ptr<SpriteArchive> SpriteArchive::loadProtobuf(const wxFileName& catalog_path, wxString& error, std::vector<std::string>& warnings) {
	std::ifstream file(catalog_path.GetFullPath().ToStdString(), std::ios::in | std::ios::binary);
	if (!file.is_open()) {
		error = wxString::FromUTF8(std::format("Failed to open protobuf catalog {}.", catalog_path.GetFullPath().utf8_string()));
		return nullptr;
	}

	json::json document = json::json::parse(file, nullptr, false);
	if (document.is_discarded() || !document.is_array()) {
		error = "Invalid protobuf catalog-content.json document.";
		return nullptr;
	}

	std::vector<ProtobufSheet> sheets;
	uint32_t sprite_count = 0;
	for (const auto& entry : document) {
		if (!entry.is_object() || entry.value("type", std::string {}) != "sprite") {
			continue;
		}

		ProtobufSheet sheet;
		sheet.first_id = entry.value("firstspriteid", 0u);
		sheet.last_id = entry.value("lastspriteid", 0u);
		sheet.layout = static_cast<ProtobufSpriteLayout>(entry.value("spritetype", 0));
		sheet.path = wxFileName(catalog_path.GetPath(), wxString::FromUTF8(entry.value("file", std::string {}))).GetFullPath().ToStdString();
		if (sheet.last_id < sheet.first_id || sheet.path.empty()) {
			warnings.push_back("Skipping invalid protobuf sprite sheet entry in catalog-content.json.");
			continue;
		}
		if (sheet.last_id > MAX_SPRITES) {
			error = wxString::FromUTF8(std::format(
				"Protobuf sprite sheet {} exceeds MAX_SPRITES={} with last sprite id {}.",
				sheet.path,
				MAX_SPRITES,
				sheet.last_id
			));
			return nullptr;
		}

		sprite_count = std::max(sprite_count, sheet.last_id);
		sheets.push_back(std::move(sheet));
	}

	if (sheets.empty()) {
		error = "No protobuf sprite sheets were found in catalog-content.json.";
		return nullptr;
	}

	std::vector<int32_t> sheet_lookup(static_cast<size_t>(sprite_count) + 1, -1);
	for (size_t index = 0; index < sheets.size(); ++index) {
		for (uint32_t sprite_id = sheets[index].first_id; sprite_id <= sheets[index].last_id && sprite_id < sheet_lookup.size(); ++sprite_id) {
			sheet_lookup[sprite_id] = static_cast<int32_t>(index);
		}
	}

	return std::shared_ptr<SpriteArchive>(new SpriteArchive(catalog_path.GetFullPath().ToStdString(), sprite_count, std::move(sheets), std::move(sheet_lookup)));
}

ImageDimensions SpriteArchive::spriteDimensions(uint32_t sprite_id) const {
	if (backend_ != Backend::Protobuf || sprite_id == 0 || sprite_id >= protobuf_sheet_lookup_.size()) {
		return {};
	}

	const int32_t sheet_index = protobuf_sheet_lookup_[sprite_id];
	if (sheet_index < 0 || static_cast<size_t>(sheet_index) >= protobuf_sheets_.size()) {
		return {};
	}

	const auto [width, height] = protobufSourceDimensions(protobuf_sheets_[static_cast<size_t>(sheet_index)].layout);
	return ImageDimensions {
		static_cast<uint16_t>(width),
		static_cast<uint16_t>(height),
	};
}


bool SpriteArchive::readCompressed(uint32_t sprite_id, std::unique_ptr<uint8_t[]>& target, uint16_t& size) const {
	size = 0;
	target.reset();

	if (backend_ != Backend::Legacy) {
		return false;
	}
	if (sprite_id == 0) {
		return true;
	}
	if (sprite_id >= sprite_offsets_.size()) {
		return false;
	}

	const uint32_t offset = sprite_offsets_[sprite_id];
	if (offset == 0) {
		return true;
	}

	FileReadHandle file(filename_);
	if (!file.isOk()) {
		return false;
	}
	if (!file.seek(offset + kLegacySpriteDataOffset)) {
		return false;
	}

	uint16_t compressed_size = 0;
	if (!file.getU16(compressed_size)) {
		return false;
	}

	auto buffer = std::make_unique<uint8_t[]>(compressed_size);
	if (!file.getRAW(buffer.get(), compressed_size)) {
		return false;
	}

	size = compressed_size;
	target = std::move(buffer);
	return true;
}

bool SpriteArchive::readLegacyRgba(uint32_t sprite_id, bool use_alpha, std::unique_ptr<uint8_t[]>& target, ImageDimensions& dimensions) const {
	std::unique_ptr<uint8_t[]> compressed;
	uint16_t compressed_size = 0;
	if (!readCompressed(sprite_id, compressed, compressed_size)) {
		return false;
	}

	dimensions = ImageDimensions { 32, 32 };
	if (sprite_id == 0 || !compressed) {
		target = std::make_unique<uint8_t[]>(dimensions.pixelCount() * 4);
		return true;
	}

	target = SpriteDecoder::DecodeRle(std::span { compressed.get(), compressed_size }, use_alpha, static_cast<int>(sprite_id));
	return target != nullptr;
}

bool SpriteArchive::loadSheetPixels(const ProtobufSheet& sheet) const {
	if (sheet.decoded_pixels) {
		return true;
	}

	std::ifstream file(sheet.path, std::ios::binary | std::ios::in);
	if (!file.is_open()) {
		spdlog::error("SpriteArchive: failed to open protobuf sprite sheet {}", sheet.path);
		return false;
	}

	file.seekg(0, std::ios::end);
	const std::streamsize size = file.tellg();
	if (size <= 0) {
		spdlog::error("SpriteArchive: protobuf sprite sheet {} is empty", sheet.path);
		return false;
	}

	file.seekg(0, std::ios::beg);
	std::vector<uint8_t> buffer(static_cast<size_t>(size));
	if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
		spdlog::error("SpriteArchive: failed to read protobuf sprite sheet {}", sheet.path);
		return false;
	}
	if (buffer.empty()) {
		spdlog::error("SpriteArchive: protobuf sprite sheet {} is empty", sheet.path);
		return false;
	}

	size_t position = 0;
	while (position < buffer.size() && buffer[position] == 0x00) {
		++position;
	}
	if (position >= buffer.size()) {
		spdlog::error("SpriteArchive: protobuf sprite sheet {} is missing the CipSoft header", sheet.path);
		return false;
	}

	if (position + kProtobufSheetMagic.size() > buffer.size()
		|| !std::equal(kProtobufSheetMagic.begin(), kProtobufSheetMagic.end(), buffer.begin() + static_cast<std::ptrdiff_t>(position))) {
		spdlog::error("SpriteArchive: protobuf sprite sheet {} has an invalid header", sheet.path);
		return false;
	}

	position += kProtobufSheetMagic.size();
	while (position < buffer.size() && (buffer[position++] & 0x80) == 0x80) {
	}
	if (position + 13 > buffer.size()) {
		spdlog::error("SpriteArchive: protobuf sprite sheet {} has an incomplete LZMA header", sheet.path);
		return false;
	}

	const uint8_t lclppb = buffer[position++];
	lzma_options_lzma options {};
	options.lc = lclppb % 9;
	const int remainder = lclppb / 9;
	options.lp = remainder % 5;
	options.pb = remainder / 5;

	uint32_t dictionary_size = 0;
	for (uint8_t byte_index = 0; byte_index < 4; ++byte_index) {
		dictionary_size += static_cast<uint32_t>(buffer[position++]) << (byte_index * 8);
	}
	options.dict_size = dictionary_size;

	position += 8; // Skip compressed-size field.

	lzma_stream stream = LZMA_STREAM_INIT;
	lzma_filter filters[2] = {
		lzma_filter { LZMA_FILTER_LZMA1, &options },
		lzma_filter { LZMA_VLI_UNKNOWN, nullptr }
	};

	if (lzma_raw_decoder(&stream, filters) != LZMA_OK) {
		spdlog::error("SpriteArchive: failed to initialize LZMA decoder for {}", sheet.path);
		return false;
	}

	auto decompressed = std::make_unique<uint8_t[]>(kSheetBytes + kBmpHeaderPadding);
	stream.next_in = buffer.data() + position;
	stream.avail_in = buffer.size() - position;
	stream.next_out = decompressed.get();
	stream.avail_out = kSheetBytes + kBmpHeaderPadding;

	const lzma_ret ret = lzma_code(&stream, LZMA_RUN);
	lzma_end(&stream);
	if (ret != LZMA_STREAM_END || stream.total_out < static_cast<uint64_t>(kBmpHeaderPadding) + kSheetBytes) {
		spdlog::error("SpriteArchive: failed to decode protobuf sprite sheet {} (lzma ret={})", sheet.path, static_cast<int>(ret));
		return false;
	}

	uint32_t pixel_offset = 0;
	std::memcpy(&pixel_offset, decompressed.get() + 10, sizeof(uint32_t));
	if (pixel_offset > static_cast<uint32_t>(kBmpHeaderPadding)) {
		spdlog::error("SpriteArchive: protobuf sprite sheet {} has an invalid BMP pixel offset", sheet.path);
		return false;
	}

	auto decoded_pixels = std::make_shared<std::vector<uint8_t>>(kSheetBytes, 0);
	uint8_t* pixel_data = decompressed.get() + pixel_offset;
	for (int row = 0; row < kSheetDimension; ++row) {
		const int source_row = kSheetDimension - row - 1;
		std::memcpy(decoded_pixels->data() + row * kSheetDimension * 4, pixel_data + source_row * kSheetDimension * 4, static_cast<size_t>(kSheetDimension) * 4);
	}

	sheet.decoded_pixels = std::move(decoded_pixels);
	++decoded_sheet_count_;
	return true;
}

void SpriteArchive::pruneDecodedSheetCache(int32_t keep_sheet_index) const {
	while (decoded_sheet_count_ > kDecodedSheetCacheLimit) {
		size_t oldest_index = protobuf_sheets_.size();
		uint64_t oldest_tick = std::numeric_limits<uint64_t>::max();

		for (size_t index = 0; index < protobuf_sheets_.size(); ++index) {
			if (static_cast<int32_t>(index) == keep_sheet_index) {
				continue;
			}

			const auto& sheet = protobuf_sheets_[index];
			if (!sheet.decoded_pixels || sheet.last_access_tick == 0 || sheet.last_access_tick >= oldest_tick) {
				continue;
			}

			oldest_tick = sheet.last_access_tick;
			oldest_index = index;
		}

		if (oldest_index == protobuf_sheets_.size()) {
			return;
		}

		protobuf_sheets_[oldest_index].releaseDecodedPixels();
		--decoded_sheet_count_;
	}
}

bool SpriteArchive::readProtobufRgba(uint32_t sprite_id, std::unique_ptr<uint8_t[]>& target, ImageDimensions& dimensions) const {
	if (sprite_id == 0) {
		dimensions = {};
		target = std::make_unique<uint8_t[]>(dimensions.pixelCount() * 4);
		return true;
	}

	if (sprite_id >= protobuf_sheet_lookup_.size()) {
		dimensions = {};
		target.reset();
		return false;
	}

	const int32_t sheet_index = protobuf_sheet_lookup_[sprite_id];
	if (sheet_index < 0 || static_cast<size_t>(sheet_index) >= protobuf_sheets_.size()) {
		return false;
	}

	std::lock_guard<std::mutex> lock(protobuf_mutex_);
	auto& sheet = protobuf_sheets_[static_cast<size_t>(sheet_index)];
	if (!loadSheetPixels(sheet) || !sheet.decoded_pixels) {
		return false;
	}
	sheet.last_access_tick = ++protobuf_sheet_access_tick_;
	pruneDecodedSheetCache(sheet_index);

	const auto [source_width, source_height] = protobufSourceDimensions(sheet.layout);
	dimensions = ImageDimensions {
		static_cast<uint16_t>(source_width),
		static_cast<uint16_t>(source_height),
	};
	target = std::make_unique<uint8_t[]>(dimensions.pixelCount() * 4);
	std::fill(target.get(), target.get() + dimensions.pixelCount() * 4, 0);

	const uint32_t sprite_offset = sprite_id - sheet.first_id;
	const int columns = kSheetDimension / source_width;
	const int sprite_row = static_cast<int>(sprite_offset / static_cast<uint32_t>(columns));
	const int rows = kSheetDimension / source_height;
	if (sprite_row >= rows) {
		spdlog::error("SpriteArchive: sprite {} is outside the bounds of sheet {}", sprite_id, sheet.path);
		target.reset();
		dimensions = {};
		return false;
	}

	const int sprite_column = static_cast<int>(sprite_offset % static_cast<uint32_t>(columns));
	const auto* pixels = sheet.decoded_pixels->data();

	for (int row = 0; row < source_height; ++row) {
		const size_t source_offset = (static_cast<size_t>(sprite_row * source_height + row) * kSheetDimension + static_cast<size_t>(sprite_column * source_width)) * 4;
		const size_t destination_offset = static_cast<size_t>(row) * dimensions.width * 4;
		for (int column = 0; column < source_width; ++column) {
			const size_t source_pixel = source_offset + static_cast<size_t>(column) * 4;
			const size_t destination_pixel = destination_offset + static_cast<size_t>(column) * 4;
			target[destination_pixel + 0] = pixels[source_pixel + 2];
			target[destination_pixel + 1] = pixels[source_pixel + 1];
			target[destination_pixel + 2] = pixels[source_pixel + 0];
			target[destination_pixel + 3] = pixels[source_pixel + 3];
		}
	}

	return true;
}

bool SpriteArchive::readRGBA(uint32_t sprite_id, bool use_alpha, std::unique_ptr<uint8_t[]>& target, ImageDimensions& dimensions) const {
	target.reset();
	dimensions = {};

	switch (backend_) {
		case Backend::Legacy:
			return readLegacyRgba(sprite_id, use_alpha, target, dimensions);
		case Backend::Protobuf:
			return readProtobufRgba(sprite_id, target, dimensions);
	}
	return false;
}
