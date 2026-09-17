#include "item_definitions/formats/protobuf/protobuf_item_parser.h"

#include "appearances.pb.h"
#include "app/client_version.h"
#include "app/definitions.h"
#include "util/json.h"

#include <fstream>
#include <format>
#include <limits>

namespace {
	using namespace canary::protobuf::appearances;

	constexpr uint64_t flagMask(ItemFlag flag) {
		return uint64_t { 1 } << static_cast<uint8_t>(flag);
	}

	void setMappedFlag(uint64_t& flags, bool enabled, ItemFlag flag) {
		if (enabled) {
			flags |= flagMask(flag);
		}
	}

	const FrameGroup* selectFrameGroup(const Appearance& appearance) {
		const FrameGroup* fallback = nullptr;
		for (const auto& frame_group : appearance.frame_group()) {
			if (!frame_group.has_sprite_info()) {
				continue;
			}
			if (!fallback) {
				fallback = &frame_group;
			}

			if (frame_group.fixed_frame_group() == FIXED_FRAME_GROUP_OBJECT_INITIAL || frame_group.fixed_frame_group() == FIXED_FRAME_GROUP_OUTFIT_IDLE) {
				return &frame_group;
			}
		}
		return fallback;
	}

	std::string buildPassiveMetadataJson(const AppearanceFlags& flags) {
		json::json metadata = json::json::object();

		const auto addBool = [&](const char* key, bool value) {
			if (value) {
				metadata[key] = true;
			}
		};

		addBool("usable", flags.usable());
		addBool("forceUse", flags.forceuse());
		addBool("multiUse", flags.multiuse());
		addBool("noMovementAnimation", flags.no_movement_animation());
		addBool("dontHide", flags.dont_hide());
		addBool("translucent", flags.translucent());
		addBool("lyingObject", flags.lying_object());
		addBool("animateAlways", flags.animate_always());
		addBool("fullBank", flags.fullbank());
		addBool("wrap", flags.wrap());
		addBool("unwrap", flags.unwrap());
		addBool("topEffect", flags.topeffect());
		addBool("corpse", flags.corpse());
		addBool("playerCorpse", flags.player_corpse());
		addBool("ammo", flags.ammo());
		addBool("reportable", flags.reportable());

		if (flags.has_bank()) {
			metadata["bank"] = json::json::object({ { "waypoints", flags.bank().waypoints() } });
		}
		if (flags.has_write()) {
			metadata["write"] = json::json::object({ { "maxTextLength", flags.write().max_text_length() } });
		}
		if (flags.has_write_once()) {
			metadata["writeOnce"] = json::json::object({ { "maxTextLengthOnce", flags.write_once().max_text_length_once() } });
		}
		if (flags.has_clothes()) {
			metadata["clothes"] = json::json::object({ { "slot", flags.clothes().slot() } });
		}
		if (flags.has_default_action()) {
			metadata["defaultAction"] = json::json::object({ { "action", flags.default_action().action() } });
		}
		if (flags.has_market()) {
			json::json market = json::json::object();
			market["category"] = flags.market().category();
			market["tradeAsObjectId"] = flags.market().trade_as_object_id();
			market["showAsObjectId"] = flags.market().show_as_object_id();
			if (!flags.market().name().empty()) {
				market["name"] = flags.market().name();
			}
			if (flags.market().minimum_level() != 0) {
				market["minimumLevel"] = flags.market().minimum_level();
			}
			if (flags.market().restrict_to_profession_size() > 0) {
				json::json professions = json::json::array();
				for (const auto profession : flags.market().restrict_to_profession()) {
					professions.push_back(profession);
				}
				market["restrictToProfession"] = std::move(professions);
			}
			metadata["market"] = std::move(market);
		}
		if (flags.npcsaledata_size() > 0) {
			json::json npc_sales = json::json::array();
			for (const auto& sale_entry : flags.npcsaledata()) {
				json::json row = json::json::object();
				if (!sale_entry.name().empty()) {
					row["name"] = sale_entry.name();
				}
				if (!sale_entry.location().empty()) {
					row["location"] = sale_entry.location();
				}
				if (sale_entry.sale_price() != 0) {
					row["salePrice"] = sale_entry.sale_price();
				}
				if (sale_entry.buy_price() != 0) {
					row["buyPrice"] = sale_entry.buy_price();
				}
				if (sale_entry.currency_object_type_id() != 0) {
					row["currencyObjectTypeId"] = sale_entry.currency_object_type_id();
				}
				if (!sale_entry.currency_quest_flag_display_name().empty()) {
					row["currencyQuestFlagDisplayName"] = sale_entry.currency_quest_flag_display_name();
				}
				npc_sales.push_back(std::move(row));
			}
			metadata["npcSaleData"] = std::move(npc_sales);
		}
		if (flags.has_changedtoexpire()) {
			metadata["changedToExpire"] = json::json::object({ { "formerObjectTypeId", flags.changedtoexpire().former_object_typeid() } });
		}
		if (flags.has_cyclopediaitem()) {
			metadata["cyclopediaItem"] = json::json::object({ { "cyclopediaType", flags.cyclopediaitem().cyclopedia_type() } });
		}
		if (flags.has_upgradeclassification()) {
			metadata["upgradeClassification"] = json::json::object({ { "value", flags.upgradeclassification().upgrade_classification() } });
		}
		if (flags.has_lenshelp()) {
			metadata["lensHelp"] = json::json::object({ { "id", flags.lenshelp().id() } });
		}

		return metadata.empty() ? std::string {} : metadata.dump();
	}

