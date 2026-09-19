//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_ITEM_SERIALIZATION_OTBM_H_
#define RME_ITEM_SERIALIZATION_OTBM_H_

#include "io/otbm/otbm_types.h"
#include <memory>

class IOMap;
class Item;
class NodeFileWriteHandle;
class FastOTBMStream;
class FastOTBMNode;

/**
 * @brief Utility for OTBM-specific item serialization.
 * Extracts the logic of how items are read from and written to OTBM streams.
 */
class ItemSerializationOTBM {
public:
	static constexpr int MAX_CONTAINER_DEPTH = 256;

	// Reading (Fast zero-copy)
	static std::unique_ptr<Item> createFromStream(const IOMap& maphandle, FastOTBMStream& stream);
	static bool unserializeItemNode(const IOMap& maphandle, FastOTBMNode& node, Item& item, int depth = 0);
	static bool unserializeAttributes(const IOMap& maphandle, FastOTBMStream& stream, Item& item);
	static bool readAttribute(const IOMap& maphandle, OTBM_ItemAttribute attr, FastOTBMStream& stream, Item& item);

	// Writing
	static bool serializeItemNode(const IOMap& maphandle, NodeFileWriteHandle& f, const Item& item);
	static void serializeItemCompact(const IOMap& /*maphandle*/, NodeFileWriteHandle& f, const Item& item);
	static void serializeItemAttributes(const IOMap& maphandle, NodeFileWriteHandle& f, const Item& item);
};

#endif // RME_ITEM_SERIALIZATION_OTBM_H_
