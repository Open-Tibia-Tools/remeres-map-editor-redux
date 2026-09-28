//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "util/image_manager.h"
#include "util/file_system.h"
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/image.h>
#include <wx/dcmemory.h>
#include <nanovg.h>
#include <glad/glad.h>
#include <spdlog/spdlog.h>
#include <vector>
#include <cstdint>
#include <string_view>
#include <span>
#include <ranges>

ImageManager& ImageManager::GetInstance() {
	static ImageManager instance;
	return instance;
}

ImageManager::ImageManager() {
}

ImageManager::~ImageManager() {
	ClearCache();
}

void ImageManager::ClearCache() {
	m_bitmapBundleCache.clear();
	m_tintedBitmapCache.clear();
	m_nvgImageCache.clear();
	m_glTextureCache.clear();
}

std::string ImageManager::ResolvePath(std::string_view assetPath) {
	// The path should be relative to the executable's "assets" directory
	static wxString executablePath = wxStandardPaths::Get().GetExecutablePath();
	static wxString assetsRoot = wxFileName(executablePath).GetPath() + wxFileName::GetPathSeparator() + "assets";

	// Use full path joining to avoid stripping subdirectories
	wxString fullPathLine = assetsRoot + wxFileName::GetPathSeparator() + wxString::FromUTF8(assetPath.data(), assetPath.size());
	wxFileName fn(fullPathLine);

	std::string fullPath = fn.GetFullPath().ToStdString();
	return fullPath;
}

wxBitmapBundle ImageManager::GetBitmapBundle(std::string_view assetPath, const wxColour& tint) {
	std::string fullPath = ResolvePath(assetPath);
	std::string cacheKey(assetPath);
	if (tint.IsOk()) {
		cacheKey += "_" + std::to_string(tint.GetRGB());
	}

	auto it = m_bitmapBundleCache.find(cacheKey);
	if (it != m_bitmapBundleCache.end()) {
		return it->second;
	}

	wxBitmapBundle bundle;
	if (wxFileName::FileExists(fullPath)) {
		if (assetPath.ends_with(".svg")) {
			bundle = wxBitmapBundle::FromSVGFile(fullPath, wxSize(16, 16));
			if (!bundle.IsOk()) {
				spdlog::error("ImageManager: Failed to load SVG bundle: {}", fullPath);
			}
		} else {
			wxBitmap bmp(fullPath, wxBITMAP_TYPE_PNG);
			if (bmp.IsOk()) {
				bundle = wxBitmapBundle::FromBitmap(bmp);
			} else {
				spdlog::error("ImageManager: Failed to load PNG bitmap: {}", fullPath);
			}
		}
	} else {
		spdlog::error("ImageManager: Asset file not found: {}", fullPath);
	}

	if (bundle.IsOk() && tint.IsOk()) {
		// If we need a tinted bundle, we might need to recreate it from tinted images
		// For simplicity now, let's just cache it. SVGs can be tinted via XML manipulation
		// but wxBitmapBundle doesn't expose that easily.
		// We could use GetBitmap and create a bundle from that.
	}

	m_bitmapBundleCache[cacheKey] = bundle;
	return bundle;
}

namespace {

wxBitmap FallbackBitmap(const wxSize& size) {
	const int width = size.GetWidth() > 0 ? size.GetWidth() : 16;
	const int height = size.GetHeight() > 0 ? size.GetHeight() : 16;
	return wxBitmap(width, height);
}

} // namespace

wxBitmap ImageManager::GetBitmap(std::string_view assetPath, const wxSize& size, const wxColour& tint) {
	wxBitmapBundle bundle = GetBitmapBundle(assetPath);
	if (!bundle.IsOk()) {
		return FallbackBitmap(size);
	}

	wxSize actualSize = size == wxDefaultSize ? bundle.GetDefaultSize() : size;

	if (!tint.IsOk()) {
		wxBitmap bitmap = bundle.GetBitmap(actualSize);
		return bitmap.IsOk() ? bitmap : FallbackBitmap(actualSize);
	}

	// For tinted bitmaps, use separate cache
	std::pair<std::string, uint32_t> cacheKey = { std::string(assetPath), static_cast<uint32_t>(tint.GetRGB()) };
	auto it = m_tintedBitmapCache.find(cacheKey);
	if (it != m_tintedBitmapCache.end()) {
		return it->second;
	}

	wxBitmap source = bundle.GetBitmap(actualSize);
	if (!source.IsOk()) {
		return FallbackBitmap(actualSize);
	}
	wxImage img = source.ConvertToImage();
	if (img.IsOk()) {
		img = TintImage(img, tint);
		wxBitmap tintedBmp(img);
		if (tintedBmp.IsOk()) {
			m_tintedBitmapCache[cacheKey] = tintedBmp;
			return tintedBmp;
		}
	}

	return FallbackBitmap(actualSize);
}

