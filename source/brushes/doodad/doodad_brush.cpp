//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "app/definitions.h"

#include "brushes/doodad/doodad_brush.h"
#include "brushes/doodad/doodad_brush_loader.h"
#include "map/basemap.h"
#include "game/item.h"

//=============================================================================
// Doodad brush

DoodadBrush::DoodadBrush() {
}

DoodadBrush::~DoodadBrush() {
}

bool DoodadBrush::load(pugi::xml_node node, std::vector<std::string>& warnings) {
	return DoodadBrushLoader::load(node, items, settings, warnings, this);
}

void DoodadBrush::getRelatedItems(std::vector<uint16_t>& items_out) {
	for (const auto& alt : items.getAlternatives()) {
		if (!alt) {
			continue;
		}
		for (const auto& single : alt->single_items) {
			if (single.item && single.item->getID() != 0) {
				items_out.push_back(single.item->getID());
			}
		}
		for (const auto& composite : alt->composite_items) {
			for (const auto& entry : composite.items) {
				for (const auto& item : entry.second) {
					if (item && item->getID() != 0) {
						items_out.push_back(item->getID());
					}
				}
			}
		}
	}
}

bool DoodadBrush::ownsItem(Item* item) const {
	if (item->getDoodadBrush() == this) {
		return true;
	}
	return items.ownsItem(item->getID());
}

void DoodadBrush::undraw(BaseMap* map, Tile* tile) {
	// Remove all doodad-related
	std::erase_if(tile->items, [this](const std::unique_ptr<Item>& item) {
		if (item->getDoodadBrush() != nullptr) {
			if (item->isComplex() && g_settings.getInteger(Config::ERASER_LEAVE_UNIQUE)) {
				return false;
			}
			if (g_settings.getInteger(Config::DOODAD_BRUSH_ERASE_LIKE)) {
				return ownsItem(item.get());
			}
			return true;
		}
		return false;
	});

	if (tile->ground && tile->ground->getDoodadBrush() != nullptr) {
		if (g_settings.getInteger(Config::DOODAD_BRUSH_ERASE_LIKE)) {
			// Only delete items of the same doodad brush
			if (ownsItem(tile->ground.get())) {
				tile->ground.reset();
			}
		} else {
			tile->ground.reset();
		}
	}
}

void DoodadBrush::draw(BaseMap* map, Tile* tile, void* parameter) {
	int variation = 0;
	if (parameter) {
		variation = *reinterpret_cast<int*>(parameter);
	}

	Item* randomItem = items.getRandomSingleItem(variation);
	if (randomItem) {
		tile->addItem(randomItem->deepCopy());
	}

	if (settings.clear_mapflags || settings.clear_statflags) {
		tile->setMapFlags(tile->getMapFlags() & (~settings.clear_mapflags));
		tile->setStatFlags(tile->getStatFlags() & (~settings.clear_statflags));
	}
}

const CompositeTileList& DoodadBrush::getComposite(int variation) const {
	return items.getComposite(variation);
}

bool DoodadBrush::isEmpty(int variation) const {
	return items.isEmpty(variation, settings);
}

int DoodadBrush::getCompositeChance(int variation) const {
	return items.getCompositeChance(variation);
}

int DoodadBrush::getSingleChance(int variation) const {
	return items.getSingleChance(variation);
}

int DoodadBrush::getTotalChance(int variation) const {
	return items.getTotalChance(variation);
}

bool DoodadBrush::hasSingleObjects(int variation) const {
	return items.hasSingleObjects(variation);
}

bool DoodadBrush::hasCompositeObjects(int variation) const {
	return items.hasCompositeObjects(variation);
}