	void applyCommonFlags(const AppearanceFlags& flags, DatCatalogEntry& entry) {
		auto& fragment = entry.item_fragment;
		fragment.flags = flagMask(ItemFlag::Replaceable);
		if (!flags.unmove()) {
			fragment.flags |= flagMask(ItemFlag::Moveable);
		}

		if (flags.clip() || flags.top() || flags.bottom()) {
			fragment.flags |= flagMask(ItemFlag::AlwaysOnBottom);
		}
		if (flags.clip()) {
			fragment.flags |= flagMask(ItemFlag::IsBorder);
			fragment.always_on_top_order = 1;
		} else if (flags.bottom()) {
			fragment.always_on_top_order = 2;
		} else if (flags.top()) {
			fragment.always_on_top_order = 3;
		}

		if (flags.container()) {
			fragment.group = ITEM_GROUP_CONTAINER;
			fragment.type = ITEM_TYPE_CONTAINER;
		} else if (flags.liquidcontainer()) {
			fragment.group = ITEM_GROUP_FLUID;
		} else if (flags.liquidpool()) {
			fragment.group = ITEM_GROUP_SPLASH;
		} else if (flags.has_bank()) {
			fragment.group = ITEM_GROUP_GROUND;
		}

		if (flags.unpass()) {
			fragment.flags |= flagMask(ItemFlag::Unpassable);
		}
		if (flags.unsight()) {
			fragment.flags |= flagMask(ItemFlag::BlockMissiles);
		}
		if (flags.avoid()) {
			fragment.flags |= flagMask(ItemFlag::BlockPathfinder);
		}
		if (flags.take()) {
			fragment.flags |= flagMask(ItemFlag::Pickupable);
		}
		if (flags.hang()) {
			fragment.flags |= flagMask(ItemFlag::IsHangable);
		}
		if (flags.rotate()) {
			fragment.flags |= flagMask(ItemFlag::Rotatable);
		}
		if (flags.ignore_look()) {
			fragment.flags |= flagMask(ItemFlag::IgnoreLook);
		}
		if (flags.cumulative()) {
			fragment.flags |= flagMask(ItemFlag::Stackable);
		}
		if (flags.has_write() || flags.has_write_once()) {
			fragment.flags |= flagMask(ItemFlag::CanReadText) | flagMask(ItemFlag::CanWriteText);
		}
		if (flags.has_write()) {
			fragment.max_text_len = static_cast<uint16_t>(std::min<uint32_t>(flags.write().max_text_length(), std::numeric_limits<uint16_t>::max()));
		}
		if (flags.has_write_once()) {
			const auto max_length_once = static_cast<uint16_t>(std::min<uint32_t>(flags.write_once().max_text_length_once(), std::numeric_limits<uint16_t>::max()));
			fragment.max_text_len = std::max(fragment.max_text_len.value_or(uint16_t { 0 }), max_length_once);
		}
		if (flags.has_height()) {
			fragment.flags |= flagMask(ItemFlag::HasElevation);
		}
		if (flags.has_hook()) {
			if (flags.hook().direction() == HOOK_TYPE_SOUTH) {
				fragment.flags |= flagMask(ItemFlag::HookSouth);
			} else if (flags.hook().direction() == HOOK_TYPE_EAST) {
				fragment.flags |= flagMask(ItemFlag::HookEast);
			}
		}
		if (flags.has_clothes()) {
			fragment.slot_position = static_cast<uint16_t>(std::min<uint32_t>(flags.clothes().slot(), std::numeric_limits<uint16_t>::max()));
		}

		setMappedFlag(fragment.flags, flags.forceuse(), ItemFlag::ForceUse);
		setMappedFlag(fragment.flags, flags.multiuse(), ItemFlag::MultiUse);
		setMappedFlag(fragment.flags, flags.usable(), ItemFlag::Usable);
		setMappedFlag(fragment.flags, flags.no_movement_animation(), ItemFlag::NoMoveAnimation);
		setMappedFlag(fragment.flags, flags.animate_always(), ItemFlag::AnimateAlways);
		setMappedFlag(fragment.flags, flags.dont_hide(), ItemFlag::DontHide);
		setMappedFlag(fragment.flags, flags.translucent(), ItemFlag::Translucent);
		setMappedFlag(fragment.flags, flags.wrap(), ItemFlag::Wrappable);
		setMappedFlag(fragment.flags, flags.unwrap(), ItemFlag::Unwrappable);
		setMappedFlag(fragment.flags, flags.topeffect(), ItemFlag::TopEffect);
		setMappedFlag(fragment.flags, flags.corpse() || flags.player_corpse(), ItemFlag::Corpse);
		setMappedFlag(fragment.flags, flags.ammo(), ItemFlag::Ammo);
		setMappedFlag(fragment.flags, flags.reportable(), ItemFlag::Reportable);

		entry.draw_height = flags.has_height() ? static_cast<uint16_t>(flags.height().elevation()) : 0;
		entry.minimap_color = flags.has_automap() ? static_cast<uint16_t>(flags.automap().color()) : 0;
		if (flags.has_shift()) {
			entry.drawoffset_x = static_cast<uint16_t>(flags.shift().x());
			entry.drawoffset_y = static_cast<uint16_t>(flags.shift().y());
		}
		if (flags.has_light()) {
			entry.has_light = true;
			entry.light.intensity = static_cast<uint8_t>(std::min<uint32_t>(flags.light().brightness(), 255));
			entry.light.color = static_cast<uint8_t>(std::min<uint32_t>(flags.light().color(), 255));
		}

		fragment.passive_metadata_json = buildPassiveMetadataJson(flags);
	}

