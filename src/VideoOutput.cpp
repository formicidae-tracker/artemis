#include "VideoOutput.hpp"
#include "Options.hpp"

#include <cpptrace/exceptions.hpp>
#include <cstdint>
#include <glib-object.h>
#include <glibconfig.h>

#include <gst/app/gstappsrc.h>
#include <gst/gst.h>

#include <gst/gstelement.h>
#include <gst/gstformat.h>
#include <gst/gstobject.h>
#include <gst/gstpad.h>
#include <gst/gstsample.h>
#include <gst/gstvalue.h>
#include <ios>
#include <mutex>

#include <glib.h>

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

	VideoOutput::Stats GetStats() const {
		return {.Processed = d_processed.load(), .Dropped = d_dropped.load()};
	}

private:
	static gchararray onLocationFull(
	    GstElement *filesink,
	    guint       fragment_id,
	    GstSample  *first_sample,
	    gpointer    userdata
	);
	void
	buildStreamOutput(const VideoOutputOptions &options, const Size &input);
	void buildFileOutput(const VideoOutputOptions &options, const Size &input);
	void linkBothOutput();
	void linkSingleOutput(GstElementRef ref);

	void onFrameDone(uint64_t frameID);
	void onFramePass(uint64_t frameID);

	slog::Logger<1> d_logger{slog::With(slog::String("task", "VideoOutput"))};
	GstElementPtr   d_pipeline;

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

	gulong d_srcProbe;

	std::optional<uint64_t> d_firstTimestamp;
	std::atomic<uint64_t>   d_lastFramePassed{0}, d_processed{0}, d_dropped{0};
	std::filesystem::path   d_outputFileTemplate{};
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

VideoOutput::Stats VideoOutput::GetStats() const {
	return d_impl->GetStats();
}

