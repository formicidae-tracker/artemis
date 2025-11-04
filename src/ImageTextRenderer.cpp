#include "ImageTextRenderer.hpp"
#include "artemis-common_rc.h"
#include <Eigen/src/Core/Matrix.h>

namespace fort {
namespace artemis {

bool ImageTextRenderer::s_initialized = false;

ImageTextRenderer::FontChar ImageTextRenderer::s_fontData[256];

void ImageTextRenderer::Initialize() {
	if (s_initialized == true) {
		return;
	}
	memcpy(s_fontData, fonts_vga_fon_start, fonts_vga_fon_size);
	s_initialized = true;
}

size_t ImageTextRenderer::TextWidth(const std::string &text) {
	return text.size() * TOTAL_GLYPH_WIDTH;
}

Rect ImageTextRenderer::RenderText(
    ImageU8               &image,
    const std::string     &text,
    const Eigen::Vector2i &position,
    TextAlignement         align
) {
	switch (align) {
	case CENTERED:
		return RenderTextAt(
		    image,
		    text,
		    {position.x() - TextWidth(text) / 2, position.y()}
		);
	case RIGHT_ALIGNED:
		return RenderTextAt(
		    image,
		    text,
		    {position.x() - TextWidth(text), position.y()}
		);
	default:
		return RenderTextAt(image, text, position);
	};
}

Rect ImageTextRenderer::RenderTextAt(
    ImageU8 &image, const std::string &text, const Eigen::Vector2i &position
) {
	Initialize();
	for (size_t iy = 0; iy < GLYPH_HEIGHT; ++iy) {
		static constexpr uint8_t FG{255}, BG{0};

		size_t yy = position.y() + iy;
		size_t ic = 0;
		for (auto c : text) {
			uint8_t xdata = s_fontData[size_t(c)][iy];
			for (size_t ix = 0; ix < GLYPH_WIDTH; ++ix) {
				size_t xx = position.x() + TOTAL_GLYPH_WIDTH * ic + ix;
				if ((xdata & (1 << (7 - ix))) != 0) {
					image.at(xx, yy) = FG;
				} else {
					image.at(xx, yy) = BG;
				}
			}
			size_t xx = position.x() + TOTAL_GLYPH_WIDTH * ic + GLYPH_WIDTH;
			if (c >= 0xC0 && c <= 0xDF) {
				if (xdata & 1) {
					image.at(xx, yy) = FG;
				} else {
					image.at(xx, yy) = BG;
				}
			} else {
				image.at(xx, yy) = BG;
			}
			ic += 1;
		}
	}
	return Rect(position, Size(TextWidth(text), GLYPH_HEIGHT));
}

} // namespace artemis
} // namespace fort
