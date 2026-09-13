#ifndef RME_DOOR_INDICATOR_DRAWER_H_
#define RME_DOOR_INDICATOR_DRAWER_H_

#include <vector>
#include "map/position.h"

struct NVGcontext;
struct RenderView;

class DoorIndicatorDrawer {
public:
	DoorIndicatorDrawer();
	~DoorIndicatorDrawer();

	struct DoorRequest {
		Position pos;
		bool locked;
		bool south;
		bool east;
	};

	void addDoor(const Position& pos, bool locked, bool south, bool east);
	void clear();
	bool empty() const noexcept {
		return requests.empty();
	}
	void draw(NVGcontext* vg, const RenderView& view);

private:
	std::vector<DoorRequest> requests;
};

#endif
