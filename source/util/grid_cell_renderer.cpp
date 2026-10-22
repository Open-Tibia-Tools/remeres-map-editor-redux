#include "app/main.h"
#include "util/grid_cell_renderer.h"
#include "util/nvg_utils.h"
#include "ui/theme.h"

namespace rme::ui::grid {

	namespace {
		constexpr float kCardRadius = 4.0f;
		constexpr float kIconRadius = 3.0f;

		// Hover reaction: the card grows by up to two pixels a side and its
		// shadow deepens as the animation runs.
		constexpr float kHoverGrow = 2.0f;
		constexpr float kHoverShadowAlphaBase = 20.0f;
		constexpr float kHoverShadowAlphaFactor = 64.0f;
		constexpr float kHoverShadowBlurBase = 6.0f;
		constexpr float kHoverShadowBlurFactor = 4.0f;

		std::string ToUtf8(const wxString& text) {
			return std::string(text.ToUTF8());
		}

		// Width of a string in the current NanoVG font.
		float TextWidth(NVGcontext* vg, const std::string& text) {
			float bounds[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			nvgTextBounds(vg, 0.0f, 0.0f, text.c_str(), nullptr, bounds);
			return bounds[2] - bounds[0];
		}
	} // namespace

	void LabelCache::Clear() {
		m_entries.clear();
	}

	const CellLabel& LabelCache::Get(NVGcontext* vg, const wxString& text, float maxWidth) {
		const std::string utf8 = ToUtf8(text);
		const std::string key = utf8 + '\x1f' + std::to_string(static_cast<int>(maxWidth));

		if (const auto cached = m_entries.find(key); cached != m_entries.end()) {
			return cached->second;
		}

		CellLabel label;
		if (!text.IsEmpty() && TextWidth(vg, utf8) > maxWidth) {
			wxString line1;
			wxString line2;

			// Prefer breaking at the "id - name" delimiter when the prefix fits.
			const int dashPos = text.Find(" - ");
			if (dashPos != wxNOT_FOUND && dashPos > 0) {
				const wxString prefix = text.substr(0, dashPos);
				if (TextWidth(vg, ToUtf8(prefix)) <= maxWidth) {
					line1 = prefix;
					line2 = text.substr(dashPos + 3);
				}
			}

			if (line1.empty()) {
				// Otherwise break on the last word boundary that still fits.
				int bestSplit = -1;
				const int length = static_cast<int>(text.length());
				for (int index = 1; index < length; ++index) {
					const wxChar character = text[index];
					if (character != ' ' && character != '-') {
						continue;
					}
					const wxString candidate = text.substr(0, character == '-' ? index + 1 : index);
					if (TextWidth(vg, ToUtf8(candidate)) <= maxWidth) {
						bestSplit = index;
					} else {
						break;
					}
				}

				if (bestSplit != -1) {
					if (text[bestSplit] == '-') {
						line1 = text.substr(0, bestSplit + 1);
						line2 = text.substr(bestSplit + 1);
					} else {
						line1 = text.substr(0, bestSplit);
						line2 = text.substr(bestSplit + 1);
					}
				} else {
					// No boundary fits: drop characters until it does.
					wxString candidate = text;
					while (candidate.length() > 1) {
						candidate.RemoveLast();
						if (TextWidth(vg, ToUtf8(candidate)) <= maxWidth) {
							line1 = candidate;
							line2 = text.substr(candidate.length());
							break;
						}
					}
					if (line1.empty()) {
						line1 = text.substr(0, 1);
						line2 = text.substr(1);
					}
				}
			}

			// Trim the separator we broke on.
			while (!line1.empty() && (line1.Last() == ' ' || line1.Last() == '-' || line1.Last() == '\t')) {
				line1.RemoveLast();
			}
			while (!line2.empty() && (line2[0] == ' ' || line2[0] == '-' || line2[0] == '\t')) {
				line2.Remove(0, 1);
			}

			label.line1 = ToUtf8(line1);
			if (!line2.empty()) {
				if (const std::string utf8Line2 = ToUtf8(line2); TextWidth(vg, utf8Line2) <= maxWidth) {
					label.line2 = utf8Line2;
				} else {
					label.line2 = "...";
					wxString truncated = line2;
					while (truncated.length() > 1) {
						truncated.RemoveLast();
						const wxString candidate = truncated + "...";
						if (TextWidth(vg, ToUtf8(candidate)) <= maxWidth) {
							label.line2 = ToUtf8(candidate);
							break;
						}
					}
				}
			}
		} else {
			label.line1 = utf8;
		}

		return m_entries.emplace(key, std::move(label)).first->second;
	}

