#ifndef RME_RENDERING_CORE_SPRITE_ARCHIVE_H_
#define RME_RENDERING_CORE_SPRITE_ARCHIVE_H_

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

struct ImageDimensions;

class SpriteArchive {
public:
	enum class ProtobufSpriteLayout : uint8_t {
		OneByOne = 0,
		OneByTwo = 1,
		TwoByOne = 2,
		TwoByTwo = 3,
	};

	[[nodiscard]] static std::shared_ptr<SpriteArchive> load(const std::filesystem::path& path, bool is_extended, std::string& error, std::vector<std::string>& warnings);
	[[nodiscard]] static std::shared_ptr<SpriteArchive> loadProtobuf(const std::filesystem::path& catalog_path, std::string& error, std::vector<std::string>& warnings);

	[[nodiscard]] uint32_t spriteCount() const {
		return sprite_count_;
	}

	[[nodiscard]] bool isExtended() const {
		return is_extended_;
	}

	[[nodiscard]] const std::string& fileName() const {
		return filename_;
	}

	[[nodiscard]] bool isProtobuf() const {
		return backend_ == Backend::Protobuf;
	}

	[[nodiscard]] ImageDimensions spriteDimensions(uint32_t sprite_id) const;
	[[nodiscard]] bool readCompressed(uint32_t sprite_id, std::unique_ptr<uint8_t[]>& target, uint16_t& size) const;
	[[nodiscard]] bool readRGBA(uint32_t sprite_id, bool use_alpha, std::unique_ptr<uint8_t[]>& target, ImageDimensions& dimensions) const;

private:
	enum class Backend : uint8_t {
		Legacy,
		Protobuf,
	};

	struct ProtobufSheet {
		uint32_t first_id = 0;
		uint32_t last_id = 0;
		ProtobufSpriteLayout layout = ProtobufSpriteLayout::OneByOne;
		std::string path;
		mutable std::shared_ptr<std::vector<uint8_t>> decoded_pixels;
		mutable uint64_t last_access_tick = 0;

		void releaseDecodedPixels() const {
			decoded_pixels.reset();
			last_access_tick = 0;
		}
	};

	SpriteArchive(std::string filename, bool is_extended, uint32_t sprite_count, std::vector<uint32_t> sprite_offsets);
	SpriteArchive(std::string filename, uint32_t sprite_count, std::vector<ProtobufSheet> sheets, std::vector<int32_t> sheet_lookup);

	[[nodiscard]] bool readLegacyRgba(uint32_t sprite_id, bool use_alpha, std::unique_ptr<uint8_t[]>& target, ImageDimensions& dimensions) const;
	[[nodiscard]] bool readProtobufRgba(uint32_t sprite_id, std::unique_ptr<uint8_t[]>& target, ImageDimensions& dimensions) const;
	[[nodiscard]] bool loadSheetPixels(const ProtobufSheet& sheet) const;
	void pruneDecodedSheetCache(int32_t keep_sheet_index) const;

	std::string filename_;
	bool is_extended_ = false;
	uint32_t sprite_count_ = 0;
	std::vector<uint32_t> sprite_offsets_;

	Backend backend_ = Backend::Legacy;
	mutable std::mutex protobuf_mutex_;
	mutable uint64_t protobuf_sheet_access_tick_ = 0;
	mutable size_t decoded_sheet_count_ = 0;
	std::vector<ProtobufSheet> protobuf_sheets_;
	std::vector<int32_t> protobuf_sheet_lookup_;
};

#endif
