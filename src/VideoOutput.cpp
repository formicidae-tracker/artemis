#include <cstdio>
#include <filesystem>
#include <glib-object.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <glibconfig.h>

#include <gio/gio.h>
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <gst/gstbin.h>
#include <gst/gstelement.h>
#include <gst/gstelementfactory.h>
#include <gst/gstobject.h>
#include <gst/gstpad.h>
#include <gst/gstutils.h>
#include <gst/rtsp/gstrtsptransport.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <memory>
#include <mutex>
#include <slog++/Logger.hpp>
#include <sstream>
#include <utility>

#include <cpptrace/exceptions.hpp>

#include <slog++/Attribute.hpp>
#include <slog++/Level.hpp>

#include "Options.hpp"
#include "VideoOutput.hpp"
#include "readerwriterqueue.h"

using namespace std::chrono_literals;

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
using GstPadPtr      = glib_owned_ptr<GstPad>;
using GstElementRef  = GstElement *;

class MetadataFile {
public:
	MetadataFile(const std::filesystem::path &path)
	    : d_logger{slog::With(slog::String("metadata_file", path.string()))} {
		std::filesystem::create_directories(path.parent_path());

		// open the file for write at filepath,truncating any, and throw
		// cpptrace::runtime_error on issue.
		GError *error = nullptr;
		Defer {
			if (error != nullptr) {
				g_error_free(error);
			}
		};

		d_file = g_file_new_for_path(path.c_str());
		if (d_file == nullptr) {
			throw cpptrace::runtime_error(
			    "could not create GFile for '" + path.string() + "'"
			);
		}
		d_stream = g_file_replace(
		    d_file,
		    nullptr,
		    FALSE,
		    G_FILE_CREATE_NONE,
		    nullptr,
		    &error
		);
		if (d_stream == nullptr || error != nullptr) {
			throw cpptrace::runtime_error(
			    "could not open metadata file '" + path.string() + "': " +
			    std::string{error == nullptr ? "unknown error" : error->message}
			);
		}
		d_logger.Debug("opened");
	}

	~MetadataFile() {
		d_logger.Debug("closing");
		d_closing.store(true);
		scheduleDispatch();
		decrementRef();
		size_t current;
		while ((current = d_refCount.load()) != 0) {
			d_refCount.wait(current);
		}
	}

	// disable copy and move
	MetadataFile(const MetadataFile &)            = delete;
	MetadataFile(MetadataFile &&)                 = delete;
	MetadataFile &operator=(const MetadataFile &) = delete;
	MetadataFile &operator=(MetadataFile &&)      = delete;

	void Write(uint64_t streamFrameID, uint64_t globalFrameID) {
		if (d_closing.load() == true) {
			return;
		}
		d_queue.enqueue({.Stream = streamFrameID, .Global = globalFrameID});
		scheduleDispatch();
	}

private:
	void scheduleDispatch() {
		incrementRef();
		g_main_context_invoke(
		    g_main_context_default(),
		    [](gpointer userdata) {
			    auto self = reinterpret_cast<MetadataFile *>(userdata);
			    self->dispatch();
			    self->decrementRef();
			    return G_SOURCE_REMOVE;
		    },
		    this
		);
	}

	void dispatch() {
		if (d_closing.load() == true) {
			if (d_stream == nullptr) {
				if (d_queue.size_approx() > 0) {
					d_logger.Error(
					    "loosing data as file is not opened on closing"
					);
				}
				return;
			}
			if (d_writing.load() == true) {
				// write will re-dispatch
				return;
			}
			Association a;
			while (d_queue.try_dequeue(a)) {
				g_output_stream_printf(
				    G_OUTPUT_STREAM(d_stream),
				    nullptr,
				    nullptr,
				    nullptr,
				    "%lu %lu\n",
				    a.Stream,
				    a.Global
				);
			}

			g_output_stream_close(G_OUTPUT_STREAM(d_stream), nullptr, nullptr);
			g_object_unref(d_stream);
			g_object_unref(d_file);
			d_stream = nullptr;
			d_file   = nullptr;

			return;
		}

		if (d_writing.load() == true) {
			return;
		}
		Association a;
		if (d_queue.try_dequeue(a) == false) {
			return;
		}
		writeAsync(a);
	}

	void incrementRef() {
		d_refCount.fetch_add(1);
	}

	void decrementRef() {
		d_refCount.fetch_sub(1);
		d_refCount.notify_all();
	}

