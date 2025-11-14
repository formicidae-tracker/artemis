#include <atomic>
#include <glib-object.h>
#include <glib.h>
#include <glibconfig.h>
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <gst/gstbuffer.h>
#include <gst/gstbus.h>
#include <gst/gstelement.h>
#include <gst/gstformat.h>
#include <gst/gstobject.h>
#include <gst/gstpad.h>
#include <gst/gstparse.h>
#include <gst/gstsample.h>
#include <gst/gstvalue.h>

#include <cstdint>
#include <ios>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

#include <cpptrace/exceptions.hpp>

#include <slog++/Attribute.hpp>

#include "Options.hpp"
#include "VideoOutput.hpp"

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
	    const VideoOutputOptions &options, const VideoOutput::Config &config
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
	std::string buildInputPipeline(const VideoOutput::Config &config);
	std::string buildStreamPipeline(
	    const VideoOutputOptions &options, const Size &inputResolution
	);
	std::string buildFilePipeline(
	    const VideoOutputOptions &options,
	    const Size               &inputResolution,
	    const Duration           &segmentDuration
	);
	std::string buildBothPipeline(
	    const VideoOutputOptions &options, const VideoOutput::Config &config
	);

	void onFrameDone(uint64_t frameID);
	void onFramePass(uint64_t frameID);
	void onMessage(GstBus *bus, GstMessage *message);

	slog::Logger<1> d_logger{slog::With(slog::String("task", "VideoOutput"))};
	GstElementPtr   d_pipeline;
	using GstBusPtr = std::unique_ptr<GstBus, GObjectUnrefer<GstBus>>;
	GstElementPtr d_appsrc;

	std::optional<uint64_t> d_firstTimestamp;
	std::atomic<uint64_t>   d_lastFramePassed{0}, d_processed{0}, d_dropped{0};
	std::filesystem::path   d_outputFileTemplate{};
	std::atomic<bool>       d_eosReached{false};
};

VideoOutput::VideoOutput(
    const VideoOutputOptions &options, const Config &config
)
    : d_impl{std::make_unique<VideoOutputImpl>(options, config)} {}

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
    const VideoOutputOptions &options, const VideoOutput::Config &config
) {
	std::call_once(gst_initialized, []() {
		int    argc = 0;
		char **argv = {nullptr};
		gst_init(&argc, &argv);
	});

	std::string processPipelineDesc = buildInputPipeline(config);
	if (options.Host.empty() == false && options.OutputDir.empty() == false) {
		processPipelineDesc += buildBothPipeline(options, config);
	} else if (options.Host.empty() == false) {
		processPipelineDesc +=
		    buildStreamPipeline(options, config.InputResolution);
	} else if (options.OutputDir.empty() == false) {
		processPipelineDesc += buildFilePipeline(
		    options,
		    config.InputResolution,
		    config.FilePeriod
		);
	} else {
		throw cpptrace::runtime_error("Both Host and OutputDir cannot be empty"
		);
	}

	d_logger.Debug(
	    "pipeline",
	    slog::String("description", processPipelineDesc)
	);

	GError *error = nullptr;
	d_pipeline =
	    GstElementPtr(gst_parse_launch(processPipelineDesc.c_str(), &error));
	if (d_pipeline == nullptr || error != nullptr) {
		Defer {
			d_pipeline.reset();
			if (error != nullptr) {
				g_error_free(error);
			}
		};
		throw cpptrace::runtime_error{
		    "could not build pipeline: " +
		    std::string{error == nullptr ? "unknow error" : error->message}
		};
	}

	auto bus = GstBusPtr{gst_element_get_bus(d_pipeline.get())};
	gst_bus_add_watch(
	    bus.get(),
	    [](GstBus *bus, GstMessage *message, gpointer userdata) -> gboolean {
		    reinterpret_cast<VideoOutputImpl *>(userdata)->onMessage(
		        bus,
		        message
		    );
		    return G_SOURCE_CONTINUE;
	    },
	    this
	);

	d_appsrc = GstElementPtr{
	    gst_bin_get_by_name(GST_BIN(d_pipeline.get()), "input-src")
	};
	if (d_appsrc == nullptr) {
		throw cpptrace::runtime_error{"could not find input-src"};
	}

	auto pad = gst_element_get_static_pad(d_appsrc.get(), "src");
	if (pad == nullptr) {
		throw cpptrace::runtime_error("could not get input-src.src");
	}
	Defer {
		g_object_unref(pad);
	};
	gst_pad_add_probe(
	    pad,
	    GST_PAD_PROBE_TYPE_BUFFER,
	    [](GstPad *pad, GstPadProbeInfo *info, gpointer userdata
	    ) -> GstPadProbeReturn {
		    auto self   = reinterpret_cast<VideoOutputImpl *>(userdata);
		    auto buffer = GST_PAD_PROBE_INFO_BUFFER(info);
		    if (buffer != nullptr) {
			    uint64_t frameID = GST_BUFFER_OFFSET(buffer);
			    self->onFramePass(frameID);
		    }
		    return GST_PAD_PROBE_OK;
	    },
	    this,
	    nullptr
	);

	d_logger.Debug("Starting pipeline");
	gst_element_set_state(d_pipeline.get(), GST_STATE_PLAYING);
}

