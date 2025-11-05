#include "ImageU8.hpp"
#include "fort/utils/Defer.hpp"
#include "libyuv/scale.h"
#include "spng.h"
#include "taskflow/taskflow.hpp" // IWYU pragma: keep
#include <cpptrace/exceptions.hpp>
#include <cpptrace/utils.hpp>
#include <cstdint>
#include <dlfcn.h>
#include <libyuv.h>
#include <system_error>

namespace fort {
namespace artemis {

void copy_lines(ImageU8 &dst, const ImageU8 &src) {
	for (int32_t i = 0; i < src.height; ++i) {
		memcpy(
		    &dst.buffer[i * dst.stride],
		    &src.buffer[i * src.stride],
		    std::min(dst.stride, src.stride) * sizeof(uint8_t)
		);
	}
}

void schedule_copy_lines(ImageU8 &dst, const ImageU8 &src, tf::Runtime &rt) {
	for (int32_t i = 0; i < src.height; ++i) {
		rt.silent_async([i, dst, src]() {
			memcpy(
			    &dst.buffer[i * dst.stride],
			    &src.buffer[i * src.stride],
			    std::min(dst.stride, src.stride) * sizeof(uint8_t)
			);
		});
	}
}

void ImageU8::Copy(ImageU8 &dst, const ImageU8 &src, tf::Runtime *rt) {
	if (dst.width != src.width || dst.height != src.height) {
		throw std::invalid_argument("Sizes must match");
	}
	if (dst.stride == src.stride) {
		memcpy(
		    dst.buffer,
		    src.buffer,
		    src.height * src.stride * sizeof(uint8_t)
		);
		return;
	}

	if (rt == nullptr) {
		copy_lines(dst, src);
	} else {
		schedule_copy_lines(dst, src, *rt);
	}
}

ImageU8 ImageU8::GetROIFromAllocated(
    uint8_t       *buffer,
    size_t         len,
    const ImageU8 &src,
    const Rect    &ROI,
    tf::Runtime   *rt
) {
	ImageU8 res{ROI.width(), ROI.height(), buffer};
	if (res.NeededSize() > len) {
		throw std::runtime_error("unsifficient size in buffer");
	}
	Copy(res, src.GetROI(ROI), rt);
	return res;
}

void ImageU8::Resize(ImageU8 &dest, const ImageU8 &src, ScaleMode mode) {
	libyuv::FilterMode mode_;
	switch (mode) {
	case ScaleMode::None:
		mode_ = libyuv::kFilterNone;
		break;
	case ScaleMode::Linear:
		mode_ = libyuv::kFilterLinear;
		break;
	case ScaleMode::Bilinear:
		mode_ = libyuv::kFilterBilinear;
		break;
	case ScaleMode::Box:
		mode_ = libyuv::kFilterBox;
		break;
	}

	libyuv::ScalePlane(
	    src.buffer,
	    src.stride,
	    src.width,
	    src.height,
	    dest.buffer,
	    dest.stride,
	    dest.width,
	    dest.height,
	    mode_
	);
}

namespace details {

template <typename Function> std::string FunctionName(Function &&fn) {
	Dl_info infos;
	if (dladdr(reinterpret_cast<void *>(&fn), &infos) != 0) {
		return cpptrace::demangle(infos.dli_sname);
	}
	return cpptrace::demangle(typeid(fn).name());
}

class SPNGError : public cpptrace::runtime_error {
public:
	template <typename Function>
	SPNGError(int error, Function && fn)
		: cpptrace::runtime_error{
				FunctionName(fn) +
		    "() error (" + std::to_string(error) +
		    "): " + spng_strerror(error)}
		, d_code{error} {
	}