	struct Association {
		uint64_t Stream, Global;
	};

	void writeAsync(const Association &a) {
		d_writing.store(true);
		auto size = snprintf(
		    d_buffer,
		    sizeof(d_buffer),
		    "%lu %lu\n",
		    a.Stream,
		    a.Global
		);
		incrementRef();
		g_output_stream_write_async(
		    G_OUTPUT_STREAM(d_stream),
		    d_buffer,
		    size,
		    G_PRIORITY_DEFAULT,
		    nullptr,
		    [](GObject *source_object, GAsyncResult *res, gpointer userdata) {
			    auto    self  = reinterpret_cast<MetadataFile *>(userdata);
			    GError *error = nullptr;
			    g_output_stream_write_finish(
			        G_OUTPUT_STREAM(source_object),
			        res,
			        &error
			    );
			    if (error != nullptr) {
				    self->d_logger.Error(
				        "could not write metadata: " +
				        std::string{error->message}
				    );
				    g_error_free(error);
			    }
			    self->d_writing.store(false);
			    self->d_writing.notify_all();
			    self->dispatch();
			    self->decrementRef();
		    },
		    this
		);
	}

	slog::Logger<1>                            d_logger;
	moodycamel::ReaderWriterQueue<Association> d_queue;
	std::atomic<size_t>                        d_refCount{1};
	std::atomic<bool>  d_closing{false}, d_writing{false};
	GFile             *d_file{nullptr};
	GFileOutputStream *d_stream{nullptr};
	char               d_buffer[1024];
};

// #define TEST_SMALL_BUFFER 1

class MetadataHandler {
public:
#ifdef TEST_SMALL_BUFFER
	constexpr static size_t BUFFER_SIZE = 10;
	constexpr static size_t RB_SIZE     = 16;
#else
	constexpr static size_t BUFFER_SIZE = 30;
	constexpr static size_t RB_SIZE     = 64;
#endif
	constexpr static size_t RB_MASK = RB_SIZE - 1;

	static_assert(
	    RB_SIZE > 0 && (RB_SIZE & RB_MASK) == 0,
	    "RB_SIZE must be a power of two"
	);

	static_assert(
	    BUFFER_SIZE < (RB_SIZE - 1),
	    "RB_SIZE must be large enough to hold BUFFER_SIZE"
	);

	MetadataHandler() {
		d_frames.resize(RB_SIZE);
	}

	~MetadataHandler() {
		std::lock_guard<std::mutex> lock{d_mutex};

		if (d_file == nullptr) {
			return;
		}
		popUntilSizeIs(0);
	}

	void Register(uint64_t frameID, uint64_t PTS) {
		std::lock_guard<std::mutex> lock{d_mutex};

		if (d_registerPTSOffset.has_value() == false) {
			d_registerPTSOffset = frameID;
		}

		enqueue(frameID, PTS - d_registerPTSOffset.value());
		if (d_file == nullptr) {
			return;
		}
		popUntilSizeIs(BUFFER_SIZE);
	}

	void
	NewSegment(const std::filesystem::path &path, uint64_t startStreamPTS) {
		std::lock_guard<std::mutex> lock{d_mutex};
		if (d_streamPTSOffset.has_value() == false) {
			d_streamPTSOffset = startStreamPTS;
		}
		startStreamPTS -= d_streamPTSOffset.value();

		if (d_file != nullptr) {
			while (bufferSize() > 0 && peekPTS() < startStreamPTS) {
				popAndWrite();
			}
		}
		d_file          = std::make_unique<MetadataFile>(path);
		d_framesWritten = 0;
		popUntilSizeIs(BUFFER_SIZE);
	}

private:
	struct FrameAssociation {
		uint64_t FrameID, PTS;
	};

	void popUntilSizeIs(size_t count) {
		while (bufferSize() > count) {
			popAndWrite();
		}
	}

	inline size_t bufferSize() const {
		return d_head - d_tail;
	}

	uint64_t peekPTS() const {
		return d_frames[d_tail & RB_MASK].PTS;
	}

	uint64_t popFrameID() {
		return d_frames[(d_tail++) & RB_MASK].FrameID;
	}

	void enqueue(uint64_t frameID, uint64_t PTS) {
		d_frames[(d_head++) & RB_MASK] = {.FrameID = frameID, .PTS = PTS};
	}

