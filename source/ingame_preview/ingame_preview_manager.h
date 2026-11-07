#ifndef RME_INGAME_PREVIEW_MANAGER_H_
#define RME_INGAME_PREVIEW_MANAGER_H_

#include "app/main.h"

namespace IngamePreview {

	class IngamePreviewWindow;

	class IngamePreviewManager {
	public:
		IngamePreviewManager();
		~IngamePreviewManager();

		static wxAuiPaneInfo DefaultPaneInfo();
		static void NormalizePaneInfo(wxAuiPaneInfo& info);

		void Create(bool updateManager = true);
		void Hide();
		void Destroy();
		void Update();
		bool IsVisible() const;

		IngamePreviewWindow* GetWindow() {
			return window;
		}

	private:
		void OnWindowDestroy(wxWindowDestroyEvent& e);

		IngamePreviewWindow* window = nullptr;
	};

} // namespace IngamePreview

extern IngamePreview::IngamePreviewManager g_preview;

#endif // RME_INGAME_PREVIEW_MANAGER_H_
