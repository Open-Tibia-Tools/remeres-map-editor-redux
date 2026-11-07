#ifndef RME_GL_CONTEXT_MANAGER_H_
#define RME_GL_CONTEXT_MANAGER_H_

#include <wx/wx.h>
#include <wx/glcanvas.h>
#include <memory>

#include <set>
#include <unordered_map>

#include "ui/managers/vsync_policy.h"

struct VSyncApplySummary {
	bool adaptive_fallback = false;
	bool apply_failed = false;
};

class GLContextManager {
public:
	GLContextManager();
	~GLContextManager();

	wxGLContext* GetGLContext(wxGLCanvas* win);
	// Every canvas sharing the GL context must be created with these attributes.
	[[nodiscard]] static const wxGLAttributes& CanvasAttributes();

	// Tries to make the context current on the preferred canvas, or falls back to
	// other available canvases if the preferred one is hidden (to avoid assertions).
	bool EnsureContextCurrent(wxGLContext& ctx, wxGLCanvas* preferredCanvas = nullptr);

	void RegisterCanvas(wxGLCanvas* canvas);
	void UnregisterCanvas(wxGLCanvas* canvas);
	VSyncApplicationOutcome ApplyVSyncIfNeeded(wxGLCanvas& canvas);
	VSyncApplySummary ReapplyVSyncToRegisteredCanvases();

	void SetFallbackCanvas(wxGLCanvas* canvas) {
		m_fallbackCanvas = canvas;
	}

private:
	[[nodiscard]] VSyncMode getConfiguredVSyncMode() const noexcept;
	[[nodiscard]] VSyncApplicationOutcome applyVSyncWithCurrentContext(wxGLCanvas& canvas);
	[[nodiscard]] VSyncApplicationOutcome probeConfiguredVSync();
	void refreshRegisteredCanvases() const;

	std::unique_ptr<wxGLContext> OGLContext;
	std::set<wxGLCanvas*> m_canvases;
	std::unordered_map<wxGLCanvas*, VSyncMode> m_lastAppliedModes;
	wxGLCanvas* m_fallbackCanvas = nullptr;
	bool m_forceStandardVSyncForSession = false;
};

extern GLContextManager g_gl_context;

#endif
