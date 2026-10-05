//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_ZONE_BRUSH_H_
#define RME_ZONE_BRUSH_H_

#include "brushes/brush.h"

// Paints a Crystal Server zone id onto tiles; the active id is set by the zone palette
class ZoneBrush : public Brush {
public:
	ZoneBrush() = default;
	~ZoneBrush() override = default;

	bool canDraw(BaseMap* map, const Position& position) const override;
	void draw(BaseMap* map, Tile* tile, void* parameter) override;
	void undraw(BaseMap* map, Tile* tile) override;

	bool canDrag() const override {
		return true;
	}
	int getLookID() const override {
		return 0;
	}
	std::string getName() const override {
		return "Zone Brush";
	}

	void setZone(uint16_t id) noexcept {
		zone_id = id;
	}
	[[nodiscard]] uint16_t getZone() const noexcept {
		return zone_id;
	}

private:
	uint16_t zone_id = 0;
};

#endif