namespace details {
template <typename T> std::string printValue(const T &value) {
	std::ostringstream oss;
	oss << std::boolalpha << value;
	return oss.str();
}

template <> std::string printValue<GValue>(const GValue &value) {
	return "unsupported";
}

template <typename Name, typename Value, typename... Other>
void append_pair(
    std::string &out, Name &&name, const Value &value, Other &&...others
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
	out += printValue<Value>(value);

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

	if (res == nullptr) {
		std::string params;
		details::append_pair(params, std::forward<Args>(args)...);
		throw cpptrace::runtime_error(
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

	d_appsrc = GstElementFactory(
	    "appsrc",
	    "name",
	    "artemis-src",
	    "format",
	    GST_FORMAT_TIME,
	    "is-live",
	    TRUE,
	    "leaky-type",
	    GST_APP_LEAKY_TYPE_UPSTREAM,
	    "max-buffers",
	    1,
	    "emit-signals",
	    FALSE
	);
	GValue value = G_VALUE_INIT;
	g_value_init(&value, GST_TYPE_FRACTION);
	gst_value_set_fraction(&value, int(FPS * 10.0), 10);
	d_videoInputFormat = GstElementFactory(
	    "capsfilter",
	    "name",
	    "input-video-format",
	    "caps",
	    ("video/x-raw,width=" + std::to_string(inputResolution.width()) +
	     ",height=" + std::to_string(inputResolution.height()) +
	     ",format=GRAY8,max-framerate=" + std::to_string(int(FPS * 10.0)) +
	     "/10")
	        .c_str()
	);

	gst_bin_add_many(
	    GST_BIN(d_pipeline.get()),
	    d_appsrc,
	    d_videoInputFormat,
	    nullptr
	);

	if (gst_element_link(d_appsrc, d_videoInputFormat) == FALSE) {
		d_pipeline.reset();
		throw cpptrace::runtime_error("could not link video input format");
	}

	d_srcProbe = gst_pad_add_probe(
	    gst_element_get_static_pad(d_appsrc, "src"),
	    GST_PAD_PROBE_TYPE_BUFFER,
	    [](GstPad *pad, GstPadProbeInfo *info, gpointer userdata
	    ) -> GstPadProbeReturn {
		    (void)pad;
		    auto self   = reinterpret_cast<VideoOutputImpl *>(userdata);
		    auto buffer = gst_pad_probe_info_get_buffer(info);
		    if (buffer != nullptr) {
			    self->onFramePass(GST_BUFFER_OFFSET(buffer));
		    }
		    return GST_PAD_PROBE_OK;
	    },
	    this,
	    nullptr
	);

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
		throw cpptrace::runtime_error("could not link input-tee");
	}
	if (gst_element_link_pads(d_inputTee, "src_0", d_fileQueue, "sink") ==
	    FALSE) {
		throw cpptrace::runtime_error("could not link file-queue");
	}
	if (gst_element_link(d_fileQueue, d_fileScale) == FALSE) {
		throw cpptrace::runtime_error("could not link file-scale");
	}

	if (gst_element_link_pads(d_inputTee, "src_1", d_streamQueue, "sink") ==
	    FALSE) {
		throw cpptrace::runtime_error("could not link stream-queue");
	}

	if (gst_element_link(d_streamQueue, d_streamScale) == FALSE) {
		throw cpptrace::runtime_error("could not link stream-scale");
	}
}

void VideoOutputImpl::linkSingleOutput(GstElementRef ref) {
	if (gst_element_link(d_videoInputFormat, ref) == FALSE) {
		throw cpptrace::runtime_error("could not link {stream,file}-scale");
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
		throw cpptrace::runtime_error("could not link stream pipeline");
	}
}

gchararray VideoOutputImpl::onLocationFull(
    GstElement *filesink,
    guint       fragment_id,
    GstSample  *first_sample,
    gpointer    userdata
) {
	auto self = reinterpret_cast<VideoOutputImpl *>(userdata);
	auto res = g_strdup_printf(self->d_outputFileTemplate.c_str(), fragment_id);
	auto buffer      = gst_sample_get_buffer(first_sample);
	uint64_t frameID = -1UL;
	if (buffer == nullptr) {
		self->d_logger.Warn("no buffer associated at segment beginning");
	} else {
		frameID = GST_BUFFER_OFFSET(buffer);
	}

	self->d_logger.Info(
	    "new file segment",
	    slog::Int("fragment", fragment_id),
	    slog::String("path", res),
	    slog::Int("frame_id", frameID)
	);
	return res;
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
	d_outputFileTemplate =
	    std::filesystem::path(options.OutputDir) / "stream.%04d.mp4";
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
	    d_outputFileTemplate.c_str(),
	    "max-size-time",
	    std::to_string(options.SegmentDuration.Nanoseconds()).c_str()
	);

	g_signal_connect(
	    d_fileSink,
	    "format-location-full",
	    G_CALLBACK(&onLocationFull),
	    this
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
		throw cpptrace::runtime_error("could not link file pipeline");
	}
}

VideoOutputImpl::~VideoOutputImpl() {}

void VideoOutputImpl::PushFrame(const Frame::Ptr &frame) {
	struct InflightFrame {
		VideoOutputImpl *self;
		Frame::Ptr       frame;
	};

	gsize size   = frame->Width() * frame->Height();
	auto  buffer = gst_buffer_new_wrapped_full(
        GstMemoryFlags(GST_MEMORY_FLAG_READONLY),
        frame->Data(),
        size,
        0,
        size,
        new InflightFrame{.self = this, .frame = frame},
        [](gpointer userdata) {
            auto frame = reinterpret_cast<InflightFrame *>(userdata);
            frame->self->onFrameDone(frame->frame->ID());
            delete frame;
        }
    );
	Defer {
		gst_object_unref(buffer);
	};

	uint64_t pts{0};
	if (d_firstTimestamp.has_value()) {
		pts = frame->Timestamp() - d_firstTimestamp.value();
	} else {
		d_firstTimestamp = frame->Timestamp();
	}

	GST_BUFFER_PTS(buffer)      = GstClockTime(pts);
	GST_BUFFER_DTS(buffer)      = GST_CLOCK_TIME_NONE;
	GST_BUFFER_DURATION(buffer) = GST_CLOCK_TIME_NONE;

	GST_BUFFER_OFFSET(buffer)     = frame->ID();
	GST_BUFFER_OFFSET_END(buffer) = frame->ID() + 1;

	auto res = gst_app_src_push_buffer(GST_APP_SRC_CAST(d_appsrc), buffer);

	if (res != GST_FLOW_OK) {
		throw cpptrace::runtime_error("Could not push buffer");
	}
}

void VideoOutputImpl::onFramePass(uint64_t frameID) {
	d_lastFramePassed.store(frameID);
}

void VideoOutputImpl::onFrameDone(uint64_t frameID) {
	if (frameID <= d_lastFramePassed.load()) {
		d_processed.fetch_add(1);
	} else {
		d_dropped.fetch_add(1);
		d_logger.Warn(
		    "frame dropped",
		    slog::Int("total_dropped", d_dropped.load()),
		    slog::Float(
		        "percent_dropped",
		        100.0 * float(d_dropped.load()) /
		            (d_dropped.load() + d_processed.load())
		    )
		);
	}
}

} // namespace artemis
} // namespace fort