	wxRect DrawCardChrome(NVGcontext* vg, const wxRect& cell, bool selected, bool hovered, float hoverAnim) {
		float x = static_cast<float>(cell.x);
		float y = static_cast<float>(cell.y);
		float w = static_cast<float>(cell.width);
		float h = static_cast<float>(cell.height);

		if (hovered) {
			const float grow = kHoverGrow * hoverAnim;
			x -= grow;
			y -= grow;
			w += grow * 2.0f;
			h += grow * 2.0f;
		}

		const wxRect drawn(static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h));

		if (selected) {
			const NVGpaint glow = nvgBoxGradient(vg, x, y, w, h, kCardRadius, 10.0f, nvgRGBA(100, 150, 255, 128), nvgRGBA(0, 0, 0, 0));
			nvgBeginPath(vg);
			nvgRect(vg, x - 10.0f, y - 10.0f, w + 20.0f, h + 20.0f);
			nvgRoundedRect(vg, x, y, w, h, kCardRadius);
			nvgPathWinding(vg, NVG_HOLE);
			nvgFillPaint(vg, glow);
			nvgFill(vg);
		} else if (hovered) {
			const float alpha = kHoverShadowAlphaFactor * hoverAnim + kHoverShadowAlphaBase;
			const float blur = kHoverShadowBlurBase + kHoverShadowBlurFactor * hoverAnim;
			const NVGpaint shadow = nvgBoxGradient(vg, x, y + 2.0f, w, h, kCardRadius, blur, nvgRGBA(0, 0, 0, static_cast<int>(alpha)), nvgRGBA(0, 0, 0, 0));
			nvgBeginPath(vg);
			nvgRect(vg, x - 10.0f, y - 10.0f, w + 20.0f, h + 20.0f);
			nvgRoundedRect(vg, x, y, w, h, kCardRadius);
			nvgPathWinding(vg, NVG_HOLE);
			nvgFillPaint(vg, shadow);
			nvgFill(vg);
		}

		nvgBeginPath(vg);
		nvgRoundedRect(vg, x, y, w, h, kCardRadius);
		if (selected) {
			const NVGcolor accent = NvgUtils::ToNvColor(Theme::Get(Theme::Role::Accent));
			nvgFillColor(vg, nvgRGBAf(accent.r, accent.g, accent.b, 1.0f));
		} else if (hovered) {
			nvgFillColor(vg, NvgUtils::ToNvColor(Theme::Get(Theme::Role::CardBaseHover)));
		} else {
			nvgFillColor(vg, NvgUtils::ToNvColor(Theme::Get(Theme::Role::CardBase)));
		}
		nvgFill(vg);

		if (selected) {
			nvgBeginPath(vg);
			nvgRoundedRect(vg, x + 0.5f, y + 0.5f, w - 1.0f, h - 1.0f, kCardRadius);
			nvgStrokeColor(vg, NvgUtils::ToNvColor(Theme::Get(Theme::Role::Accent)));
			nvgStrokeWidth(vg, 2.0f);
			nvgStroke(vg);
		}

		return drawn;
	}

