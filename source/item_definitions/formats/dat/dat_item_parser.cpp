#include "item_definitions/formats/dat/dat_item_parser.h"

#include "app/client_version.h"
#include "io/filehandle.h"
#include "util/common.h"

#include <algorithm>
#include <climits>
#include <format>
#include <limits>

namespace {
	constexpr uint64_t flagMask(ItemFlag flag) {
		return uint64_t { 1 } << static_cast<uint8_t>(flag);
	}

	constexpr uint8_t remapFlag(uint8_t flag, DatFormat format) {
		if (format >= DAT_FORMAT_1010) {
			if (flag == 16) {
				return DatFlagNoMoveAnimation;
			}
			if (flag > 16) {
				return flag - 1;
			}
		} else if (format >= DAT_FORMAT_86) {
			return flag;
		} else if (format >= DAT_FORMAT_78) {
			if (flag == 8) {
				return DatFlagChargeable;
			}
			if (flag > 8) {
				return flag - 1;
			}
		} else if (format >= DAT_FORMAT_755) {
			if (flag == 23) {
				return DatFlagFloorChange;
			}
		} else if (format >= DAT_FORMAT_74) {
			// Legacy 7.4 keeps flags 1..15 densely packed one slot earlier.
			// This shift must happen before any other remapping or payload reads desync.
			if (flag > 0 && flag <= 15) {
				return static_cast<uint8_t>(flag + 1);
			}
			switch (flag) {
				case 16: return DatFlagLight;
				case 17: return DatFlagFloorChange;
				case 18: return DatFlagFullGround;
				case 19: return DatFlagElevation;
				case 20: return DatFlagDisplacement;
				case 22: return DatFlagMinimapColor;
				case 23: return DatFlagRotateable;
				case 24: return DatFlagLyingCorpse;
				case 25: return DatFlagHangable;
				case 26: return DatFlagHookSouth;
				case 27: return DatFlagHookEast;
				case 28: return DatFlagAnimateAlways;
			}
		}
		return flag;
	}

