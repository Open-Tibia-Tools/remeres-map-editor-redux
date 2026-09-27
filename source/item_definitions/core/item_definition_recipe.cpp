#include "item_definitions/core/item_definition_recipe.h"

const ItemDefinitionRecipe& ItemDefinitionRecipeRegistry::get(ItemDefinitionMode mode) {
	static const ItemDefinitionRecipe dat_otb {
		.mode = ItemDefinitionMode::DatOtb,
		.sources = { ItemDefinitionSourceKind::Dat, ItemDefinitionSourceKind::Otb, ItemDefinitionSourceKind::Xml },
		.source_count = 3,
		.runnable = true,
	};
	static const ItemDefinitionRecipe dat_only {
		.mode = ItemDefinitionMode::DatOnly,
		.sources = { ItemDefinitionSourceKind::Dat, ItemDefinitionSourceKind::Xml, ItemDefinitionSourceKind::Xml },
		.source_count = 2,
		.runnable = true,
	};
	static const ItemDefinitionRecipe dat_srv {
		.mode = ItemDefinitionMode::DatSrv,
		.sources = { ItemDefinitionSourceKind::Dat, ItemDefinitionSourceKind::Srv, ItemDefinitionSourceKind::Xml },
		.source_count = 3,
		.runnable = false,
	};
	static const ItemDefinitionRecipe protobuf_otb {
		.mode = ItemDefinitionMode::ProtobufOtb,
		.sources = { ItemDefinitionSourceKind::Protobuf, ItemDefinitionSourceKind::Otb, ItemDefinitionSourceKind::Xml },
		.source_count = 3,
		.runnable = true,
	};
	static const ItemDefinitionRecipe protobuf_only {
		.mode = ItemDefinitionMode::ProtobufOnly,
		.sources = { ItemDefinitionSourceKind::Protobuf, ItemDefinitionSourceKind::Xml, ItemDefinitionSourceKind::Xml },
		.source_count = 2,
		.runnable = true,
	};
	static const ItemDefinitionRecipe dat_ron {
		.mode = ItemDefinitionMode::DatRon,
		.sources = { ItemDefinitionSourceKind::Dat, ItemDefinitionSourceKind::Ron, ItemDefinitionSourceKind::Ron },
		.source_count = 2,
		.runnable = true,
	};

	switch (mode) {
		case ItemDefinitionMode::DatOtb:
			return dat_otb;
		case ItemDefinitionMode::DatOnly:
			return dat_only;
		case ItemDefinitionMode::DatSrv:
			return dat_srv;
		case ItemDefinitionMode::ProtobufOtb:
			return protobuf_otb;
		case ItemDefinitionMode::ProtobufOnly:
			return protobuf_only;
		case ItemDefinitionMode::DatRon:
			return dat_ron;
		default:
			return dat_otb;
	}
}