	void applyAnimation(const SpriteAnimation& animation, DatCatalogEntry& entry) {
		if (animation.sprite_phase_size() == 0) {
			return;
		}

		DatAnimationInfo animation_info;
		animation_info.asynchronous = !animation.synchronized();
		animation_info.loop_count = static_cast<int>(animation.loop_count());
		animation_info.start_frame = static_cast<int8_t>(animation.default_start_phase());
		animation_info.frame_durations.reserve(animation.sprite_phase_size());
		for (const auto& phase : animation.sprite_phase()) {
			animation_info.frame_durations.push_back(DatAnimationFrameDuration {
				.minimum = phase.duration_min(),
				.maximum = phase.duration_max(),
			});
		}
		entry.animation = std::move(animation_info);
	}

	bool fillEntry(const Appearance& appearance, uint32_t client_id, DatCatalogEntry& entry, DatCatalog& catalog, bool apply_item_flags) {
		entry = {};
		entry.client_id = client_id;
		entry.item_fragment.client_id = static_cast<ClientItemId>(client_id);
		entry.width = 1;
		entry.height = 1;
		entry.pattern_x = 1;
		entry.pattern_y = 1;
		entry.pattern_z = 1;
		entry.layers = 1;
		entry.frames = 1;

		const FrameGroup* frame_group = selectFrameGroup(appearance);
		if (!frame_group || !frame_group->has_sprite_info()) {
			return false;
		}

		const auto& sprite_info = frame_group->sprite_info();
		entry.pattern_x = static_cast<uint8_t>(std::max<uint32_t>(1, sprite_info.pattern_width()));
		entry.pattern_y = static_cast<uint8_t>(std::max<uint32_t>(1, sprite_info.pattern_height()));
		entry.pattern_z = static_cast<uint8_t>(std::max<uint32_t>(1, sprite_info.pattern_depth()));
		entry.layers = static_cast<uint8_t>(std::max<uint32_t>(1, sprite_info.layers()));
		entry.frames = static_cast<uint8_t>(std::max<int>(1, sprite_info.animation().sprite_phase_size()));
		entry.numsprites = static_cast<uint32_t>(sprite_info.sprite_id_size());
		entry.sprite_ids.assign(sprite_info.sprite_id().begin(), sprite_info.sprite_id().end());
		for (uint32_t sprite_id : entry.sprite_ids) {
			catalog.max_sprite_id = std::max(catalog.max_sprite_id, sprite_id);
		}

		if (sprite_info.has_animation()) {
			applyAnimation(sprite_info.animation(), entry);
		}

		if (appearance.has_flags()) {
			if (appearance.flags().has_shift()) {
				entry.drawoffset_x = static_cast<uint16_t>(appearance.flags().shift().x());
				entry.drawoffset_y = static_cast<uint16_t>(appearance.flags().shift().y());
			}
		}

		if (appearance.has_flags() && apply_item_flags) {
			applyCommonFlags(appearance.flags(), entry);
			if (appearance.flags().show_off_socket()) {
				entry.item_fragment.type = ITEM_TYPE_PODIUM;
				entry.item_fragment.group = ITEM_GROUP_PODIUM;
			}
		}

		return entry.numsprites > 0;
	}
}

