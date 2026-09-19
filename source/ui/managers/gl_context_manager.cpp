#include "app/main.h"
#include "ui/managers/gl_context_manager.h"
#include "app/settings.h"
#include "util/system_specs.h"
#include "rendering/core/hardware_profile.h"
#include <spdlog/spdlog.h>

#ifdef __WXMSW__
	#include <windows.h>
#endif

namespace {

#ifdef __WXMSW__
using WglSwapIntervalProc = BOOL(WINAPI*)(int);

SwapIntervalApplicationResult applySwapIntervalWgl(int interval) {
	const auto swap_interval = reinterpret_cast<WglSwapIntervalProc>(wglGetProcAddress("wglSwapIntervalEXT"));
	if (!swap_interval) {
		return SwapIntervalApplicationResult::NotSet;
	}

	if (swap_interval(interval) == TRUE) {
		return SwapIntervalApplicationResult::Set;
	}

	if (interval < 0 && swap_interval(1) == TRUE) {
		return SwapIntervalApplicationResult::NonAdaptive;
	}

	return SwapIntervalApplicationResult::NotSet;
}
#endif

SwapIntervalApplicationResult applySwapInterval([[maybe_unused]] wxGLCanvas& canvas, [[maybe_unused]] VSyncMode mode) {
#if wxCHECK_VERSION(3, 3, 2)
	switch (canvas.SetSwapInterval(getSwapIntervalForMode(mode))) {
	case wxGLCanvas::SwapInterval::Set:
		return SwapIntervalApplicationResult::Set;
	case wxGLCanvas::SwapInterval::NonAdaptive:
		return SwapIntervalApplicationResult::NonAdaptive;
	case wxGLCanvas::SwapInterval::NotSet:
	default:
		return SwapIntervalApplicationResult::NotSet;
	}
#elif defined(__WXMSW__)
	return applySwapIntervalWgl(getSwapIntervalForMode(mode));
#else
	return SwapIntervalApplicationResult::NotSet;
#endif
}

} // namespace

GLContextManager g_gl_context;

GLContextManager::GLContextManager() = default;
GLContextManager::~GLContextManager() = default;

wxGLContext* GLContextManager::GetGLContext(wxGLCanvas* win) {
	if (win) {
		RegisterCanvas(win);
	}

	if (!OGLContext) {
#ifdef __WXOSX__
		OGLContext = std::make_unique<wxGLContext>(win, nullptr);
#else
		wxGLContextAttrs ctxAttrs;
		ctxAttrs.PlatformDefaults().CoreProfile().MajorVersion(4).MinorVersion(5).EndList();
		OGLContext = std::make_unique<wxGLContext>(win, nullptr, &ctxAttrs);
		spdlog::info("GLContextManager: Created new OpenGL 4.5 Core Profile context");
#endif
		// Initialize GLAD for the new context
		if (win) {
			if (EnsureContextCurrent(*OGLContext, win)) {
				if (!gladLoadGL()) {
					spdlog::error("GLContextManager: Failed to initialize GLAD!");
				} else {
					spdlog::info("GLContextManager: GLAD initialized successfully");
					const auto gl_specs = SystemSpecsDetector::detect(true);
					HardwareProfileManager::get().updateSpecs(gl_specs);
				}
			}
		}
	}

	return OGLContext.get();
}

void GLContextManager::RegisterCanvas(wxGLCanvas* canvas) {
	if (canvas) {
		m_canvases.insert(canvas);
	}
}

void GLContextManager::UnregisterCanvas(wxGLCanvas* canvas) {
	m_canvases.erase(canvas);
	m_lastAppliedModes.erase(canvas);
	if (m_fallbackCanvas == canvas) {
		m_fallbackCanvas = nullptr;
	}
}

