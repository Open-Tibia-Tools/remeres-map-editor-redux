#ifndef RME_MATERIAL_SERVER_IDS_H_
#define RME_MATERIAL_SERVER_IDS_H_

#include <cstdint>
#include <string>
#include <vector>

class wxFileName;

// Brush and tileset XML still cite legacy server ids. OTBM item ids are already
// client ids and must not pass through this map.
uint16_t materialClientId(uint16_t server_id);
// False when items.otb is missing or unreadable. The caller must not load materials.
bool loadMaterialServerIds(const wxFileName& otb_path, std::vector<std::string>& warnings);
void clearMaterialServerIds();

#endif