	void popAndWrite() {
		d_file->Write(d_framesWritten++, popFrameID());
	}

	std::unique_ptr<MetadataFile> d_file;
	uint64_t                      d_framesWritten;
	std::optional<uint64_t>       d_registerPTSOffset, d_streamPTSOffset;
	std::vector<FrameAssociation> d_frames;
	size_t                        d_head{0}, d_tail{0};
	std::mutex                    d_mutex;
};

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

	bool PushFrame(const Frame::Ptr &frame);

	VideoOutput::Stats GetStats() const {
		return {
		    .Processed     = d_processed.load(),
		    .Dropped       = d_dropped.load(),
		    .Reconnections = d_reconnections.load()
		};
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
	    const VideoOutputOptions  &options,
	    const VideoOutput::Config &config,
	    bool                       withQueue
	);
	std::string buildFilePipeline(
	    const VideoOutputOptions &options,
	    const Size               &inputResolution,
	    const Duration           &segmentDuration,
	    bool                      withQueue
	);
	std::string buildBothPipeline(
	    const VideoOutputOptions &options, const VideoOutput::Config &config
	);

	void notifyDrop(uint64_t frameID);
	void onFrameDone(uint64_t frameID);
	void onFramePass(uint64_t frameID, uint64_t PTS);
	void onMessage(GstBus *bus, GstMessage *message);
	void onStreamSinkError(GstMessage *message);
	void logGstMessage(GstMessage *message);

	void scheduleReconnect();
	void disconnectStream();
	void reconnectStream();

	void printDebug(const std::filesystem::path &path);

	std::string targetURL() const {
		return "rtsp://" + d_host + ":8554/" + d_hostname;
	}

	slog::Logger<1> d_logger{slog::With(slog::String("task", "VideoOutput"))};
	const size_t    d_inputBuffers;
	GstElementPtr   d_pipeline;
	using GstBusPtr = std::unique_ptr<GstBus, GObjectUnrefer<GstBus>>;
	GstElementPtr d_appsrc;
	GstElementPtr d_fileSplitMux, d_streamValve, d_streamEnc, d_streamParse,
	    d_streamSink;
	GstPadPtr d_streamParseSrc;
	gulong d_streamParseSrcProbe{0};

	std::optional<uint64_t> d_firstTimestamp;
	std::atomic<uint64_t>   d_lastFramePassed{0}, d_processed{0}, d_dropped{0},
	    d_reconnections{0};
	std::filesystem::path d_outputFileTemplate{};
	std::atomic<bool>     d_eosReached{false}, d_closing{false},
	    d_reconnectionScheduled{false};
	std::mutex d_reconfiguring;

	guint d_enforceOn{0}, d_watch{0}, d_reconnection{0};

	MetadataHandler d_metadata;

	std::string               d_host;
	std::string               d_hostname;
	std::chrono::milliseconds d_reconnectTimeout;
	friend class VideoOutput;
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

bool VideoOutput::PushFrame(const Frame::Ptr &frame) {
	return d_impl->PushFrame(frame);
}

VideoOutput::Stats VideoOutput::GetStats() const {
	return d_impl->GetStats();
}

