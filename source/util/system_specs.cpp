#include "util/system_specs.h"

#include <thread>
#include <algorithm>
#include <spdlog/spdlog.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dxgi.h>
#endif

#include <glad/glad.h>

SystemSpecs SystemSpecsDetector::detect(bool gl_context_active) {
	SystemSpecs specs;

	// 1. CPU Cores
	const unsigned int hardware_threads = std::thread::hardware_concurrency();
	specs.cpu_cores = hardware_threads > 0 ? hardware_threads : 4;

	// 2. System RAM (Windows)
#if defined(_WIN32)
	MEMORYSTATUSEX mem_status;
	mem_status.dwLength = sizeof(MEMORYSTATUSEX);
	if (GlobalMemoryStatusEx(&mem_status)) {
		specs.total_ram_mb = mem_status.ullTotalPhys / (1024ULL * 1024ULL);
		specs.avail_ram_mb = mem_status.ullAvailPhys / (1024ULL * 1024ULL);
	}
#endif

	// 3. GPU & Dedicated VRAM via DXGI (Windows)
#if defined(_WIN32)
	HMODULE dxgi_lib = LoadLibraryA("dxgi.dll");
	if (dxgi_lib) {
		using PFN_CreateDXGIFactory1 = HRESULT(WINAPI*)(REFIID, void**);
		auto pCreateDXGIFactory1 = reinterpret_cast<PFN_CreateDXGIFactory1>(GetProcAddress(dxgi_lib, "CreateDXGIFactory1"));
		if (pCreateDXGIFactory1) {
			IDXGIFactory1* factory = nullptr;
			if (SUCCEEDED(pCreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory))) && factory) {
				IDXGIAdapter1* adapter = nullptr;
				uint64_t best_dedicated_vram = 0;
				for (UINT idx = 0; factory->EnumAdapters1(idx, &adapter) != DXGI_ERROR_NOT_FOUND; ++idx) {
					if (!adapter) {
						continue;
					}
					DXGI_ADAPTER_DESC1 desc;
					if (SUCCEEDED(adapter->GetDesc1(&desc))) {
						const bool is_software = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
						if (!is_software) {
							const uint64_t vram_mb = desc.DedicatedVideoMemory / (1024ULL * 1024ULL);
							if (vram_mb > best_dedicated_vram || best_dedicated_vram == 0) {
								best_dedicated_vram = vram_mb;
								specs.dedicated_vram_mb = vram_mb;
								specs.shared_vram_mb = desc.SharedSystemMemory / (1024ULL * 1024ULL);

								char desc_utf8[256] = {};
								WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, desc_utf8, sizeof(desc_utf8) - 1, nullptr, nullptr);
								specs.gpu_renderer = desc_utf8;
							}
						}
					}
					adapter->Release();
				}
				factory->Release();
			}
		}
		FreeLibrary(dxgi_lib);
	}
#endif

	// 4. OpenGL Context Inspection (if context active)
	if (gl_context_active && glad_glGetString) {
		const auto* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
		const auto* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
		const auto* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));

		if (vendor) {
			specs.gpu_vendor = vendor;
		}
		if (renderer && (specs.gpu_renderer.empty() || specs.gpu_renderer == "Unknown")) {
			specs.gpu_renderer = renderer;
		}
		if (version) {
			specs.gl_version = version;
		}

		// Fallback VRAM query via OpenGL extension if DXGI returned 0
		if (specs.dedicated_vram_mb == 0 && glad_glGetIntegerv) {
			// NVIDIA: GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX
			GLint nvx_vram_kb = 0;
			glGetIntegerv(0x9047, &nvx_vram_kb);
			if (glGetError() == GL_NO_ERROR && nvx_vram_kb > 0) {
				specs.dedicated_vram_mb = static_cast<uint64_t>(nvx_vram_kb) / 1024ULL;
			} else {
				// AMD: GL_VBO_FREE_MEMORY_ATI
				GLint ati_mem[4] = { 0 };
				glGetIntegerv(0x87FB, ati_mem);
				if (glGetError() == GL_NO_ERROR && ati_mem[0] > 0) {
					specs.dedicated_vram_mb = static_cast<uint64_t>(ati_mem[0]) / 1024ULL;
				}
			}
		}
	}

	spdlog::info("[SystemSpecs] Detected CPU: {} logical threads | RAM: {} MB (available: {} MB) | GPU: {} ({}) | Dedicated VRAM: {} MB",
		specs.cpu_cores,
		specs.total_ram_mb,
		specs.avail_ram_mb,
		specs.gpu_renderer,
		specs.gpu_vendor,
		specs.dedicated_vram_mb
	);

	return specs;
}
