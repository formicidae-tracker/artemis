#include "VideoOutput.hpp"
#include "Options.hpp"
#include <glib-object.h>
#include <glibconfig.h>
#include <gst/gstbin.h>
#include <gst/gstbuffer.h>
#include <gst/gstcompat.h>
#include <gst/gstelement.h>
#include <gst/gstelementfactory.h>
#include <gst/gstmemory.h>
#include <gst/gstutils.h>
#include <mutex>

#include <glib.h>
#include <gst/gst.h>
#include <string>
#include <string_view>
#include <utility>

namespace fort {

namespace artemis {

static std::once_flag gst_initialized;

template <typename T> struct GObjectUnrefer {
	void operator()(T *obj) const noexcept {
		if (obj != nullptr) {
			g_object_unref(obj);
		}
	}
};

template <typename T>
using glib_owned_ptr = std::unique_ptr<T, GObjectUnrefer<T>>;
using GstElementPtr  = glib_owned_ptr<GstElement>;
using GstElementRef  = GstElement *;

class VideoOutputImpl {
public:
	VideoOutputImpl(
	    const VideoOutputOptions &options, const Size &input, float FPS
	);

	~VideoOutputImpl();

	VideoOutputImpl(const VideoOutputImpl &)            = delete;
	VideoOutputImpl(VideoOutputImpl &&)                 = delete;
	VideoOutputImpl &operator=(const VideoOutputImpl &) = delete;
	VideoOutputImpl &operator=(VideoOutputImpl &&)      = delete;

	void PushFrame(const Frame::Ptr &frame);

private:
	struct InflightFrame {
		VideoOutputImpl *self;
		Frame::Ptr       frame;
	};

	void
	buildStreamOutput(const VideoOutputOptions &options, const Size &input);
	void buildFileOutput(const VideoOutputOptions &options, const Size &input);
	void linkBothOutput();
	void linkSingleOutput(GstElementRef ref);

	void onFrameDone(const Frame::Ptr &frame);

	GstElementPtr d_pipeline;

	GstElementRef d_appsrc;
	GstElementRef d_videoInputFormat;

	GstElementRef d_inputTee;
	GstElementRef d_streamQueue;
	GstElementRef d_fileQueue;

	GstElementRef d_streamScale;
	GstElementRef d_streamVideoFormat;
	GstElementRef d_streamEncoder;
	GstElementRef d_streamCaps;
	GstElementRef d_streamRtscp;