	bool readFlagPayload(DatFormat format, FileReadHandle& file, DatCatalogEntry& entry, uint8_t flag, uint8_t previous_flag, wxString& error, std::vector<std::string>& warnings) {
		auto& fragment = entry.item_fragment;

		switch (flag) {
			case DatFlagGroundBorder:
				fragment.flags |= flagMask(ItemFlag::AlwaysOnBottom) | flagMask(ItemFlag::IsBorder);
				fragment.always_on_top_order = 1;
				return true;
			case DatFlagOnBottom:
				fragment.flags |= flagMask(ItemFlag::AlwaysOnBottom);
				fragment.always_on_top_order = 2;
				return true;
			case DatFlagOnTop:
				fragment.flags |= flagMask(ItemFlag::AlwaysOnBottom);
				fragment.always_on_top_order = 3;
				return true;
			case DatFlagContainer:
				fragment.group = ITEM_GROUP_CONTAINER;
				fragment.type = ITEM_TYPE_CONTAINER;
				return true;
			case DatFlagStackable:
				fragment.flags |= flagMask(ItemFlag::Stackable);
				return true;
			case DatFlagForceUse:
				fragment.flags |= flagMask(ItemFlag::ForceUse);
				return true;
			case DatFlagMultiUse:
				fragment.flags |= flagMask(ItemFlag::MultiUse);
				return true;
			case DatFlagFluidContainer:
				fragment.group = ITEM_GROUP_FLUID;
				return true;
			case DatFlagSplash:
				fragment.group = ITEM_GROUP_SPLASH;
				return true;
			case DatFlagNotWalkable:
				fragment.flags |= flagMask(ItemFlag::Unpassable);
				return true;
			case DatFlagNotMoveable:
				fragment.flags &= ~flagMask(ItemFlag::Moveable);
				return true;
			case DatFlagBlockProjectile:
				fragment.flags |= flagMask(ItemFlag::BlockMissiles);
				return true;
			case DatFlagNotPathable:
				fragment.flags |= flagMask(ItemFlag::BlockPathfinder);
				return true;
			case DatFlagPickupable:
				fragment.flags |= flagMask(ItemFlag::Pickupable);
				return true;
			case DatFlagHangable:
				fragment.flags |= flagMask(ItemFlag::IsHangable);
				return true;
			case DatFlagHookSouth:
				fragment.flags |= flagMask(ItemFlag::HookSouth);
				return true;
			case DatFlagHookEast:
				fragment.flags |= flagMask(ItemFlag::HookEast);
				return true;
			case DatFlagRotateable:
				fragment.flags |= flagMask(ItemFlag::Rotatable);
				return true;
			case DatFlagDontHide:
				fragment.flags |= flagMask(ItemFlag::DontHide);
				return true;
			case DatFlagTranslucent:
				fragment.flags |= flagMask(ItemFlag::Translucent);
				return true;
			case DatFlagLyingCorpse:
				fragment.flags |= flagMask(ItemFlag::Corpse);
				return true;
			case DatFlagAnimateAlways:
				fragment.flags |= flagMask(ItemFlag::AnimateAlways);
				return true;
			case DatFlagLook:
			case DatFlagDefault:
				return true;
			case DatFlagWrappable:
				fragment.flags |= flagMask(ItemFlag::Wrappable);
				return true;
			case DatFlagUnwrappable:
				fragment.flags |= flagMask(ItemFlag::Unwrappable);
				return true;
			case DatFlagTopEffect:
				fragment.flags |= flagMask(ItemFlag::TopEffect);
				return true;
			case DatFlagNoMoveAnimation:
				fragment.flags |= flagMask(ItemFlag::NoMoveAnimation);
                return true;
			case DatFlagFullGround:
				fragment.flags |= flagMask(ItemFlag::FullTile);
				return true;
			case DatFlagFloorChange:
				fragment.flags |= flagMask(ItemFlag::FloorChange);
				return true;
			case DatFlagChargeable:
				fragment.flags |= flagMask(ItemFlag::ClientChargeable);
				return true;
			case DatFlagGround:
				fragment.group = ITEM_GROUP_GROUND;
				return file.getU16(fragment.way_speed);
			case DatFlagWritable:
			case DatFlagWritableOnce:
				fragment.flags |= flagMask(ItemFlag::CanReadText) | flagMask(ItemFlag::CanWriteText);
				return file.skip(2);
			case DatFlagCloth:
				return file.skip(2);
			case DatFlagLensHelp:
				fragment.flags |= flagMask(ItemFlag::LensHelp);
				return file.skip(2);
			case DatFlagUsable:
				fragment.flags |= flagMask(ItemFlag::ForceUse);
				fragment.flags |= flagMask(ItemFlag::Usable);
				return file.skip(2);
			case DatFlagLight: {
				uint16_t intensity = 0;
				uint16_t color = 0;
				if (!file.getU16(intensity) || !file.getU16(color)) {
					return false;
				}
				constexpr auto kMaxSpriteLightValue = static_cast<uint16_t>(std::numeric_limits<uint8_t>::max());
				if (intensity > kMaxSpriteLightValue || color > kMaxSpriteLightValue) {
					warnings.push_back(std::format(
						"DAT catalog: light values intensity={} color={} exceed SpriteLight range for client id {}. Capping to {}.",
						intensity,
						color,
						entry.client_id,
						kMaxSpriteLightValue));
				}
				const auto capped_intensity = std::min(intensity, kMaxSpriteLightValue);
				const auto capped_color = std::min(color, kMaxSpriteLightValue);
				entry.has_light = true;
				entry.light = SpriteLight { static_cast<uint8_t>(capped_intensity), static_cast<uint8_t>(capped_color) };
				return true;
			}
			case DatFlagDisplacement:
				if (format >= DAT_FORMAT_755) {
					return file.getU16(entry.drawoffset_x) && file.getU16(entry.drawoffset_y);
				}
				entry.drawoffset_x = 8;
				entry.drawoffset_y = 8;
				return true;
			case DatFlagElevation:
				fragment.flags |= flagMask(ItemFlag::HasElevation);
				return file.getU16(entry.draw_height);
			case DatFlagMinimapColor:
				return file.getU16(entry.minimap_color);
			case DatFlagMarket: {
				if (!file.skip(6)) {
					return false;
				}
				std::string market_name;
				if (!file.getString(market_name)) {
					return false;
				}
				return file.skip(4);
			}
			case DatFlagWings:
				return file.skip(16);
			default:
				error = wxString::FromUTF8(std::format("DAT catalog: unknown flag {} after {}.", static_cast<int>(flag), static_cast<int>(previous_flag)));
				return false;
		}
	}