	int Code() const noexcept {
		return d_code;
	}

private:
	int d_code;
};

template <typename Function, typename... Args>
void SPNGCall(Function &&fn, Args &&...args) {
	int err = std::forward<Function>(fn)(std::forward<Args>(args)...);
	if (err != 0) {
		throw SPNGError(err, fn);
	}
}

} // namespace details

ImageU8 ImageU8::ReadPNG(const std::filesystem::path &filepath) {
	FILE *file = std::fopen(filepath.c_str(), "rb");
	if (file == nullptr) {
		throw std::system_error{
		    errno,
		    std::generic_category(),
		    "open(\"" + filepath.string() + "\",\"rb\")"
		};
	}
	auto ctx = spng_ctx_new(0);
	Defer {
		spng_ctx_free(ctx);
		fclose(file);
	};

	details::SPNGCall(spng_set_png_file, ctx, file);
	struct spng_ihdr ihdr;
	details::SPNGCall(spng_get_ihdr, ctx, &ihdr);
	size_t decodedSize;
	details::SPNGCall(spng_decoded_image_size, ctx, SPNG_FMT_G8, &decodedSize);
	constexpr static size_t MAX_IMAGE_SIZE = 8192 * 8192;

	if (decodedSize > MAX_IMAGE_SIZE) {
		throw cpptrace::runtime_error{
		    "PNG file \"" + filepath.string() +
		    "\" is too large: decoded_size=" + std::to_string(decodedSize) +
		    "is larger than "
		};
	}

	ImageU8 res{
	    int32_t(ihdr.width),
	    int32_t(ihdr.height),
	    nullptr,
	    int32_t(ihdr.width)
	};

	details::SPNGCall(
	    spng_decode_image,
	    ctx,
	    nullptr,
	    0,
	    SPNG_FMT_G8,
	    SPNG_DECODE_PROGRESSIVE
	);

	struct spng_row_info rowInfo;

	try {
		while (true) {
			details::SPNGCall(spng_get_row_info, ctx, &rowInfo);

			// note that video::Frame is not necessarly packed, use stride
			// instead of width.
			uint8_t *rowAddr = res.buffer + res.stride * rowInfo.row_num;

			details::SPNGCall(spng_decode_row, ctx, rowAddr, res.width);
		}
	} catch (const details::SPNGError &e) {
		if (e.Code() != SPNG_EOI) {
			throw;
		}
	}

	return res;

	return res;
}

void ImageU8::WritePNG(const std::filesystem::path &filepath) {
	auto file = fopen(filepath.c_str(), "wb");
	if (file == nullptr) {
		std::ostringstream oss;
		oss << "fopen(" << filepath << ", \"wb\") error";
		throw std::system_error{errno, std::generic_category(), oss.str()};
	}
	auto ctx = spng_ctx_new(SPNG_CTX_ENCODER);
	Defer {
		spng_ctx_free(ctx);
		fclose(file);
	};

	details::SPNGCall(spng_set_png_file, ctx, file);

	struct spng_ihdr ihdr {
		.width = uint32_t(width), .height = uint32_t(height), .bit_depth = 8,
		.color_type       = SPNG_COLOR_TYPE_GRAYSCALE,
		.filter_method    = SPNG_FILTER_NONE,
		.interlace_method = SPNG_INTERLACE_NONE,
	};

	details::SPNGCall(spng_set_ihdr, ctx, &ihdr);

	details::SPNGCall(
	    spng_encode_image,
	    ctx,
	    nullptr,
	    0,
	    SPNG_FMT_PNG,
	    SPNG_ENCODE_FINALIZE | SPNG_ENCODE_PROGRESSIVE
	);

	try {
		struct spng_row_info rowInfo;
		while (true) {
			details::SPNGCall(spng_get_row_info, ctx, &rowInfo);

			uint8_t *addr = buffer + stride * rowInfo.row_num;

			details::SPNGCall(spng_encode_row, ctx, addr, width);
		}
	} catch (const details::SPNGError &e) {
		if (e.Code() != SPNG_EOI) {
			throw;
		}
	}
}
} // namespace artemis
} // namespace fort