	void DrawCellIcon(NVGcontext* vg, const wxRect& iconRect, int texture, rme::rendering::TileIndicatorType badge) {
		if (badge != rme::rendering::TileIndicatorType::None) {
			rme::rendering::DrawNanoVGIndicatorBadge(vg, badge, static_cast<float>(iconRect.x), static_cast<float>(iconRect.y), static_cast<float>(iconRect.width));
			return;
		}

		const wxColour textColour = Theme::Get(Theme::Role::Text);

		if (texture <= 0) {
			// Placeholder for entries without a sprite.
			nvgBeginPath(vg);
			nvgRoundedRect(vg, static_cast<float>(iconRect.x), static_cast<float>(iconRect.y), static_cast<float>(iconRect.width), static_cast<float>(iconRect.height), kIconRadius);
			nvgFillColor(vg, nvgRGBA(textColour.Red(), textColour.Green(), textColour.Blue(), 12));
			nvgFill(vg);
			nvgStrokeColor(vg, nvgRGBA(textColour.Red(), textColour.Green(), textColour.Blue(), 40));
			nvgStrokeWidth(vg, 1.0f);
			nvgStroke(vg);

			nvgFontSize(vg, static_cast<float>(iconRect.width) * 0.45f);
			nvgFontFace(vg, "sans");
			nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgFillColor(vg, nvgRGBA(textColour.Red(), textColour.Green(), textColour.Blue(), 120));
			nvgText(vg, iconRect.x + iconRect.width / 2.0f, iconRect.y + iconRect.height / 2.0f, "?", nullptr);
			return;
		}

		// The sprite always fills the icon box: the icon size setting is the
		// zoom factor, so a 32px tile is drawn at 64 or 128 when asked for.
		const float drawX = static_cast<float>(iconRect.x);
		const float drawY = static_cast<float>(iconRect.y);
		const float drawWidth = static_cast<float>(iconRect.width);
		const float drawHeight = static_cast<float>(iconRect.height);

		nvgBeginPath(vg);
		nvgRoundedRect(vg, drawX, drawY, drawWidth, drawHeight, kIconRadius);
		nvgFillPaint(vg, nvgImagePattern(vg, drawX, drawY, drawWidth, drawHeight, 0.0f, texture, 1.0f));
		nvgFill(vg);
	}

	void DrawCellLabel(NVGcontext* vg, const wxRect& cell, const wxString& text, bool selected, LabelCache& cache) {
		if (text.IsEmpty()) {
			return;
		}

		nvgFontSize(vg, static_cast<float>(kLabelFontSize));
		nvgFontFace(vg, "sans");

		const CellLabel& label = cache.Get(vg, text, static_cast<float>(cell.width - 4));

		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFillColor(vg, selected ? NvgUtils::ToNvColor(Theme::Get(Theme::Role::TextOnAccent)) : NvgUtils::ToNvColor(Theme::Get(Theme::Role::Text)));

		const float centreX = cell.x + cell.width / 2.0f;
		const float labelTop = static_cast<float>(cell.y + cell.height - kLabelHeight);

		if (label.line2.empty()) {
			nvgText(vg, centreX, labelTop + kLabelHeight / 2.0f, label.line1.c_str(), nullptr);
		} else {
			nvgText(vg, centreX, labelTop + 9.0f, label.line1.c_str(), nullptr);
			nvgText(vg, centreX, labelTop + 23.0f, label.line2.c_str(), nullptr);
		}
	}

	void DrawRemoveBadge(NVGcontext* vg, const wxRect& cell, bool hovered) {
		const float size = static_cast<float>(kBadgeSize);
		const float x = static_cast<float>(cell.GetRight()) - size - 2.0f;
		const float y = static_cast<float>(cell.y) + 2.0f;

		nvgBeginPath(vg);
		nvgRoundedRect(vg, x, y, size, size, 2.0f);
		nvgFillColor(vg, hovered ? NvgColor(Theme::Role::Error) : NvgColor(Theme::Role::RaisedSurface));
		nvgFill(vg);
		nvgStrokeColor(vg, hovered ? NvgColor(Theme::Role::Error) : NvgColor(Theme::Role::Border, 200));
		nvgStrokeWidth(vg, 1.0f);
		nvgStroke(vg);

		const float arm = size * 0.22f;
		const float cx = x + size / 2.0f;
		const float cy = y + size / 2.0f;
		nvgBeginPath(vg);
		nvgMoveTo(vg, cx - arm, cy - arm);
		nvgLineTo(vg, cx + arm, cy + arm);
		nvgMoveTo(vg, cx + arm, cy - arm);
		nvgLineTo(vg, cx - arm, cy + arm);
		nvgStrokeColor(vg, hovered ? NvgColor(Theme::Role::TextOnAccent) : NvgColor(Theme::Role::Text, 220));
		nvgStrokeWidth(vg, 1.5f);
		nvgStroke(vg);
	}

	void DrawListLabel(NVGcontext* vg, const wxRect& row, const wxString& text) {
		nvgFontSize(vg, static_cast<float>(kListLabelFontSize));
		nvgFontFace(vg, "sans");
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgFillColor(vg, NvgUtils::ToNvColor(Theme::Get(Theme::Role::Text)));
		nvgText(vg, row.x + 40.0f, row.y + row.height / 2.0f, ToUtf8(text).c_str(), nullptr);
	}

} // namespace rme::ui::grid