	bool readFlags(DatFormat format, FileReadHandle& file, DatCatalogEntry& entry, wxString& error, std::vector<std::string>& warnings) {
		uint8_t flag = 0xFF;
		uint8_t previous_flag = 0xFF;

		entry.item_fragment.flags = flagMask(ItemFlag::Moveable) | flagMask(ItemFlag::Replaceable);

		for (int count = 0; count < static_cast<int>(DatFlagLast); ++count) {
			previous_flag = flag;
			if (!file.getU8(flag)) {
				return false;
			}
			if (flag == DatFlagLast) {
				return true;
			}

			flag = remapFlag(flag, format);
			if (!readFlagPayload(format, file, entry, flag, previous_flag, error, warnings)) {
				return false;
			}
		}

		warnings.push_back(std::format("DAT catalog: flag list exceeded limit for client id {}.", entry.client_id));
		return true;
	}

	bool readFrameMetadata(const DatCatalog& catalog, FileReadHandle& file, DatCatalogEntry& entry, uint8_t frames, uint32_t group_index) {
		if (frames <= 1) {
			return true;
		}

		auto animation = DatAnimationInfo {};
		if (catalog.has_frame_durations) {
			uint8_t async = 0;
			if (!file.getByte(async) || !file.get32(animation.loop_count) || !file.getSByte(animation.start_frame)) {
				return false;
			}
			animation.asynchronous = async == 1;
			animation.frame_durations.reserve(frames);
			for (uint8_t frame_index = 0; frame_index < frames; ++frame_index) {
				DatAnimationFrameDuration duration;
				if (!file.getU32(duration.minimum) || !file.getU32(duration.maximum)) {
					return false;
				}
				if (group_index == 0) {
					animation.frame_durations.push_back(duration);
				}
			}
		}

		if (group_index == 0) {
			entry.animation = std::move(animation);
		}
		return true;
	}

	bool readSpriteIds(DatCatalog& catalog, FileReadHandle& file, DatCatalogEntry& entry, uint32_t sprite_count, uint32_t group_index, std::vector<std::string>& warnings) {
		if (group_index == 0) {
			entry.sprite_ids.clear();
			entry.sprite_ids.reserve(sprite_count);
		}

		for (uint32_t sprite_index = 0; sprite_index < sprite_count; ++sprite_index) {
			uint32_t sprite_id = 0;
			if (catalog.is_extended) {
				if (!file.getU32(sprite_id)) {
					return false;
				}
			} else {
				uint16_t compact_id = 0;
				if (!file.getU16(compact_id)) {
					return false;
				}
				sprite_id = compact_id;
			}
			if (sprite_id >= MAX_SPRITES) {
				warnings.push_back(std::format(
					"DAT catalog: sprite id {} exceeds MAX_SPRITES={} for client id {}.",
					sprite_id,
					MAX_SPRITES,
					entry.client_id));
				return false;
			}

			if (group_index != 0) {
				continue;
			}

			entry.sprite_ids.push_back(sprite_id);
			catalog.max_sprite_id = std::max(catalog.max_sprite_id, sprite_id);
		}
		return true;
	}