size_t VideoOutput::InflightBufferSize() const {
	return d_impl->d_inputBuffers;
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
)
    : d_inputBuffers{config.InputBuffer}
    , d_host{options.Host}
    , d_reconnectTimeout{int64_t(config.ConnectionTimeout.Milliseconds())} {
	char buffer[1024];
	if (gethostname(buffer, 1024) != 0) {
		throw cpptrace::runtime_error("could not get hostname");
	}
	d_hostname = buffer;

	std::call_once(gst_initialized, []() {
		int    argc = 0;
		char **argv = {nullptr};
		gst_init(&argc, &argv);
	});

	std::string processPipelineDesc = buildInputPipeline(config);
	if (options.Host.empty() == false && options.OutputDir.empty() == false) {
		processPipelineDesc += buildBothPipeline(options, config);
	} else if (options.Host.empty() == false) {
		processPipelineDesc += buildStreamPipeline(options, config, false);
	} else if (options.OutputDir.empty() == false) {
		processPipelineDesc += buildFilePipeline(
		    options,
		    config.InputResolution,
		    config.FilePeriod,
		    false
		);
	} else {
		throw cpptrace::runtime_error("Both Host and OutputDir cannot be empty"
		);
	}

	d_logger.Info("pipeline", slog::String("description", processPipelineDesc));

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
	d_watch  = gst_bus_add_watch(
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
			    uint64_t PTS     = GST_BUFFER_PTS(buffer);
			    self->onFramePass(frameID, PTS);
		    }
		    return GST_PAD_PROBE_OK;
	    },
	    this,
	    nullptr
	);

	d_fileSplitMux = GstElementPtr{
	    gst_bin_get_by_name(GST_BIN(d_pipeline.get()), "file-muxsink")
	};
	if (options.OutputDir.empty() == false && d_fileSplitMux == nullptr) {
		throw cpptrace::runtime_error{"missing file-muxsink element"};
	}
	if (d_fileSplitMux != nullptr) {
		g_signal_connect(
		    d_fileSplitMux.get(),
		    "format-location-full",
		    G_CALLBACK(&VideoOutputImpl::onLocationFull),
		    this
		);
	}
	auto bin      = GST_BIN(d_pipeline.get());
	d_streamValve = GstElementPtr{gst_bin_get_by_name(bin, "stream-valve")};
	d_streamEnc   = GstElementPtr{gst_bin_get_by_name(bin, "stream-encoder")};
	d_streamParse = GstElementPtr{gst_bin_get_by_name(bin, "stream-parse")};
	if (d_streamParse != nullptr) {
		d_streamParseSrc =
		    GstPadPtr{gst_element_get_static_pad(d_streamParse.get(), "src")};
	}
	d_streamSink = GstElementPtr{gst_bin_get_by_name(bin, "stream-sink")};
#define check(v, n)                                                            \
	do {                                                                       \
		if (v == nullptr) {                                                    \
			throw cpptrace::logic_error{"could not found '" n "' in pipeline"  \
			};                                                                 \
		}                                                                      \
	} while (0)
	if (options.Host.empty() == false) {
		check(d_streamValve, "stream-valve");
		check(d_streamEnc, "stream-encoder");
		check(d_streamParse, "stream-parse");
		check(d_streamParseSrc, "stream-parse.src");
		check(d_streamSink, "stream-sink");
	}
#undef check

	d_logger.Debug("Starting pipeline");
	gst_element_set_state(d_pipeline.get(), GST_STATE_PLAYING);
	if (d_fileSplitMux != nullptr) {
		d_enforceOn = g_timeout_add(
		    1000,
		    [](gpointer userdata) -> gboolean {
			    auto   self = reinterpret_cast<VideoOutputImpl *>(userdata);
			    gst_element_set_state(
			        self->d_fileSplitMux.get(),
			        GST_STATE_PLAYING
			    );
			    return G_SOURCE_CONTINUE;
		    },
		    this
		);
	}
}

std::string
VideoOutputImpl::buildInputPipeline(const VideoOutput::Config &config) {
	std::ostringstream oss;
	oss << "appsrc name=input-src"                                    //
	    << " format=time"                                             //
	    << " is-live=true"                                            //
	    << " leaky-type=" << (config.LeakyPush ? "upstream" : "none") //
	    << " block=" << std::boolalpha << !config.LeakyPush           //
	    << " max-buffers=" << config.InputBuffer                      //
	    << " emit-signals=false";                                     //

	oss << " ! video/x-raw"                              //
	    << ",width=" << config.InputResolution.width()   //
	    << ",height=" << config.InputResolution.height() //
	    << ",format=GRAY8"                               //
	    << ",framerate=0/1"                              // variable framerate
	    << ",max-framerate=" << int(std::ceil(config.FPS * 10)) << "/10";

	return oss.str();
}

