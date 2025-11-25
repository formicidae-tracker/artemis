#include "StreamPipeline.hpp"
#include "video/BusManagedPipeline.hpp"
#include "video/gstreamer.hpp"
#include <atomic>
#include <glib.h>
#include <gst/app/gstappsrc.h>
#include <gst/gstmessage.h>
#include <gst/gstpad.h>
#include <memory>

using namespace std::chrono_literals;

namespace fort {
namespace artemis {
StreamPipeline::StreamPipeline(const Config &config)
    : BusManagedPipeline{"stream-pipeline", buildPipelineDescription(config)}
    , d_onStreamError{config.OnStreamError} {

	d_inputSrc = GetByName("stream-input-src");

	d_streamValve = GetByName("stream-valve");

	d_streamSink = GetByName("stream-sink");

	auto streamParse = GetByName("stream-parse");
	d_streamParse_src =
	    GstPadPtr{gst_element_get_static_pad(streamParse.get(), "src")};
	if (d_streamParse_src == nullptr) {
		throw cpptrace::runtime_error(
		    "could not get src pad from stream-parse element in stream pipeline"
		);
	}

	d_logger.Debug("starting");
	SetState(GST_STATE_PLAYING);
}

StreamPipeline::~StreamPipeline() {
	d_closing.store(true);
}

bool StreamPipeline::PushBuffer(GstBuffer *buffer) {
	if (d_closing.load() == true) {
		return false;
	}

	gst_buffer_ref(buffer); // push will steal this ref;
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
		return false;
	}
	return true;
}

void StreamPipeline::OnMessage(GstBus *bus, GstMessage *message) {
	BusManagedPipeline::OnMessage(bus, message);

	if (std::string{message->src->name} != "stream-sink" ||
	    GST_MESSAGE_TYPE(message) != GST_MESSAGE_ERROR) {
		return;
	}

	if (d_blockProbe != 0) {
		return;
	}

	g_object_set(d_streamValve.get(), "drop", TRUE, nullptr);

	d_blockProbe = gst_pad_add_probe(
	    d_streamParse_src.get(),
	    GST_PAD_PROBE_TYPE_BLOCK,
	    [](GstPad *pad, GstPadProbeInfo *info, gpointer userdata) {
		    return GST_PAD_PROBE_DROP;
	    },
	    nullptr,
	    nullptr
	);

	gst_element_set_state(d_streamSink.get(), GST_STATE_NULL);

	d_onStreamError();
}

std::string StreamPipeline::buildPipelineDescription(const Config &config) {
	auto streamSize = VideoOutputOptions::TargetResolution(
	    config.Height,
	    config.InputResolution
	);

	std::ostringstream oss;

	oss << "appsrc name=stream-input-src"        //
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

	oss << " ! valve name=stream-valve" //
	    << " drop=false";

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

	if (config.EnforceVideoRate) {

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

	oss << " ! rtspclientsink name=stream-sink"                               //
	    << " protocols=tcp"                                                   //
	    << " location=rtsp://" << config.Host << ":8554/" << config.Hostname; //
	return oss.str();
}

} // namespace artemis
} // namespace fort