std::string
VideoOutputImpl::buildInputPipeline(const VideoOutput::Config &config) {
	std::ostringstream oss;
	oss << "appsrc name=input-src"                                    //
	    << " format=time"                                             //
	    << " is-live=true"                                            //
	    << " leaky-type=" << (config.LeakyPush ? "upstream" : "none") //
	    << " block=" << std::boolalpha << !config.LeakyPush           //
	    << " max-buffers=1"                                           //
	    << " emit-signals=false";                                     //
	oss << " ! video/x-raw"                                           //
	    << ",width=" << config.InputResolution.width()                //
	    << ",height=" << config.InputResolution.height()              //
	    << ",format=GRAY8"                                            //
	    << ",framerate=0/1" // variable framerate
	    << ",max-framerate=" << int(std::ceil(config.FPS * 10)) << "/10";
	return oss.str();
}

std::string VideoOutputImpl::buildStreamPipeline(
    const VideoOutputOptions &options, const Size &inputResolution
) {
	auto streamSize = VideoOutputOptions::TargetResolution(
	    options.StreamHeight,
	    inputResolution
	);

	std::ostringstream oss;
	oss << " ! videoscale name=stream-scale";
	oss << " ! capsfilter name=stream-video-format"        //
	    << "caps=video/x-raw"                              //
	    << ",width=" << streamSize.width()                 //
	    << ",height=" << streamSize.height();              //
	oss << " ! videoconvert name=stream-videoconvert";     //
	oss << " ! capsfilter name=stream-videoconvert-format" //
	    << " caps=video/x-raw(VAMemory),format=NV12";      //
	oss << " ! vah264enc name=stream-encoder"              //
	    << " bitrate=" << 1000                             // 1Mbit/s
	    << " rate-control=vbr"                             //
	    << "target-percentage=" << int(100 / options.BitrateMaxRatio); //
	oss << " ! capsfilter name=stream-encoder-format"                  //
	    << " caps=video/h-264,profile=constrained-baseline";           //
	oss << " ! h264parse name=stream-parse";                           //
	oss << " ! rtspclientsink name=stream-sink"                        //
	    << " location=" << options.Host;
	return oss.str();
}

std::string VideoOutputImpl::buildFilePipeline(
    const VideoOutputOptions &options,
    const Size               &inputResolution,
    const Duration           &segmentDuration
) {
	d_outputFileTemplate =
	    std::filesystem::path(options.OutputDir) / "stream.%04d.mp4";
	auto fileResolution = inputResolution;
	if (options.Height > 0) {
		fileResolution = VideoOutputOptions::TargetResolution(
		    options.Height,
		    inputResolution
		);
	}

	std::ostringstream oss;
	oss << " ! videoscale name=file-scale"                              //
	    << " ! capsfilter name=file-video-format"                       //
	    << "caps=video/x-raw"                                           //
	    << ",width=" << fileResolution.width()                          //
	    << ",height=" << fileResolution.height();                       //
	oss << " ! videoconvert name=file-videoconvert";                    //
	oss << " ! capsfilter name=file-videoconvert-format"                //
	    << " caps=video/x-raw,format=NV12";                             //
	oss << " ! vah265enc name=file-encoder"                             //
	    << " bitrate=" << options.Bitrate_KB                            //
	    << " rate-control=vbr"                                          //
	    << " target-percentage=" << int(100 / options.BitrateMaxRatio); //
	oss << " ! capsfilter name=file-encoder-format"                     //
	    << " caps=video/x-h265,profile=screen-extended-main";           //
	oss << " ! h265parse name=file-h265parse";                          //
	oss << " ! splitmuxsink name=file-muxsink"                          //
	    << " location=" << d_outputFileTemplate.c_str()                 //
	    << " max-size-time=" << segmentDuration.Nanoseconds()           //
	    << " muxer=mp4mux";
	return oss.str();
}