std::string VideoOutputImpl::buildStreamPipeline(
    const VideoOutputOptions  &options,
    const VideoOutput::Config &config,
    bool                       withQueue
) {
	auto streamSize = VideoOutputOptions::TargetResolution(
	    options.StreamHeight,
	    config.InputResolution
	);

	std::ostringstream oss;

	oss << " ! valve name=stream-valve" //
	    << " drop=false";

	oss << " ! queue name=stream-input-queue" //
	    << " max-size-bytes=0"                //
	    << " max-size-time=0"                 //
	    << " max-size-buffers=1";             //

	oss << " ! videoconvertscale name=stream-videoconvertscale" //
	    << " n-threads=1"                                       //
	    << " method=bilinear";                                  //

	oss << " ! capsfilter name=stream-videoconvert-format" //
	    << " caps=video/x-raw"                             //
	    << ",format=NV12"                                  //
	    << ",width=" << streamSize.width()                 //
	    << ",height=" << streamSize.height();              //

	oss << " ! queue name=stream-convert-queue" //
	    << " leaky=upstream"                    //
	    << " max-size-time=0"                   //
	    << " max-size-bytes=0";                 //

	if (config.EnforceStreamVideoRate) {

		oss << " ! videorate name=stream-videorate";

		oss << " ! capsfilter name=stream-videorate-caps" //
		    << " caps=video/x-raw,framerate=" << int(config.FPS * 10)
		    << "/10"; //
	}

	oss << " ! queue2 name=stream-encoder-queue"                      //
	    << " max-size-bytes=0"                                        //
	    << " max-size-buffers=0"                                      //
	    << " max-size-time=" << std::chrono::nanoseconds{2s}.count(); //

	oss << " ! vah264enc name=stream-encoder" //
	    << " bitrate=" << 1000                // 1Mbit/s
	    << " rate-control=cbr";               //

	oss << " ! capsfilter name=stream-encoder-format"         //
	    << " caps=video/x-h264,profile=constrained-baseline"; //

	oss << " ! h264parse name=stream-parse" //
	    << " config-interval=-1";

	oss << " ! rtspclientsink name=stream-sink" //
	    << " protocols=tcp"                     //
	    << " location=" << targetURL();         //
	return oss.str();
}

std::string VideoOutputImpl::buildFilePipeline(
    const VideoOutputOptions &options,
    const Size               &inputResolution,
    const Duration           &segmentDuration,
    bool                      withQueue
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

	oss << " ! queue name=file-scale-queue" //
	    << " max-size-bytes=0"              //
	    << " max-size-time=0"               //
	    << " max-size-buffers=1";

	oss << " ! videoconvertscale name=file-convert" //
	    << " n-threads=1"                           //
	    << " method=bilinear"                       //
	    ;

	oss << " ! video/x-raw"                      //
	    << ",format=NV12"                        //
	    << ",width=" << fileResolution.width()   //
	    << ",height=" << fileResolution.height() //
	    ;

	oss << " ! queue name=file-encoder-queue" //
	    << " max-size-bytes=0"                //
	    << " max-size-buffers=20"             //
	    << " max-size-time=0";

	oss << " ! vah265enc name=file-encoder"                             //
	    << " bitrate=" << options.Bitrate_KB                            //
	    << " rate-control=vbr"                                          //
	    << " target-percentage=" << int(100 / options.BitrateMaxRatio); //

	oss << " ! h265parse name=file-h265parse"; //

	oss << " ! splitmuxsink name=file-muxsink"                //
	    << " location=" << d_outputFileTemplate.c_str()       //
	    << " max-size-time=" << segmentDuration.Nanoseconds() //
	    << " muxer=mp4mux";
	return oss.str();
}

std::string VideoOutputImpl::buildBothPipeline(
    const VideoOutputOptions &options, const VideoOutput::Config &config
) {
	std::ostringstream oss;

	oss << " ! tee name=t";

	oss << " t.src_0" //
	    << buildFilePipeline(
	           options,
	           config.InputResolution,
	           config.FilePeriod,
	           true
	       );
	oss << " t.src_1" //
	    << buildStreamPipeline(options, config, true);

	return oss.str();
}

void VideoOutputImpl::printDebug(const std::filesystem::path &filepath) {
#ifndef NDEBUG1
	auto          data = std::string{gst_debug_bin_to_dot_data(
        GST_BIN(d_pipeline.get()),
        GST_DEBUG_GRAPH_SHOW_ALL
    )};
	std::ofstream file(filepath);
	file << data;
	file.close();
	d_logger.Info(
	    "saved pipeline debug file",
	    slog::String("filepath", filepath)
	);
#else
	(void)filepath;
#endif
}

