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

#ifndef RME_SPAWN_H_
#define RME_SPAWN_H_

class Tile;

class Spawn {
public:
	Spawn(int size = 3) :
		size(0), selected(false) {
		setSize(size);
	}
	~Spawn() { }

	std::unique_ptr<Spawn> deepCopy() {
		std::unique_ptr<Spawn> copy = std::make_unique<Spawn>(size);
		copy->selected = selected;
		return copy;
	}

	bool isSelected() const {
		return selected;
	}
	void select() {
		selected = true;
	}
	void deselect() {
		selected = false;
	}

	// Does not compare selection!
	bool operator==(const Spawn& other) {
		return size == other.size;
	}
	bool operator!=(const Spawn& other) {
		return size != other.size;
	}

	void setSize(int newsize) {
		ASSERT(newsize >= 0);
		size = newsize;
	}
	int getSize() const {
		return size;
	}

protected:
	int size;
	bool selected;
};

using SpawnPositionList = std::set<Position>;
using SpawnList = std::list<Spawn*>;

class Spawns {
public:
	Spawns();
	~Spawns();

	void addSpawn(Tile* tile);
	void removeSpawn(Tile* tile);

	SpawnPositionList::iterator begin() {
		return spawns.begin();
	}
	SpawnPositionList::const_iterator begin() const {
		return spawns.begin();
	}
	SpawnPositionList::iterator end() {
		return spawns.end();
	}
	SpawnPositionList::const_iterator end() const {
		return spawns.end();
	}
	SpawnPositionList::iterator erase(SpawnPositionList::iterator iter) {
		return spawns.erase(iter);
	}
	SpawnPositionList::iterator find(Position& pos) {
		return spawns.find(pos);
	}
	SpawnPositionList::const_iterator find(const Position& pos) const {
		return spawns.find(pos);
	}
	SpawnPositionList::iterator lower_bound(const Position& pos) {
		return spawns.lower_bound(pos);
	}
	SpawnPositionList::const_iterator lower_bound(const Position& pos) const {
		return spawns.lower_bound(pos);
	}

	[[nodiscard]] bool empty() const noexcept {
		return spawns.empty();
	}
	[[nodiscard]] size_t size() const noexcept {
		return spawns.size();
	}

private:
	SpawnPositionList spawns;
};

#endif