	bool readSpriteGroup(DatCatalog& catalog, FileReadHandle& file, DatCatalogEntry& entry, uint32_t group_index, std::vector<std::string>& warnings) {
		if (catalog.has_frame_groups && entry.client_id > catalog.item_count && !file.skip(1)) {
			return false;
		}

		uint8_t width = 0;
		uint8_t height = 0;
		uint8_t layers = 0;
		uint8_t pattern_x = 0;
		uint8_t pattern_y = 0;
		uint8_t pattern_z = 0;
		uint8_t frames = 0;

		if (!file.getByte(width) || !file.getByte(height)) {
			return false;
		}
		if ((width > 1 || height > 1) && !file.skip(1)) {
			return false;
		}
		if (!file.getU8(layers) || !file.getU8(pattern_x) || !file.getU8(pattern_y)) {
			return false;
		}
		if (catalog.format <= DAT_FORMAT_74) {
			pattern_z = 1;
		} else if (!file.getU8(pattern_z)) {
			return false;
		}
		if (!file.getU8(frames)) {
			return false;
		}

		if (!readFrameMetadata(catalog, file, entry, frames, group_index)) {
			return false;
		}

		if (width == 0 || height == 0 || layers == 0 || pattern_x == 0 || pattern_y == 0 || pattern_z == 0 || frames == 0) {
			std::string zero_dimensions;
			const auto append_zero_dimension = [&](bool is_zero, const char* name) {
				if (!is_zero) {
					return;
				}
				if (!zero_dimensions.empty()) {
					zero_dimensions += ", ";
				}
				zero_dimensions += name;
			};
			append_zero_dimension(width == 0, "width");
			append_zero_dimension(height == 0, "height");
			append_zero_dimension(layers == 0, "layers");
			append_zero_dimension(pattern_x == 0, "pattern_x");
			append_zero_dimension(pattern_y == 0, "pattern_y");
			append_zero_dimension(pattern_z == 0, "pattern_z");
			append_zero_dimension(frames == 0, "frames");
			warnings.push_back(std::format(
				"DAT catalog: sprite group for client id {} has zero dimension(s): {}.",
				entry.client_id,
				zero_dimensions));
			return false;
		}

		if (width > MAX_SPRITE_PARTS || height > MAX_SPRITE_PARTS) {
			warnings.push_back(std::format(
				"DAT catalog: sprite group for client id {} has dimension(s) exceeding MAX_SPRITE_PARTS={}: width={}, height={}.",
				entry.client_id,
				MAX_SPRITE_PARTS,
				width,
				height));
			return false;
		}

		const uint64_t sprite_count_64 = static_cast<uint64_t>(width) * static_cast<uint64_t>(height) * static_cast<uint64_t>(layers) *
			static_cast<uint64_t>(pattern_x) * static_cast<uint64_t>(pattern_y) * static_cast<uint64_t>(pattern_z) * static_cast<uint64_t>(frames);
		if (sprite_count_64 > static_cast<uint64_t>(MAX_SPRITES)) {
			warnings.push_back(std::format(
				"DAT catalog: sprite group for client id {} expands to {} sprites which exceeds MAX_SPRITES={}.",
				entry.client_id,
				sprite_count_64,
				MAX_SPRITES));
			return false;
		}
		const auto sprite_count = static_cast<uint32_t>(sprite_count_64);

		if (group_index == 0) {
			entry.width = width;
			entry.height = height;
			entry.layers = layers;
			entry.pattern_x = pattern_x;
			entry.pattern_y = pattern_y;
			entry.pattern_z = pattern_z;
			entry.frames = frames;
			entry.numsprites = sprite_count;
		}

		return readSpriteIds(catalog, file, entry, sprite_count, group_index, warnings);
	}
}

void DatItemParser::appendFragments(const DatCatalog& catalog, ItemDefinitionFragments& fragments) {
	for (uint32_t client_id = 100; client_id <= catalog.item_count; ++client_id) {
		const auto* entry = catalog.entry(client_id);
		if (!entry) {
			continue;
		}
		fragments.dat[static_cast<ClientItemId>(client_id)] = entry->item_fragment;
	}
}

