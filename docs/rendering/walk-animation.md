# Outfit walk animation: frame groups, walk progress and the frame picker

## 1. Pattern & Key Files

- `source/rendering/core/walk_animation.h`: pure `constexpr` decision layer. `IsWalking(walk_progress)` and `Frame(frames, walk_progress, walking_group)` pick the drawn frame; `walk_progress` is steps completed plus progress through the current step (negative while standing).
- `source/item_definitions/formats/dat/dat_item_parser.cpp` / `protobuf_item_parser.cpp`: parse the walking frame group (DAT group type byte 1 / `FIXED_FRAME_GROUP_OUTFIT_MOVING`, outfits only) into `DatCatalogEntry::walking_group`.
- `source/rendering/core/graphics_assembler.cpp`: `installWalkingGroup` builds the nested `GameSprite::walking_group`.
- `source/rendering/drawers/entities/creature_drawer.cpp`: swaps the outfit (and mount) to the walking group while `walkProgress` says the creature walks; `source/ingame_preview/ingame_preview_canvas.cpp` feeds the step counter.
- `tests/test_walk_animation.cpp`: frame formula, classic-formula compatibility and cycling.

## 2. When to Use It

- Any feature that needs "which frame does a walking creature show": pass the caller's `walk_progress` and let `WalkAnimation::Frame` decide. Never derive frames in the drawer again.
- `walk_progress` comes from one place: the entity that knows the steps (the in-game preview canvas). Everything else uses the `CreatureDrawOptions::walkProgress` default `-1.0f`, which means standing and reproduces the pre-walk behaviour exactly.
- A walking frame group is optional data: outfits without one walk on the standing sprite's frames 1..n-1 with the classic two-frame pace (4 frame changes per step).

## 3. Minimal Usage

```cpp
const bool walking = WalkAnimation::IsWalking(options.walkProgress);
const int frame = WalkAnimation::Frame(spr->frames, options.walkProgress, spr->getWalkingGroup() != nullptr);
```

## 4. Invariants & Pitfalls

- `Frame` is a pure function: no state, no clock. The step cadence lives entirely in `walk_progress`, so consecutive steps continue the cycle instead of restarting it, and stopping resets the counter to a standing value.
- A broken walking group (out-of-range sprite id) downgrades the outfit to the standing sprite's classic walk frames - it never drops the whole catalog entry; the failing id is reported through the load warnings.
- Classic clients (no frame groups) must keep the historical frames; `test_walk_animation` pins the old formula explicitly, so changing the pace is a deliberate, visible test change.
- Mount and outfit pick frames independently: both swap to their own walking group, and the swap happens before mount metrics are computed so rendering and light placement stay aligned.
