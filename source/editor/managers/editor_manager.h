//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_EDITOR_MANAGER_H_
#define RME_EDITOR_MANAGER_H_

#include "app/main.h"
#include "editor/persistence/map_load_options.h"

class ClientVersion;
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
    void CloseTab(int idx);
    void CloseCurrentEditor();
	Editor* GetCurrentEditor();
	MapTab* GetCurrentMapTab() const;
	bool CloseLiveEditors(LiveSocket* sock);
	bool CloseAllEditors();
	void NewMapView();

	// Map lifecycle
	Map& GetCurrentMap();
	int GetOpenMapCount();
	bool ShouldSave();
    bool ShouldSave(const Editor& editor) const;
    bool SaveMap(Editor& editor, MapTab* mapTab, FileName filename, bool showdialog);
    bool SaveCurrentMap(FileName filename, bool showdialog);
	bool NewMap();
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
    int CountRealOpenMaps() const;
    bool SwitchClientVersionForLoad(ClientVersion& target);
};

extern EditorManager g_editors;

#endif
