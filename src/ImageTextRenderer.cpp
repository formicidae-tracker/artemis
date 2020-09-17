#include "ImageTextRenderer.hpp"

#include "fonts/fonts_data.h"


namespace fort {
namespace artemis {

bool ImageTextRenderer::s_initialized = false;

ImageTextRenderer::FontChar ImageTextRenderer::s_fontData[256];

void ImageTextRenderer::Initialize() {
	if ( s_initialized == true ) {
		return;
	}
	memcpy(s_fontData,vga_fon,vga_fon_size);
	s_initialized = true;
}

size_t ImageTextRenderer::TextWidth(const std::string & text) {
	return text.size() * TOTAL_GLYPH_WIDTH;
}


cv::Rect ImageTextRenderer::RenderText(cv::Mat & image,
                                       const std::string & text,
                                       const cv::Point & position,
                                       TextAlignement align) {
	switch(align) {
	case CENTERED:
		return RenderTextAt(image,text,cv::Point(position.x - TextWidth(text)/2),
		                                         position.y);
	case RIGHT_ALIGNED:
		return RenderTextAt(image,text,cv::Point(position.x - TextWidth(text),
		                                         position.y));
	default:
		return RenderTextAt(image,text,position);
	};

}

cv::Rect ImageTextRenderer::RenderTextAt(cv::Mat & image,
                                         const std::string & text,
                                         const cv::Point & position) {
	Initialize();
	static cv::Vec3b fg(255,255,255);
	static cv::Vec3b bg(0,0,0);
	for ( size_t iy = 0;  iy < GLYPH_HEIGHT; ++iy ) {
		size_t yy = position.y+iy;
		size_t ic = 0;
		for ( auto c : text ) {
			uint8_t xdata = s_fontData[c][iy];
			for ( size_t ix = 0; ix < GLYPH_WIDTH; ++ix) {
				size_t xx = position.x + TOTAL_GLYPH_WIDTH * ic + ix;
				if ((xdata & (1 << (7-ix))) != 0) {
					image.at<cv::Vec3b>(yy,xx) = fg;
				} else {
					image.at<cv::Vec3b>(yy,xx) = bg;
				}
			}
			size_t xx = position.x + TOTAL_GLYPH_WIDTH * ic + GLYPH_WIDTH;
			if ( c >= 0xC0 && c <= 0xDF ) {
				if ( xdata & 1 ) {
					image.at<cv::Vec3b>(yy,xx) = fg;
				} else {
					image.at<cv::Vec3b>(yy,xx) = bg;
				}
			} else {
				image.at<cv::Vec3b>(yy,xx) = bg;
			}
			ic += 1;
		}
	}
	return cv::Rect(position,cv::Size(TextWidth(text),GLYPH_HEIGHT));
}

} // namespace artemis
} // namespace fort