VSyncMode GLContextManager::getConfiguredVSyncMode() const noexcept {
	const auto configured_mode = sanitizeVSyncMode(g_settings.getInteger(Config::VSYNC_MODE));
	if (configured_mode == VSyncMode::Adaptive && m_forceStandardVSyncForSession) {
		return VSyncMode::On;
	}

	return configured_mode;
}

VSyncApplicationOutcome GLContextManager::applyVSyncWithCurrentContext(wxGLCanvas& canvas) {
	const auto requested_mode = getConfiguredVSyncMode();
	const auto result = classifySwapIntervalResult(requested_mode, applySwapInterval(canvas, requested_mode));
	if (result == VSyncApplicationOutcome::AdaptiveFallback) {
		m_forceStandardVSyncForSession = true;
		m_lastAppliedModes[&canvas] = VSyncMode::On;
	} else if (result == VSyncApplicationOutcome::Applied) {
		m_lastAppliedModes[&canvas] = requested_mode;
	}

	return result;
}

VSyncApplicationOutcome GLContextManager::ApplyVSyncIfNeeded(wxGLCanvas& canvas) {
	const auto requested_mode = getConfiguredVSyncMode();
	if (auto it = m_lastAppliedModes.find(&canvas); it != m_lastAppliedModes.end() && it->second == requested_mode) {
		return VSyncApplicationOutcome::Applied;
	}

	return applyVSyncWithCurrentContext(canvas);
}

VSyncApplicationOutcome GLContextManager::probeConfiguredVSync() {
	if (!OGLContext) {
		return VSyncApplicationOutcome::Failed;
	}

	for (auto* canvas : m_canvases) {
		if (!canvas || !canvas->IsShownOnScreen()) {
			continue;
		}

		if (!EnsureContextCurrent(*OGLContext, canvas)) {
			continue;
		}

		return applyVSyncWithCurrentContext(*canvas);
	}

	// No visible canvas could be probed right now. The refreshed canvases will
	// apply vSync on their next successful paint once they have a current context.
	return VSyncApplicationOutcome::Applied;
}

void GLContextManager::refreshRegisteredCanvases() const {
	for (auto* canvas : m_canvases) {
		if (canvas && canvas->IsShownOnScreen()) {
			canvas->Refresh(false);
		}
	}
}

VSyncApplySummary GLContextManager::ReapplyVSyncToRegisteredCanvases() {
	m_lastAppliedModes.clear();
	m_forceStandardVSyncForSession = false;

	VSyncApplySummary summary;
	switch (probeConfiguredVSync()) {
	case VSyncApplicationOutcome::AdaptiveFallback:
		summary.adaptive_fallback = true;
		break;
	case VSyncApplicationOutcome::Failed:
		summary.apply_failed = true;
		break;
	case VSyncApplicationOutcome::Applied:
	default:
		break;
	}

	refreshRegisteredCanvases();
	return summary;
}

bool GLContextManager::EnsureContextCurrent(wxGLContext& ctx, wxGLCanvas* preferredCanvas) {
	// 1. Try preferred canvas if valid and shown
	if (preferredCanvas && preferredCanvas->IsShownOnScreen()) {
		if (preferredCanvas->SetCurrent(ctx)) {
			return true;
		}
	}

	// 2. Try the designated fallback canvas if it's shown
	if (m_fallbackCanvas && m_fallbackCanvas != preferredCanvas && m_fallbackCanvas->IsShownOnScreen()) {
		wxLogNull logNo; // Suppress "Invalid Pixel Format" or other errors if incompatible
		if (m_fallbackCanvas->SetCurrent(ctx)) {
			return true;
		}
	}

	// 3. Try any other registered canvas as a last resort
	wxLogNull logNo; // Suppress errors during fallback attempts
	for (auto* canvas : m_canvases) {
		if (canvas != preferredCanvas && canvas != m_fallbackCanvas && canvas->IsShownOnScreen()) {
			if (canvas->SetCurrent(ctx)) {
				return true;
			}
		}
	}

	return false;
}
