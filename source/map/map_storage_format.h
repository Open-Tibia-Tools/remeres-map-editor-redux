#ifndef RME_MAP_MAP_STORAGE_FORMAT_H_
#define RME_MAP_MAP_STORAGE_FORMAT_H_

#include <cstdint>

enum class OtbmCompression : uint8_t {
	None = 0, // Standard uncompressed OTBM (The Forgotten Server, Nostalrius, etc.)
	Gzip = 1  // GZIP-compressed OTBM (Canary, Crystal Server)
};

#endif // RME_MAP_MAP_STORAGE_FORMAT_H_
