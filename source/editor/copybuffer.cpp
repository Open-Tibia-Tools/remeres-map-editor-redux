//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#include "app/definitions.h"

#include "editor/copybuffer.h"
#include "editor/editor.h"
#include "editor/operations/copy_operations.h"
#include "game/creature.h"

CopyBuffer::CopyBuffer() :
	tiles(nullptr),
	copyPos() {
}

CopyBuffer::~CopyBuffer() {
	tiles.reset();
}

size_t CopyBuffer::GetTileCount() {
	if (!tiles) {
		return 0;
	}
	return tiles->size();
}

Position CopyBuffer::getPosition() const {
	return copyPos;
}

void CopyBuffer::clear() {
	tiles.reset();
}

CopyResult CopyBuffer::copy(Editor& editor, int floor) {
	return CopyOperations::copy(editor, *this, floor);
}

CopyResult CopyBuffer::cut(Editor& editor, int floor) {
	return CopyOperations::cut(editor, *this, floor);
}

void CopyBuffer::paste(Editor& editor, const Position& toPosition) {
	CopyOperations::paste(editor, *this, toPosition);
}

bool CopyBuffer::canPaste() const {
	return tiles && tiles->size() != 0;
}

BaseMap& CopyBuffer::getBufferMap() {
	if (!tiles) {
		tiles = std::make_unique<BaseMap>();
	}
	return *tiles;
}