bool DatItemParser::parse(const ItemDefinitionLoadInput& input, ItemDefinitionFragments& fragments, wxString& error, std::vector<std::string>& warnings) const {
	if (input.dat_catalog != nullptr) {
		appendFragments(*input.dat_catalog, fragments);
		if (fragments.dat.empty()) {
			error = "No item definitions were available in the DAT catalog.";
			return false;
		}
		return true;
	}

	DatCatalog catalog;
	if (!parseCatalog(input, catalog, error, warnings)) {
		return false;
	}

	appendFragments(catalog, fragments);
	if (fragments.dat.empty()) {
		error = "No item definitions were parsed from the DAT file.";
		return false;
	}
	return true;
}

bool DatItemParser::parseCatalog(const ItemDefinitionLoadInput& input, DatCatalog& catalog, wxString& error, std::vector<std::string>& warnings) const {
	if (input.client_version == nullptr) {
		error = "DAT parser requires a client version.";
		return false;
	}
	if (input.dat_path.GetFullPath().IsEmpty()) {
		error = "DAT parser requires a dat path.";
		return false;
	}

	FileReadHandle file(nstr(input.dat_path.GetFullPath()));
	if (!file.isOk()) {
		error = wxstr(std::format("Failed to open {} for reading\nThe error reported was: {}", input.dat_path.GetFullPath().utf8_string(), file.getErrorMessage()));
		return false;
	}

	catalog = {};
	catalog.is_extended = input.client_version->isExtended();
	catalog.has_transparency = input.client_version->isTransparent();
	catalog.has_frame_durations = input.client_version->hasFrameDurations();
	catalog.has_frame_groups = input.client_version->hasFrameGroups();

	if (!file.getU32(catalog.signature)) {
		error = "Failed to read DAT signature.";
		return false;
	}
	if (!file.getU16(catalog.item_count) || !file.getU16(catalog.creature_count) || !file.getU16(catalog.effect_count) || !file.getU16(catalog.distance_count)) {
		error = "Failed to read DAT header counts.";
		return false;
	}
	if (catalog.item_count < 100 || catalog.creature_count == 0) {
		error = "Invalid DAT header counts.";
		return false;
	}

	catalog.format = input.client_version->getDatFormatForSignature(catalog.signature);
	if (catalog.format == DAT_FORMAT_UNKNOWN) {
		error = wxstr(std::format("Unknown dat signature: {:#x}", catalog.signature));
		return false;
	}

	catalog.entries.clear();
	catalog.entries.resize(static_cast<size_t>(catalog.lastEntryId()) + 1);

	for (uint32_t client_id = 100; client_id <= catalog.lastEntryId(); ++client_id) {
		auto& entry = catalog.entries[client_id];
		entry = {};
		entry.client_id = client_id;
		entry.item_fragment.client_id = static_cast<ClientItemId>(client_id);

		if (!readFlags(catalog.format, file, entry, error, warnings)) {
			if (error.empty()) {
				error = wxstr(std::format("Failed to read DAT flags for client id {}", client_id));
			}
			return false;
		}

		uint8_t group_count = 1;
		if (catalog.has_frame_groups && client_id > catalog.item_count && !file.getU8(group_count)) {
			error = wxstr(std::format("Failed to read DAT frame-group count for client id {}", client_id));
			return false;
		}
		if (group_count == 0) {
			error = wxstr(std::format("Invalid DAT frame-group count for client id {}", client_id));
			return false;
		}

		for (uint32_t group_index = 0; group_index < group_count; ++group_index) {
			if (!readSpriteGroup(catalog, file, entry, group_index, warnings)) {
				error = wxstr(std::format("Failed to read DAT sprite group {} for client id {}", group_index, client_id));
				return false;
			}
		}
	}

	if (catalog.item_count == 0 || catalog.entries.size() <= 100 || !catalog.entry(100)) {
		error = "No DAT entries were parsed.";
		return false;
	}

	catalog.sprite_dimensions.assign(static_cast<size_t>(catalog.max_sprite_id) + 1, DatSpriteDimensions {});
	return true;
}