	GstElementRef d_fileScale;
	GstElementRef d_fileVideoFormat;
	GstElementRef d_fileEncoder;
	GstElementRef d_fileSink;
};

VideoOutput::VideoOutput(
    const VideoOutputOptions &options, const Size &inputResolution, float FPS
)
    : d_impl{std::make_unique<VideoOutputImpl>(options, inputResolution, FPS)} {
}

VideoOutput::~VideoOutput()                                  = default;
VideoOutput::VideoOutput(VideoOutput &&) noexcept            = default;
VideoOutput &VideoOutput::operator=(VideoOutput &&) noexcept = default;

void VideoOutput::Close() {
	d_impl.reset();
}

void VideoOutput::PushFrame(const Frame::Ptr &frame) {
	d_impl->PushFrame(frame);
}

namespace details {
template <typename Name, typename Value, typename... Other>
void append_pair(
    std::string &out, Name &&name, Value &&value, Other &&...others
) {
	static_assert(
	    sizeof...(Other) % 2 == 0,
	    "must have a pair number of name=value properties"
	);
	if (out.empty() == false) {
		out.push_back(' ');
	}

	out += std::string_view(std::forward<Name>(name));
	out += "=";
	out += std::string_view(std::forward<Value>(value));
	if constexpr (sizeof...(Other) != 0) {
		append_pair(out, std::forward<Other>(others)...);
	}
}
} // namespace details

template <typename Str, typename... Args>
GstElement *GstElementFactory(Str &&name, Args &&...args) {
	static_assert(
	    sizeof...(Args) % 2 == 0,
	    "Must have an even number of parameters( pairs  name=value properties)"
	);
	GstElement *res = gst_element_factory_make_full(
	    std::forward<Str>(name),
	    std::forward<Args>(args)...,
	    nullptr
	);

	if (res != nullptr) {
		std::string params;
		details::append_pair(params, std::forward<Args>(args)...);
		throw std::runtime_error(
		    "could not create GstElement '" + std::string{name} + " " + params +
		    "'"
		);
	}
	return res;
}

VideoOutputImpl::VideoOutputImpl(
    const VideoOutputOptions &options, const Size &inputResolution, float FPS
) {
	std::call_once(gst_initialized, []() {
		int    argc = 0;
		char **argv = {nullptr};
		gst_init(&argc, &argv);
	});

	d_pipeline = GstElementPtr{gst_pipeline_new("artemis-pipeline")};
	d_appsrc   = GstElementFactory(
        "appsrc",
        "name",
        "artemis-src",
        "is-live",
        "true",
        "leaky",
        "upstream",
        "max-buffers",
        "1",
        "emit-signals",
        "false"
    );

	d_videoInputFormat = GstElementFactory(
	    "video/x-raw",
	    "format",
	    "GRAY8",
	    "width",
	    std::to_string(inputResolution.width()).c_str(),
	    "height",
	    std::to_string(inputResolution.height()).c_str(),
	    "max-framerate",
	    (std::to_string(int(FPS * 10.0)) + "/10").c_str()
	);

	gst_bin_add_many(
	    GST_BIN(d_pipeline.get()),
	    d_appsrc,
	    d_videoInputFormat,
	    nullptr
	);
	if (gst_element_link_many(d_appsrc, d_videoInputFormat, nullptr) == FALSE) {
		d_pipeline.reset();
		throw std::runtime_error("could not link head of the pipeline");
	}

	if (options.Host.empty() == false) {
		buildStreamOutput(options, inputResolution);
	}

	if (options.OutputDir.empty() == false) {
		buildFileOutput(options, inputResolution);
	}

	if (d_fileScale != nullptr && d_streamScale != nullptr) {
		linkBothOutput();
		return;
	}
	if (d_fileScale != nullptr) {
		linkSingleOutput(d_fileScale);
	}
	if (d_streamScale != nullptr) {
		linkSingleOutput(d_streamScale);
	}
}

void VideoOutputImpl::linkBothOutput() {
	d_inputTee    = GstElementFactory("tee", "name", "input-tee");
	d_streamQueue = GstElementFactory("queue", "name", "stream-queue");
	d_fileQueue   = GstElementFactory("queue", "name", "file-queue");
	gst_bin_add_many(
	    GST_BIN(d_pipeline.get()),
	    d_inputTee,
	    d_streamQueue,
	    d_fileQueue,
	    nullptr
	);
	if (gst_element_link(d_videoInputFormat, d_inputTee) == FALSE) {
		throw std::runtime_error("could not link input-tee");
	}
	if (gst_element_link_pads(d_inputTee, "src_0", d_fileQueue, "sink") ==
	    FALSE) {
		throw std::runtime_error("could not link file-queue");
	}
	if (gst_element_link(d_fileQueue, d_fileScale) == FALSE) {
		throw std::runtime_error("could not link file-scale");
	}

	if (gst_element_link_pads(d_inputTee, "src_1", d_streamQueue, "sink") ==
	    FALSE) {
		throw std::runtime_error("could not link stream-queue");
	}

	if (gst_element_link(d_streamQueue, d_streamScale) == FALSE) {
		throw std::runtime_error("could not link stream-scale");
	}
}

void VideoOutputImpl::linkSingleOutput(GstElementRef ref) {
	if (gst_element_link(d_videoInputFormat, ref) == FALSE) {
		throw std::runtime_error("could not link {stream,file}-scale");
	}
}

void VideoOutputImpl::buildStreamOutput(
    const VideoOutputOptions &options, const Size &inputResolution
) {
	auto streamSize = VideoOutputOptions::TargetResolution(
	    options.StreamHeight,
	    inputResolution
	);

	d_streamScale = GstElementFactory("videoscale", "name", "stream-scale");

	d_streamVideoFormat = GstElementFactory(
	    "video/x-raw",
	    "name",
	    "stream-video-format",
	    "width",
	    std::to_string(streamSize.width()).c_str(),
	    "height",
	    std::to_string(streamSize.height()).c_str()
	);
	d_streamEncoder = GstElementFactory(
	    "openh264enc",
	    "name",
	    "stream-encoder",
	    "bitrate",
	    "500000"
	);
	d_streamCaps = GstElementFactory(
	    "capsfilter",
	    "name",
	    "stream-caps",
	    "caps",
	    "video/x-h264"
	);

	d_streamRtscp = GstElementFactory(
	    "rtspclientsink",
	    "name",
	    "stream-sink",
	    "location",
	    options.Host.c_str()
	);

	gst_bin_add_many(
	    GST_BIN(d_pipeline.get()),
	    d_streamScale,
	    d_streamVideoFormat,
	    d_streamEncoder,
	    d_streamCaps,
	    d_streamRtscp,
	    nullptr
	);
	if (gst_element_link_many(
	        d_streamScale,
	        d_streamVideoFormat,
	        d_streamEncoder,
	        d_streamCaps,
	        d_streamRtscp,
	        nullptr
	    ) == FALSE) {
		throw std::runtime_error("could not link stream pipeline");
	}
}

void VideoOutputImpl::buildFileOutput(
    const VideoOutputOptions &options, const Size &inputResolution
) {
	Size fileResolution = inputResolution;
	if (options.Height > 0) {
		fileResolution = VideoOutputOptions::TargetResolution(
		    options.Height,
		    inputResolution
		);
	}

	d_fileScale = GstElementFactory("videoscale", "name", "file-videscale");
	d_fileVideoFormat = GstElementFactory(
	    "video/x-raw",
	    "name",
	    "file-videoformat",
	    "width",
	    std::to_string(fileResolution.width()).c_str(),
	    "height",
	    std::to_string(fileResolution.height()).c_str()
	);

	d_fileEncoder = GstElementFactory(
	    "x264enc",
	    "name",
	    "file-encoder",
	    "bitrate",
	    "pass",
	    "3",
	    std::to_string(int(options.Bitrate_KB * options.BitrateMaxRatio))
	        .c_str(),
	    "speed-preset",
	    options.Quality.c_str(),
	    "tune",
	    options.Tune.c_str()
	);
	d_fileSink = GstElementFactory(
	    "splitmuxsink",
	    "name",
	    "file-splitsink",
	    "location",
	    (std::filesystem::path(options.OutputDir) / "stream.%04d.mp4").c_str(),
	    "max-size-time",
	    std::to_string((2 * Duration::Hour).Nanoseconds())
	);
	gst_bin_add_many(
	    GST_BIN(d_pipeline.get()),
	    d_fileScale,
	    d_fileVideoFormat,
	    d_fileEncoder,
	    d_fileSink,
	    nullptr
	);
	if (gst_element_link_many(
	        d_fileScale,
	        d_fileVideoFormat,
	        d_fileEncoder,
	        d_fileSink,
	        nullptr
	    ) == FALSE) {
		throw std::runtime_error("could not link file pipeline");
	}
}

VideoOutputImpl::~VideoOutputImpl() {}

void VideoOutputImpl::PushFrame(const Frame::Ptr &frame) {
	gsize size   = frame->Width() * frame->Height();
	auto  buffer = gst_buffer_new_wrapped_full(
        GstMemoryFlags(
            GST_MEMORY_FLAG_READONLY | GST_MEMORY_FLAG_PHYSICALLY_CONTIGUOUS
        ),
        frame->Data(),
        size,
        0,
        size,
        new InflightFrame{.self = this, .frame = frame},
        [](gpointer userdata) {
            auto frame = reinterpret_cast<InflightFrame *>(userdata);
            frame->self->onFrameDone(frame->frame);
            delete frame;
        }
    );
}

} // namespace artemis
} // namespace fort
