//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_EDITOR_MANAGER_H_
#define RME_EDITOR_MANAGER_H_

#include "app/main.h"
#include "editor/persistence/map_load_options.h"

#include <memory>

class Map;
class Editor;
class EditorTab;
class MapTab;
class LiveSocket;

class EditorManager {
public:
	EditorManager();
	~EditorManager();

	EditorTab* GetCurrentTab();
	EditorTab* GetTab(int idx);
	int GetTabCount() const;
	bool IsAnyEditorOpen() const;
	bool IsEditorOpen() const;
	void CloseCurrentEditor();
	Editor* GetCurrentEditor();
	MapTab* GetCurrentMapTab() const;
	void CycleTab(bool forward = true);
	bool CloseLiveEditors(LiveSocket* sock);
	bool CloseAllEditors();
	void NewMapView();

	// Map lifecycle
	Map& GetCurrentMap();
	int GetOpenMapCount();
	bool ShouldSave();
	bool SaveCurrentMap(FileName filename, bool showdialog);
	bool NewMap();
	// An empty sector map (see IOMapSec), with the client that reads objects.srv
	bool NewSectorMap();
	void OpenMap();
	void SaveMap();
	void SaveMapAs();
	void SaveMapAsGzip();
	bool LoadMap(const FileName& fileName, const MapLoadOptions& load_options = {});

	// Edit operations
	void DoCut();
	void DoCopy();
	void DoPaste();
	void PreparePaste();
	void StartPasting();
	void EndPasting();

	bool CanUndo();
	bool CanRedo();
	bool DoUndo();
	bool DoRedo();

private:
	// Shows a newly created empty map in a tab of its own.
	MapTab* ShowNewMap(std::unique_ptr<Editor> editor);
};

extern EditorManager g_editors;

#endif
