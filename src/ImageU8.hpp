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
	    ImageU8       &buffer,
	    const ImageU8 &src,
	    const Rect    &ROI,
	    tf::Runtime   *rt = nullptr
	);

	~ImageU8() {
		if (external == true || buffer == nullptr) {
			return;
		}
		free(buffer);
	}

	ImageU8() = default;

	ImageU8(const ImageU8 &other) {
		if (external == false && buffer != nullptr) {
			throw std::logic_error("allocated image is not copyable");
		}
		width    = other.width;
		height   = other.height;
		stride   = other.stride;
		buffer   = other.buffer;
		external = true;
	}

	ImageU8(ImageU8 &&other) {
		if (external == false && buffer != nullptr) {
			throw std::logic_error("allocated image is not movable");
		}
		width          = other.width;
		height         = other.height;
		stride         = other.stride;
		buffer         = other.buffer;
		external       = other.external;
		other.external = false;
		other.buffer   = nullptr;
	}

	ImageU8 &operator=(const ImageU8 &other) {
		if (external == false && buffer != nullptr) {
			throw std::logic_error("allocated image is not assignable");
		}
		width    = other.width;
		height   = other.height;
		stride   = other.stride;
		buffer   = other.buffer;
		external = true;
		return *this;
	}

	ImageU8 &operator=(ImageU8 &&other) {
		if (external == false && buffer != nullptr) {
			throw std::logic_error("allocated image is not movable");
		}
		width          = other.width;
		height         = other.height;
		stride         = other.stride;
		buffer         = other.buffer;
		external       = other.external;
		other.external = false;
		other.buffer   = nullptr;
		return *this;
	}

	ImageU8(
	    int32_t  width_,
	    int32_t  height_,
	    uint8_t *buffer_ = nullptr,
	    int32_t  stride_ = -1
	)
	    : width{width_}
	    , height{height_}
	    , stride{stride_}
	    , buffer{buffer_}
	    , external{buffer_ != nullptr} {
		if (width == 0 && height == 0) {
			return;
		}
		if (stride < 0 || stride < width) {
			stride = (width + 63) & ~63;
		}
		if (buffer == nullptr) {
			buffer = static_cast<uint8_t *>(aligned_alloc(64, height * stride));
		}
	}

	ImageU8(const ImageU8 &other, const Rect &ROI)
	    : width{ROI.width()}
	    , height{ROI.height()}
	    , stride{other.stride}
	    , buffer{&other.buffer[ROI.y() * other.stride + ROI.x()]}
	    , external{true} {

		if (other.width < (ROI.x() + ROI.width()) ||
		    other.height < (ROI.y() + ROI.height())) {
			throw std::invalid_argument("ROI does not fit in source image");
		}
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

private:
	bool external = false;
};

} // namespace artemis
} // namespace fort
