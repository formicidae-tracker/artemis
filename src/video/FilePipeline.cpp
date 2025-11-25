#include "FilePipeline.hpp"
#include "VideoOutput.hpp"
#include "video/gstreamer.hpp"
#include <cstdint>
#include <glib-object.h>
#include <gst/app/gstappsrc.h>
#include <gst/gstbin.h>
#include <gst/gstbuffer.h>
#include <gst/gstelement.h>
#include <gst/gstenumtypes.h>
#include <gst/gstmemory.h>
#include <gst/gstpad.h>
#include <memory>
#include <sstream>

namespace fort {
namespace artemis {
FilePipeline::FilePipeline(
    const VideoOutputOptions &options, const VideoOutput::Config &config
)
    : BusManagedPipeline{"file_pipeline", buildPipelineDescription(options, config)}
    , d_outputFileTemplate{
          std::filesystem::path(options.OutputDir) / "stream.%04d.mp4"
      } {
	d_inputSrc = GetByName("file-input-src");
	d_inputSrc_src =
	    GstPadPtr{gst_element_get_static_pad(d_inputSrc.get(), "src")};
	if (d_inputSrc_src == nullptr) {
		throw cpptrace::runtime_error(
		    "could not get src pad from input-src element in file pipeline"
		);
	}

	d_splitMuxSink = GetByName("file-muxsink");

	gst_pad_add_probe(
	    d_inputSrc_src.get(),
	    GST_PAD_PROBE_TYPE_BUFFER,
	    [](GstPad *pad, GstPadProbeInfo *info, gpointer userdata) {
		    auto self   = reinterpret_cast<FilePipeline *>(userdata);
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

	g_signal_connect(
	    d_splitMuxSink.get(),
	    "format-location-full",
	    G_CALLBACK(&FilePipeline::onLocationFull),
	    this
	);

	d_logger.Debug("starting");
	SetState(GST_STATE_PLAYING);
}

FilePipeline::~FilePipeline() {
#ifndef NDEBUG
	PrintDebug("/tmp/video-file-pipeline.dot");
#endif
	d_closing.store(true);
	gst_app_src_end_of_stream(GST_APP_SRC(d_inputSrc.get()));
	waitOnEOS();
}

std::string FilePipeline::buildPipelineDescription(
    const VideoOutputOptions &options, const VideoOutput::Config &config
) {

	auto fileResolution = config.InputResolution;
	if (options.Height > 0) {
		fileResolution = VideoOutputOptions::TargetResolution(
		    options.Height,
		    config.InputResolution
		);
	}

	std::ostringstream oss;

	oss << "appsrc name=file-input-src"          //
	    << " format=time"                        //
	    << " is-live=true"                       //
	    << " leaky-type=upstream"                //
	    << " block=false"                        //
	    << " max-buffers=" << config.InputBuffer //
	    << " emit-signals=false";

	oss << " ! video/x-raw"                                               //
	    << ",width=" << config.InputResolution.width()                    //
	    << ",height=" << config.InputResolution.height()                  //
	    << ",format=GRAY8"                                                //
	    << ",framerate=0/1"                                               //
	    << ",max-framerate=" << int(std::ceil(config.FPS * 10)) << "/10"; //

	oss << " ! videoconvertscale name=file-convert" //
	    << " n-threads=1"                           //
	    << " method=bilinear";                      //

	oss << " ! video/x-raw"                       //
	    << ",format=NV12"                         //
	    << ",width=" << fileResolution.width()    //
	    << ",height=" << fileResolution.height(); //

	oss << " ! queue name=file-encoder-queue" //
	    << " max-size-bytes=0"                //
	    << " max-size-buffers=20"             //
	    << " max-size-time=0";                //

	oss << " ! vah265enc name=file-encoder"                             //
	    << " bitrate=" << options.Bitrate_KB                            //
	    << " rate-control=vbr"                                          //
	    << " target-percentage=" << int(100 / options.BitrateMaxRatio); //

	oss << " ! h265parse name=file-h265parse"; //

	oss << " ! splitmuxsink name=file-muxsink" //
	    << " location="
	    << (std::filesystem::path(options.OutputDir) / "stream.%04d.mp4")
	           .c_str()                                               //
	    << " max-size-time=" << options.FileMaxSizeTime.Nanoseconds() //
	    << " muxer=mp4mux";

	return oss.str();
}

gchararray FilePipeline::onLocationFull(
    GstElement *filesink,
    guint       fragment_id,
    GstSample  *first_sample,
    gpointer    userdata
) {
	auto self = reinterpret_cast<FilePipeline *>(userdata);
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

void FilePipeline::onFramePass(uint64_t frameID, uint64_t PTS) {
	d_lastFramePassed.store(frameID);
	d_metadata.Register(frameID, PTS);
	d_logger.DDebug("frame passed", slog::Int("ID", frameID));
}

void FilePipeline::notifyDrop(uint64_t frameID) {
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

void FilePipeline::onFrameDone(uint64_t frameID) {
	if (frameID <= d_lastFramePassed.load()) {
		d_processed.fetch_add(1);
	} else {
		notifyDrop(frameID);
	}
}

bool FilePipeline::PushFrame(
    const Frame::Ptr &frame, const std::shared_ptr<FilePipeline> &self
) {
	if (d_closing.load() == true) {
		notifyDrop(frame->ID());
		return false;
	}

	struct Context {
		std::weak_ptr<FilePipeline> self;
		Frame::Ptr                  frame;
	};

	constexpr auto update = [](gpointer userdata) -> void {
		auto ctx  = reinterpret_cast<Context *>(userdata);
		auto self = ctx->self.lock();
		if (self) {
			self->onFrameDone(ctx->frame->ID());
		}
		delete ctx;
	};

	gsize      size = frame->Width() * frame->Height();
	auto       ctx  = new Context{.self = self, .frame = frame};
	GstBuffer *buffer{nullptr};

	buffer = gst_buffer_new_wrapped_full(
	    GST_MEMORY_FLAG_READONLY,
	    frame->Data(),
	    size,
	    0,
	    size,
	    ctx,
	    update
	);

	if (d_firstTimestamp_us.has_value() == false) {
		d_firstTimestamp_us = frame->Timestamp();
	}

	GST_BUFFER_PTS(buffer) =
	    GstClockTime((frame->Timestamp() - d_firstTimestamp_us.value()) * 1000);
	GST_BUFFER_DTS(buffer)      = GST_CLOCK_TIME_NONE;
	GST_BUFFER_DURATION(buffer) = GST_CLOCK_TIME_NONE;

	GST_BUFFER_OFFSET(buffer)     = frame->ID();
	GST_BUFFER_OFFSET_END(buffer) = frame->ID() + 1;

	auto ret = gst_app_src_push_buffer(GST_APP_SRC(d_inputSrc.get()), buffer);
	if (ret != GST_FLOW_OK) {
		uint64_t frameID = GST_BUFFER_OFFSET(buffer);
		d_logger.Error(
		    "could not push buffer",
		    slog::Int("ID", frameID),
		    slog::String(
		        "flow_return",
		        GEnumToString(gst_flow_return_get_type(), ret)
		    )
		);
		notifyDrop(frameID);
		return false;
	}
	return true;
}

} // namespace artemis
} // namespace fort
