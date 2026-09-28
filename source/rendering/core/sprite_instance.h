#ifndef RME_RENDERING_CORE_SPRITE_INSTANCE_H_
#define RME_RENDERING_CORE_SPRITE_INSTANCE_H_

/**
 * Per-sprite instance data for instanced rendering.
 * Layout matches vertex attributes in sprite_batch.vert.
 *
 * 64 bytes per instance (aligned for GPU efficiency).
 */
struct SpriteInstance {
	float x, y, w, h; // Byte 0-15:  Screen rect (Location 2)
	float u_min, v_min, u_max, v_max; // Byte 16-31: UV rect (Location 3)
	float r, g, b, a; // Byte 32-47: Tint color (Location 4)
	float atlas_layer; // Byte 48-51: Texture layer (Location 5)
	float house_id = 0.0f; // Byte 52-55: House ID for overlay shader (Location 6)
	float zone_flags = 0.0f; // Byte 56-59: Zone and pathing flags for overlay shader (Location 7)
	float _pad3 = 0.0f; // Byte 60-63: Padding
};
static_assert(sizeof(SpriteInstance) == 64, "SpriteInstance must be 64 bytes");

#endif
