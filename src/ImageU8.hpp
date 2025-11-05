#pragma once

#include "Rect.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>

namespace tf {
class Runtime;
}

namespace fort {
namespace artemis {

struct ImageU8 {
	int32_t  width  = 0;
	int32_t  height = 0;
	int32_t  stride = 0;
	uint8_t *buffer = nullptr;

	enum class ScaleMode {
		None     = 0,
		Linear   = 1,
		Bilinear = 2,
		Box      = 3,
	};

	static void
	Copy(ImageU8 &dest, const ImageU8 &src, tf::Runtime *rt = nullptr);

	static void Resize(ImageU8 &dest, const ImageU8 &src, ScaleMode mode);

	static ImageU8 ReadPNG(const std::filesystem::path &filepath);

	static ImageU8 GetROIFromAllocated(
	    uint8_t       *buffer,
	    size_t         len,
	    const ImageU8 &src,
	    const Rect    &ROI,
	    tf::Runtime   *rt = nullptr
	);

	ImageU8() = default;

	ImageU8(
	    int32_t width_, int32_t height_, uint8_t *buffer_, int32_t stride_ = -1
	)
	    : width{width_}
	    , height{height_}
	    , stride{stride_}
	    , buffer{buffer_} {
		if (width == 0 || height == 0) {
			return;
		}
		if (stride < 0 || stride < width) {
			stride = (width + 63) & ~63;
		}
	}

	ImageU8 GetROI(const Rect &ROI) const {
		if (width < (ROI.x() + ROI.width()) ||
		    height < (ROI.y() + ROI.height())) {
			throw std::invalid_argument("ROI does not fit in source image");
		}
		return ImageU8{
		    width,
		    height,
		    &buffer[ROI.y() * stride + ROI.x()],
		    stride
		};
	}

	uint8_t &at(int32_t x, int32_t y) {
		return buffer[y * stride + x];
	}

	const uint8_t &at(int32_t x, int32_t y) const {
		return buffer[y * stride + x];
	}

	void WritePNG(const std::filesystem::path &path);

	artemis::Size Size() const {
		return {width, height};
	}

	size_t NeededSize() const {
		return stride * height;
	}
};

} // namespace artemis
} // namespace fort
