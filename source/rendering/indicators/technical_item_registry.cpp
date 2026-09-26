#include "rendering/indicators/technical_item_registry.h"
#include "brushes/brush.h"
#include "brushes/raw/raw_brush.h"
#include "brushes/spawn/spawn_brush.h"
#include "brushes/waypoint/waypoint_brush.h"
#include "brushes/house/house_exit_brush.h"

namespace rme::rendering {

TileIndicatorType TechnicalItemRegistry::GetBrushIndicatorType(const Brush* brush) {
	if (!brush) {
		return TileIndicatorType::None;
	}
	if (brush->is<SpawnBrush>()) {
		return TileIndicatorType::Spawn;
	}
	if (brush->is<WaypointBrush>()) {
		return TileIndicatorType::Waypoint;
	}
	if (brush->is<HouseExitBrush>()) {
		return TileIndicatorType::HouseEntry;
	}
	if (const auto* raw = dynamic_cast<const RAWBrush*>(brush)) {
		uint16_t s_id = raw->getItemID();
		uint16_t c_id = static_cast<uint16_t>(raw->getLookID());
		auto tech = Classify(s_id, c_id);
		if (tech != TileIndicatorType::None) {
			return tech;
		}
	}
	int look_id = brush->getLookID();
	if (look_id > 0) {
		auto tech = Classify(static_cast<uint16_t>(look_id), static_cast<uint16_t>(look_id));
		if (tech != TileIndicatorType::None) {
			return tech;
		}
	}
	const std::string& bname = brush->getName();
	if (bname == "stairs" || bname == "invisible stairs" || bname == "stair") {
		return TileIndicatorType::TechInvisibleStair;
	}
	return TileIndicatorType::None;
}

} // namespace rme::rendering
