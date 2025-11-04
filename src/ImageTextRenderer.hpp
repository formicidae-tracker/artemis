#pragma once

#include "ImageU8.hpp"
#include <cstddef>
#include <cstdint>
#include <string>

namespace fort {
namespace artemis {

class ImageTextRenderer {
public:
	enum TextAlignement {
		LEFT_ALIGNED  = 0,
		CENTERED      = 1,
		RIGHT_ALIGNED = 2,
	};

	static Rect RenderText(
	    ImageU8               &image,
	    const std::string     &text,
	    const Eigen::Vector2i &position,
	    TextAlignement         align = LEFT_ALIGNED
	);

private:
	typedef uint8_t FontChar[16];

	const static size_t GLYPH_HEIGHT      = 16;
	const static size_t GLYPH_WIDTH       = 8;
	const static size_t TOTAL_GLYPH_WIDTH = 9;

	static Rect RenderTextAt(
	    ImageU8 &image, const std::string &text, const Eigen::Vector2i &position
	);

	static size_t TextWidth(const std::string &text);

	static void Initialize();

	static FontChar s_fontData[256];
	static bool     s_initialized;
};

} // namespace artemis
} // namespace fort