std::string VideoOutputImpl::buildBothPipeline(
    const VideoOutputOptions &options, const VideoOutput::Config &config
) {
	std::ostringstream oss;

	oss << " ! tee name=t";
	oss << " t.src_0 ! queue name=file-queue"
	    << buildFilePipeline(
	           options,
	           config.InputResolution,
	           config.FilePeriod
	       );
	oss << " t.src_1 ! queue name=stream-queue"
	    << buildStreamPipeline(options, config.InputResolution);

	return oss.str();
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

VideoOutputImpl::~VideoOutputImpl() {
	gst_app_src_end_of_stream(GST_APP_SRC(d_appsrc.get()));
	d_eosReached.wait(false);
}

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

	uint64_t pts{0};
	if (d_firstTimestamp.has_value()) {
		pts = frame->Timestamp() - d_firstTimestamp.value();
	} else {
		d_firstTimestamp = frame->Timestamp();
	}

	GST_BUFFER_PTS(buffer)      = GstClockTime(pts * 1000);
	GST_BUFFER_DTS(buffer)      = GST_CLOCK_TIME_NONE;
	GST_BUFFER_DURATION(buffer) = GST_CLOCK_TIME_NONE;

	GST_BUFFER_OFFSET(buffer)     = frame->ID();
	GST_BUFFER_OFFSET_END(buffer) = frame->ID() + 1;

	auto res = gst_app_src_push_buffer(GST_APP_SRC(d_appsrc.get()), buffer);

	if (res != GST_FLOW_OK) {
		gst_buffer_unref(buffer);
		throw cpptrace::runtime_error("Could not push buffer");
	}
}

void VideoOutputImpl::onFramePass(uint64_t frameID) {
	d_lastFramePassed.store(frameID);
	d_logger.Debug("frame passed", slog::Int("ID", frameID));
}

void VideoOutputImpl::onFrameDone(uint64_t frameID) {
	if (frameID <= d_lastFramePassed.load()) {
		d_processed.fetch_add(1);
	} else {
		d_dropped.fetch_add(1);
		d_logger.Warn(
		    "frame dropped",
		    slog::Int("ID", frameID),
		    slog::Int("total_dropped", d_dropped.load()),
		    slog::Float(
		        "percent_dropped",
		        100.0 * float(d_dropped.load()) /
		            (d_dropped.load() + d_processed.load())
		    )
		);
	}
}

void VideoOutputImpl::onMessage(GstBus *bus, GstMessage *message) {
	switch (GST_MESSAGE_TYPE(message)) {
	case GST_MESSAGE_EOS:
		d_logger.Info("End-Of-Stream reached");
		gst_element_set_state(d_pipeline.get(), GST_STATE_NULL);
		d_eosReached.store(true);
		d_eosReached.notify_all();
		break;
	case GST_MESSAGE_ERROR: {
		GError *err;
		gchar  *debug_info;
		gst_message_parse_error(message, &err, &debug_info);
		d_logger.Error(
		    "Pipeline Error",
		    slog::String("element", GST_OBJECT_NAME(message->src)),
		    slog::String("error", err->message),
		    slog::String(
		        "debug_info",
		        debug_info == nullptr ? "none" : std::string{debug_info}
		    )
		);
		g_clear_error(&err);
		g_free(debug_info);
		break;
	}
	default:

		// Unhandled message
		break;
	}
}

} // namespace artemis
} // namespace fort