VideoOutputImpl::~VideoOutputImpl() {
	printDebug("/tmp/videooutput.dot");
	d_logger.DDebug("marking closing");
	d_closing.store(true);
	// ensure we are not reconfiguring, closing flag will disable any future
	// reconfiguring.
	g_main_context_invoke(
	    g_main_context_default(),
	    [](gpointer userdata) {
		    auto self = reinterpret_cast<VideoOutputImpl *>(userdata);
		    if (self->d_reconnection == 0) {
			    // reconnect not scheduled and not already fired, nothing to
			    // do.
			    return G_SOURCE_REMOVE;
		    }
		    g_source_remove(self->d_reconnection);
		    self->d_logger.DDebug("Removing scheduled reconnection timeout");
		    self->d_reconnectionScheduled.store(false);
		    self->d_reconnectionScheduled.notify_all();

		    return G_SOURCE_REMOVE;
	    },
	    this
	);
	// wait for none sheduled, no reconfiguration pending
	d_logger.DDebug("waiting scheduled false");
	d_reconnectionScheduled.wait(true);

	std::lock_guard<std::mutex> lock{d_reconfiguring};
	d_logger.DDebug("Sending EOS through appsrc");
	gst_app_src_end_of_stream(GST_APP_SRC(d_appsrc.get()));
	d_logger.DDebug("Waiting EOS");

	if (d_enforceOn != 0) {
		g_source_remove(d_enforceOn);
		d_enforceOn = 0;
	}

	while (d_eosReached.load() == false) {
		GstState state, pending;
		auto     ret = gst_element_get_state(
            d_pipeline.get(),
            &state,
            &pending,
            std::chrono::nanoseconds{std::chrono::milliseconds{100}}.count()
        );
		if (state == GST_STATE_NULL) {
			continue;
		}
		if (state != GST_STATE_PLAYING || ret == GST_STATE_CHANGE_FAILURE) {
			d_logger.Error(
			    "Pipeline is not playing on closing, data maybe lost",
			    slog::String(
			        "state",
			        (const char *)g_enum_to_string(gst_state_get_type(), state)
			    ),
			    slog::String(
			        "pending",
			        (const char *)
			            g_enum_to_string(gst_state_get_type(), pending)
			    ),
			    slog::String(
			        "state_change",
			        (const char *)g_enum_to_string(
			            gst_state_change_return_get_type(),
			            ret
			        )
			    )
			);
			d_eosReached.store(true);
			break;
		}
	}
	g_source_remove(d_watch);
	gst_element_set_state(d_pipeline.get(), GST_STATE_NULL);
}

bool VideoOutputImpl::PushFrame(const Frame::Ptr &frame) {
	if (d_closing.load() == true) {
		notifyDrop(frame->ID());
		return false;
	}

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

	GstFlowReturn ret;
	{
		std::lock_guard<std::mutex> lock{d_reconfiguring};
		ret = gst_app_src_push_buffer(GST_APP_SRC(d_appsrc.get()), buffer);
	}

	d_logger.DDebug(
	    "frame pushed",
	    slog::Int("ID", GST_BUFFER_OFFSET(buffer)),
	    slog::Duration("PTS", std::chrono::nanoseconds{GST_BUFFER_PTS(buffer)})
	);

	if (ret != GST_FLOW_OK) {
		auto name = g_enum_to_string(gst_flow_return_get_type(), ret);
		d_logger.Error(
		    "could not push buffer",
		    slog::Int("ID", frame->ID()),
		    slog::String("return", name != nullptr ? name : "UNKNOWN")
		);
		notifyDrop(frame->ID());
		return false;
	}
	return true;
}

void VideoOutputImpl::onFramePass(uint64_t frameID, uint64_t PTS) {
	d_lastFramePassed.store(frameID);
	d_metadata.Register(frameID, PTS);
	d_logger.DDebug("frame passed", slog::Int("ID", frameID));
}