void ProtobufItemParser::appendFragments(const DatCatalog& catalog, ItemDefinitionFragments& fragments) {
	for (uint32_t client_id = 100; client_id <= catalog.item_count; ++client_id) {
		const auto* entry = catalog.entry(client_id);
		if (!entry) {
			continue;
		}
		fragments.dat[static_cast<ClientItemId>(client_id)] = entry->item_fragment;
	}
}

bool ProtobufItemParser::parse(const ItemDefinitionLoadInput& input, ItemDefinitionFragments& fragments, wxString& error, std::vector<std::string>& warnings) const {
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
		error = "No item definitions were parsed from the protobuf appearances file.";
		return false;
	}
	return true;
}

bool ProtobufItemParser::parseCatalog(const ItemDefinitionLoadInput& input, DatCatalog& catalog, wxString& error, std::vector<std::string>& warnings) const {
	if (input.dat_path.GetFullPath().IsEmpty()) {
		error = "Protobuf parser requires an appearances file path.";
		return false;
	}

	std::ifstream stream(input.dat_path.GetFullPath().ToStdString(), std::ios::in | std::ios::binary);
	if (!stream.is_open()) {
		error = wxString::FromUTF8(std::format("Failed to open {} for reading.", input.dat_path.GetFullPath().utf8_string()));
		return false;
	}

	Appearances appearances;
	if (!appearances.ParseFromIstream(&stream)) {
		error = "Failed to parse protobuf appearances file.";
		return false;
	}

	uint32_t item_count = 0;
	for (const auto& object : appearances.object()) {
		item_count = std::max(item_count, object.id());
	}

	uint32_t creature_count = 0;
	for (const auto& outfit : appearances.outfit()) {
		creature_count = std::max(creature_count, outfit.id());
	}

	if (item_count < 100) {
		error = "The protobuf appearances file does not contain a valid item id range.";
		return false;
	}
	if (item_count > std::numeric_limits<uint16_t>::max() || creature_count > std::numeric_limits<uint16_t>::max()) {
		error = "The protobuf appearances file contains ids outside the supported catalog range.";
		return false;
	}

	catalog = {};
	catalog.format = DAT_FORMAT_1057;
	catalog.is_extended = true;
	catalog.has_transparency = true;
	catalog.has_frame_durations = true;
	catalog.has_frame_groups = true;
	catalog.item_count = static_cast<uint16_t>(std::min<uint32_t>(item_count, std::numeric_limits<uint16_t>::max()));

	catalog.creature_count = static_cast<uint16_t>(std::min<uint32_t>(creature_count, std::numeric_limits<uint16_t>::max()));
	catalog.entries.resize(static_cast<size_t>(item_count + creature_count) + 1);

	for (const auto& object : appearances.object()) {
		if (!object.has_id()) {
			warnings.push_back("Skipping protobuf object appearance without an id.");
			continue;
		}

		auto& entry = catalog.entries[object.id()];
		if (!fillEntry(object, object.id(), entry, catalog, true)) {
			entry = {};
			warnings.push_back(std::format("Skipping protobuf object appearance {} because it has no sprite data.", object.id()));
		}
	}

	for (const auto& outfit : appearances.outfit()) {
		if (!outfit.has_id()) {
			warnings.push_back("Skipping protobuf outfit appearance without an id.");
			continue;
		}

		const uint32_t client_id = item_count + outfit.id();
		auto& entry = catalog.entries[client_id];
		if (!fillEntry(outfit, client_id, entry, catalog, false)) {
			entry = {};
			warnings.push_back(std::format("Skipping protobuf outfit appearance {} because it has no sprite data.", outfit.id()));
		}
	}

	if (catalog.max_sprite_id >= MAX_SPRITES) {
		error = wxString::FromUTF8(std::format(
			"Protobuf catalog references sprite id {} which exceeds MAX_SPRITES={}.",
			catalog.max_sprite_id,
			MAX_SPRITES));
		return false;
	}
	catalog.sprite_dimensions.assign(static_cast<size_t>(catalog.max_sprite_id) + 1, DatSpriteDimensions {});
	return true;
}
