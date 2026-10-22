#ifndef RME_UI_DIALOGS_COLLECTION_STUDIO_WINDOW_H_
#define RME_UI_DIALOGS_COLLECTION_STUDIO_WINDOW_H_

#include <wx/dialog.h>
#include <wx/stattext.h>
#include <wx/button.h>

#include "editor/operations/collection_extractor_service.h"
#include <memory>

class Editor;
class TilesetGridToolbar;
class Brush;

namespace rme::ui {
class CollectionItemGrid;
class CollectionBrushGrid;
class CollectionOptionsPanel;
}

class CollectionStudioWindow : public wxDialog {
public:
	CollectionStudioWindow(wxWindow* parent, Editor* editor);
	~CollectionStudioWindow() override;

	void AppendFromSelection(Editor* editor);
	void DetachEditor();

protected:
	void OnClose(wxCloseEvent& event);
	void OnCharHook(wxKeyEvent& event);

private:
	void InitLayout();
	void UpdateUI();
	void OnDiscardItem(uint16_t itemId);
	void OnDiscardBrush(Brush* brush);
	void OnClearItems();
	void OnClearBrushes();
	void OnClearAll();
	void OnSave(const std::string& paletteName, const std::string& name, bool isNew, bool append, bool includeItems, bool includeBrushes);

	struct DiscardAction {
		enum class Kind {
			Item,
			Brush,
			ClearItems,
			ClearBrushes,
			ClearAll
		};

		Kind kind = Kind::Item;
		uint16_t itemId = 0;
		size_t originalIndex = 0;
		Brush* brush = nullptr;
		std::vector<uint16_t> savedItems;
		std::vector<Brush*> savedBrushes;
	};

	void UndoDiscard();
	void RedoDiscard();
	[[nodiscard]] bool CanUndo() const { return !m_undoStack.empty(); }
	[[nodiscard]] bool CanRedo() const { return !m_redoStack.empty(); }

	Editor* m_editor = nullptr;
	rme::collection::CollectionStagingContent m_staging;
	bool m_isDirty = false;
	std::vector<DiscardAction> m_undoStack;
	std::vector<DiscardAction> m_redoStack;

	// Col 1: Items
	TilesetGridToolbar* m_itemToolbar = nullptr;
	rme::ui::CollectionItemGrid* m_itemGrid = nullptr;
	wxButton* m_clearItemsBtn = nullptr;
	wxStaticText* m_itemCountLabel = nullptr;

	// Col 2: Brushes
	TilesetGridToolbar* m_brushToolbar = nullptr;
	rme::ui::CollectionBrushGrid* m_brushGrid = nullptr;
	wxButton* m_clearBrushesBtn = nullptr;
	wxStaticText* m_brushCountLabel = nullptr;

	// Col 3: Options
	rme::ui::CollectionOptionsPanel* m_optionsPanel = nullptr;
};

#endif // RME_UI_DIALOGS_COLLECTION_STUDIO_WINDOW_H_