void VideoOutputImpl::notifyDrop(uint64_t frameID) {
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

void VideoOutputImpl::onFrameDone(uint64_t frameID) {
	if (frameID <= d_lastFramePassed.load()) {
		d_processed.fetch_add(1);
	} else {
		notifyDrop(frameID);
	}
}

void VideoOutputImpl::onStreamSinkError(GstMessage *message) {
	GError *err{nullptr};
	gchar  *debug_info{nullptr};
	gst_message_parse_error(message, &err, &debug_info);
	Defer {
		g_clear_error(&err);
		if (debug_info != nullptr) {
			g_free(debug_info);
		}
	};

	d_logger.Error(
	    "stream-sink error",
	    slog::String("error", err->message),
	    slog::String(
	        "debug_info",
	        debug_info == nullptr ? "<none>" : debug_info
	    )
	);

	if (d_closing.load() == true) {
		// if closing the pipeline would not be PLAYING, and prevents EOS to
		// propagate
		GstState state, pending;
		auto ret = gst_element_get_state(d_pipeline.get(), &state, &pending, 0);

		d_logger.Warn(
		    "Gstreamer ERROR while closing. Data maybe lost.",
		    slog::String(
		        "state",
		        (const char *)g_enum_to_string(gst_state_get_type(), state)
		    ),
		    slog::String(
		        "pending",
		        (const char *)g_enum_to_string(gst_state_get_type(), pending)
		    ),
		    slog::String(
		        "state_change",
		        (const char *)
		            g_enum_to_string(gst_state_change_return_get_type(), ret)
		    )
		);
		return;
	}
	disconnectStream();
	scheduleReconnect();
}

#ifndef NDEBUG
#define LOG_STATE_CHANGE 1
#endif

void VideoOutputImpl::onMessage(GstBus *bus, GstMessage *message) {
	auto type = GST_MESSAGE_TYPE(message);

	if (type == GST_MESSAGE_EOS) {
		d_logger.Info("End-Of-Stream reached");
		gst_element_set_state(d_pipeline.get(), GST_STATE_NULL);
		d_eosReached.store(true);
		return;
	}

	if (type == GST_MESSAGE_ERROR &&
	    std::string{message->src->name} == "stream-sink") {
		onStreamSinkError(message);
		return;
	}

	switch (type) {
	case GST_MESSAGE_ERROR:
	case GST_MESSAGE_WARNING:
	case GST_MESSAGE_INFO:
		logGstMessage(message);
		return;
#ifdef LOG_STATE_CHANGE
	case GST_MESSAGE_STATE_CHANGED:
		GstState oldstate, newstate, pending;
		gst_message_parse_state_changed(
		    message,
		    &oldstate,
		    &newstate,
		    &pending
		);
		d_logger.Info(
		    "state changed",
		    slog::String("element", (const char *)message->src->name),
		    slog::String(
		        "oldstate",
		        (const char *)g_enum_to_string(gst_state_get_type(), oldstate)
		    ),
		    slog::String(
		        "newstate",
		        (const char *)g_enum_to_string(gst_state_get_type(), newstate)
		    ),
		    slog::String(
		        "pending",
		        (const char *)g_enum_to_string(gst_state_get_type(), pending)
		    )

		);
		return;
#endif
	default:
		return;
	}
}

void VideoOutputImpl::logGstMessage(GstMessage *message) {
	GError *err{nullptr};
	gchar  *debug_info{nullptr};
	auto    level = slog::Level::Info;
	switch (GST_MESSAGE_TYPE(message)) {
	case GST_MESSAGE_INFO:
#ifdef NDEBUG
		return;
#endif
		gst_message_parse_info(message, &err, &debug_info);
		level = slog::Level::Info;
		break;
	case GST_MESSAGE_ERROR:
		level = slog::Level::Error;
		gst_message_parse_error(message, &err, &debug_info);
		break;
	case GST_MESSAGE_WARNING:
		gst_message_parse_warning(message, &err, &debug_info);
		level = slog::Level::Warn;
		break;
	default:
		return;
	}

	d_logger.Log(
	    level,
	    "pipeline message",
	    slog::String("source", (const char *)message->src->name),
	    slog::String("error", (const char *)err->message),
	    slog::String(
	        "debug_info",
	        debug_info == nullptr ? "<none>" : (const char *)debug_info
	    )
	);

	g_clear_error(&err);
	if (debug_info != nullptr) {
		g_free(debug_info);
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
	auto buffer = gst_sample_get_buffer(first_sample);
	uint64_t PTS{0};
	if (buffer == nullptr) {
		self->d_logger.Warn("no buffer associated at segment beginning");
	} else {
		PTS = GST_BUFFER_DTS(buffer);
	}

	std::filesystem::path movieFile = std::filesystem::path{res};

	auto metadataFile = movieFile.parent_path() /
	                    (movieFile.stem().stem().string() + ".frame-matching" +
	                     movieFile.stem().extension().string() + ".txt");
	self->d_metadata.NewSegment(metadataFile, PTS);
	self->d_logger.Info(
	    "new file segment",
	    slog::Int("fragment", fragment_id),
	    slog::String("movie", movieFile),
	    slog::String("metadata", metadataFile),
	    slog::Duration("PTS", std::chrono::nanoseconds{PTS})
	);
	return res;
}

void VideoOutputImpl::disconnectStream() {
	std::lock_guard<std::mutex> lock{d_reconfiguring};
	if (d_fileSplitMux != nullptr) {
		gst_element_set_state(d_fileSplitMux.get(), GST_STATE_PLAYING);
	}
	auto logger =
	    d_logger.With(slog::Int("reconnections", d_reconnections.load()));

	if (d_streamSink == nullptr) {
		logger.Warn("already disconnected");
		return;
	}

	g_object_set(d_streamValve.get(), "drop", TRUE, nullptr);

	d_streamParseSrcProbe = gst_pad_add_probe(
	    d_streamParseSrc.get(),
	    GstPadProbeType(GST_PAD_PROBE_TYPE_BLOCK),
	    [](GstPad *pad, GstPadProbeInfo *info, gpointer udata) {
		    return GST_PAD_PROBE_DROP;
	    },
	    nullptr,
	    nullptr
	);

	auto streamSinkChange =
	    gst_element_set_state(d_streamSink.get(), GST_STATE_NULL);

	auto streamEncChange =
	    gst_element_set_state(d_streamEnc.get(), GST_STATE_NULL);

	auto bin = GST_BIN(d_pipeline.get());
	gst_bin_remove(bin, d_streamSink.get());

	d_streamSink.reset();

	logger.Info(
	    "disconnected",
	    slog::String(
	        "stream-encoder_change",
	        (const char *)g_enum_to_string(
	            gst_state_change_return_get_type(),
	            streamEncChange
	        )
	    ),
	    slog::String(
	        "stream-sink_change",
	        (const char *)g_enum_to_string(
	            gst_state_change_return_get_type(),
	            streamSinkChange
	        )
	    )
	);
}

void VideoOutputImpl::reconnectStream() {
	std::lock_guard<std::mutex> lock{d_reconfiguring};
	if (d_fileSplitMux != nullptr) {
		gst_element_set_state(d_fileSplitMux.get(), GST_STATE_PLAYING);
	}

	if (d_streamSink != nullptr) {
		d_logger.Warn(
		    "already connected",
		    slog::Int("reconnections", d_reconnections.load())
		);
		return;
	}
	auto logger = d_logger.With(
	    slog::Int("reconnections", d_reconnections.fetch_add(1) + 1)
	);

	d_streamSink = GstElementPtr{GstElementFactory(
	    "rtspclientsink",
	    "name",
	    "stream-sink",
	    "location",
	    targetURL().c_str(),
	    "protocols",
	    GST_RTSP_LOWER_TRANS_TCP
	)};
	gst_object_ref(d_streamSink.get());
	gst_bin_add(GST_BIN(d_pipeline.get()), d_streamSink.get());

	gst_element_link_many(d_streamParse.get(), d_streamSink.get(), NULL);

	if (d_streamParseSrcProbe > 0) {
		gst_pad_remove_probe(d_streamParseSrc.get(), d_streamParseSrcProbe);
		d_streamParseSrcProbe = 0;
	}

	auto encChange =
	    gst_element_set_state(d_streamEnc.get(), GST_STATE_PLAYING);
	auto sinkChange =
	    gst_element_set_state(d_streamSink.get(), GST_STATE_PLAYING);

	g_object_set(d_streamValve.get(), "drop", FALSE, nullptr);

	logger.Info(
	    "connecting",
	    slog::String(
	        "stream-sink_change",
	        (const char *)
	            g_enum_to_string(gst_state_change_return_get_type(), sinkChange)
	    ),
	    slog::String(
	        "stream-encoder_change",
	        (const char *)
	            g_enum_to_string(gst_state_change_return_get_type(), encChange)
	    )
	);
}

void VideoOutputImpl::scheduleReconnect() {
	bool scheduled = false;
	if (d_reconnectionScheduled.compare_exchange_strong(scheduled, true) ==
	    false) {
		return;
	}

	d_reconnection = g_timeout_add(
	    d_reconnectTimeout.count(),
	    [](gpointer userdata) {
		    auto self = reinterpret_cast<VideoOutputImpl *>(userdata);

		    try {
			    self->reconnectStream();
		    } catch (const std::exception &e) {
			    self->d_logger.Error("could not reconnect", slog::Err(e));
		    }
		    self->d_reconnectionScheduled.store(false);
		    self->d_reconnectionScheduled.notify_all();
		    self->d_reconnection = 0;
		    return G_SOURCE_REMOVE;
	    },
	    this
	);
}

} // namespace artemis
} // namespace fort
