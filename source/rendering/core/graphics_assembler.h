#ifndef RME_RENDERING_CORE_GRAPHICS_ASSEMBLER_H_
#define RME_RENDERING_CORE_GRAPHICS_ASSEMBLER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class GraphicManager;
class GameSprite;
class NormalImage;
class SpriteArchive;
struct DatAnimationInfo;
struct DatCatalog;
struct DatCatalogEntry;

class GraphicsAssembler {
public:
	static bool install(GraphicManager& manager, const DatCatalog& catalog, std::shared_ptr<SpriteArchive> sprite_archive, std::string& error, std::vector<std::string>& warnings);

private:
	static NormalImage* ensureImage(GraphicManager& manager, const DatCatalog& catalog, const std::shared_ptr<SpriteArchive>& sprite_archive, uint32_t sprite_id);
	static void installAnimation(GameSprite& sprite, uint8_t frames, const DatAnimationInfo* animation);
	static bool installImages(GraphicManager& manager, const DatCatalog& catalog, const std::shared_ptr<SpriteArchive>& sprite_archive, GameSprite& sprite, const std::vector<uint32_t>& sprite_ids, std::vector<std::string>& warnings);
	static bool installWalkingGroup(GraphicManager& manager, const DatCatalog& catalog, const std::shared_ptr<SpriteArchive>& sprite_archive, GameSprite& sprite, const DatCatalogEntry& entry, std::vector<std::string>& warnings);
	static bool installSpriteEntry(GraphicManager& manager, const DatCatalog& catalog, const std::shared_ptr<SpriteArchive>& sprite_archive, const DatCatalogEntry& entry, std::vector<std::string>& warnings);
	static void resetRuntimeState(GraphicManager& manager);
};

#endif