wxIconBundle ImageManager::GetIconBundle(std::string_view assetPath, const wxColour& tint) {
	wxIconBundle bundle;
	wxIcon icon;
	icon.CopyFromBitmap(GetBitmap(assetPath, wxDefaultSize, tint));
	if (icon.IsOk()) {
		bundle.AddIcon(icon);
	}
	return bundle;
}

wxImage ImageManager::TintImage(const wxImage& image, const wxColour& tint) {
	wxImage tinted = image.Copy();
	if (!tinted.HasAlpha()) {
		tinted.InitAlpha();
	}

	unsigned char r = tint.Red();
	unsigned char g = tint.Green();
	unsigned char b = tint.Blue();

	unsigned char* data = tinted.GetData();
	unsigned char* alpha = tinted.GetAlpha();
	int size = tinted.GetWidth() * tinted.GetHeight();

	std::span<unsigned char> pixels(data, size * 3);
	for (int i : std::views::iota(0, size)) {
		// Silhouette tinting: replace existing color with the tint color.
		// The original alpha channel handles the shape and anti-aliasing.
		pixels[i * 3 + 0] = r;
		pixels[i * 3 + 1] = g;
		pixels[i * 3 + 2] = b;
	}

	return tinted;
}

int ImageManager::GetNanoVGImage(NVGcontext* vg, std::string_view assetPath, const wxColour& tint) {
	NvgCacheKey cacheKey = { vg, std::string(assetPath), tint.IsOk() ? static_cast<uint32_t>(tint.GetRGB()) : 0xFFFFFFFF };
	auto it = m_nvgImageCache.find(cacheKey);
	if (it != m_nvgImageCache.end()) {
		return it->second;
	}

	std::string fullPath = ResolvePath(assetPath);
	int img = 0;

	if (assetPath.ends_with(".svg")) {
		// NanoVG doesn't native load SVG. We need to rasterize it.
		// For now, let's Rasterize it via wxImage (easiest bridge)
		wxBitmapBundle bundle = wxBitmapBundle::FromSVGFile(fullPath, wxSize(128, 128)); // High res raster
		if (bundle.IsOk()) {
			wxImage image = bundle.GetBitmap(wxSize(128, 128)).ConvertToImage();
			if (image.IsOk()) {
				if (tint.IsOk()) {
					image = TintImage(image, tint);
				}
				img = CreateNanoVGImageFromWxImage(vg, image);
			}
		}
	} else {
		if (tint.IsOk()) {
			// For PNGs with tint, we need to load via wxImage, tint, and then create NanoVG image
			wxImage image(fullPath, wxBITMAP_TYPE_PNG);
			if (image.IsOk()) {
				image = TintImage(image, tint);
				img = CreateNanoVGImageFromWxImage(vg, image);
			}
		} else {
			img = nvgCreateImage(vg, fullPath.c_str(), 0);
		}
	}

	if (img != 0) {
		m_nvgImageCache[cacheKey] = img;
	} else {
		spdlog::error("Failed to load NanoVG image: {}", assetPath);
	}

	return img;
}

int ImageManager::CreateNanoVGImageFromWxImage(NVGcontext* vg, const wxImage& image) {
	int w = image.GetWidth();
	int h = image.GetHeight();
	std::vector<uint8_t> rgba(w * h * 4);
	unsigned char* data = image.GetData();
	unsigned char* alpha = image.GetAlpha();
	bool hasAlpha = image.HasAlpha();

	std::span<uint8_t> dest(rgba);
	std::span<const uint8_t> src(data, w * h * 3);
	// srcAlpha might be null, so we must be careful.
	// We only access it if hasAlpha && alpha is true.

	if (hasAlpha && alpha) {
		for (int i : std::views::iota(0, w * h)) {
			dest[i * 4 + 0] = src[i * 3 + 0];
			dest[i * 4 + 1] = src[i * 3 + 1];
			dest[i * 4 + 2] = src[i * 3 + 2];
			dest[i * 4 + 3] = alpha[i];
		}
	} else {
		for (int i : std::views::iota(0, w * h)) {
			dest[i * 4 + 0] = src[i * 3 + 0];
			dest[i * 4 + 1] = src[i * 3 + 1];
			dest[i * 4 + 2] = src[i * 3 + 2];
			dest[i * 4 + 3] = 255;
		}
	}
	return nvgCreateImageRGBA(vg, w, h, 0, rgba.data());
}

uint32_t ImageManager::GetGLTexture(std::string_view assetPath) {
	// Not implemented yet - usually we can use NanoVG's image as GL texture if we know how it's stored,
	// or load it via glad.
	return 0;
}
