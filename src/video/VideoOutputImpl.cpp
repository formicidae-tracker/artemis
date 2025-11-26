#include "VideoOutputImpl.hpp"
#include "video/FilePipeline.hpp"
#include "video/StreamPipeline.hpp"
#include "video/gstreamer.hpp"

#include <cpptrace/exceptions.hpp>
#include <glib.h>
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <gst/gstelement.h>
#include <gst/rtsp/gstrtsptransport.h>

using namespace std::chrono_literals;

namespace fort {
namespace artemis {

std::string GetHostname() {
	char buffer[1024];
	if (gethostname(buffer, 1024) != 0) {
		throw cpptrace::runtime_error("could not get hostname");
	}
	return buffer;
}

VideoOutputImpl::VideoOutputImpl(
    const VideoOutputOptions &options, const VideoOutput::Config &config
)
    : d_timeout{config.ConnectionTimeout}
    , d_streamConfig{StreamPipeline::Config{
          .Host             = options.Host,
          .Hostname         = GetHostname(),
          .OnStreamError    = [this]() { onStreamError(); },
          .InputResolution  = config.InputResolution,
          .Height           = std::clamp(options.StreamHeight, 240UL, 1080UL),
          .InputBuffer      = config.InputBuffer,
          .FPS              = config.FPS,
          .EnforceVideoRate = config.EnforceStreamVideoRate,
      }}
    , d_logger{slog::With(slog::String("task", "VideoOutput"))} {

	EnsureGSTInitialized();

	if (options.Host.empty() && options.OutputDir.empty()) {
		throw cpptrace::runtime_error{"Host and OutputDir cannot both be empty"
		};
	}
	if (options.OutputDir.empty() == false) {
		d_filePipeline = std::make_shared<FilePipeline>(options, config);
	}
	if (options.Host.empty() == false) {
		d_streamPipeline = std::make_unique<StreamPipeline>(d_streamConfig);
		d_streamPipeline->SetState(GST_STATE_PLAYING);
	}
}

VideoOutputImpl::~VideoOutputImpl() {
#ifndef NDEBUG
	if (d_filePipeline) {
		d_filePipeline->PrintDebug("/tmp/video-file-pipeline.dot");
	}
#endif
	d_closing.store(true);

	d_filePipeline.reset();

	struct Context {
		VideoOutputImpl  *self;
		std::atomic<bool> done{false};
		Context(VideoOutputImpl *self_)
		    : self{self_} {};
	};

	auto ctx = std::make_unique<Context>(this);
	g_main_context_invoke(
	    g_main_context_default(),
	    [](gpointer userdata) {
		    auto context = reinterpret_cast<Context *>(userdata);
		    Defer {
			    context->done.store(true);
			    context->done.notify_all();
		    };
		    if (context->self->d_reconnectionSchedule != 0) {
			    g_source_remove(context->self->d_reconnectionSchedule);
		    }
		    return G_SOURCE_REMOVE;
	    },
	    ctx.get()
	);
	ctx->done.wait(false);
}

bool VideoOutputImpl::PushFrame(const Frame::Ptr &frame) {
	bool res{false};
	if (d_filePipeline) {
		res = d_filePipeline->PushFrame(frame, d_filePipeline);
	}
	Lock lock{d_reconfiguration};
	if (d_streamPipeline) {
		auto resStream = d_streamPipeline->PushFrame(frame);
		if (d_filePipeline == nullptr) {
			res = resStream;
		}
	}
	return res;
}

void VideoOutputImpl::onStreamError() {
	if (d_closing.load() == true) {
		return;
	}
	disconnectStream();
	scheduleReconnect();
}

void VideoOutputImpl::disconnectStream() {
	Lock lock{d_reconfiguration};
	if (d_streamPipeline == nullptr) {
		return;
	}
	d_streamPipeline.reset();
	d_logger.Info(
	    "disconnected",
	    slog::Int("reconnections", d_reconnections.load())
	);
}

void VideoOutputImpl::reconnectStream() {
	if (d_closing.load() == true) {
		return;
	}
	d_reconnections.fetch_add(1);
	auto logger =
	    d_logger.With(slog::Int("reconnections", d_reconnections.load()));
	Defer {
		logger.Info(
		    "connecting",
		    slog::Int("reconnections", d_reconnections.load())
		);
	};
	std::unique_ptr<StreamPipeline> newStream;

	try {
		newStream = std::make_unique<StreamPipeline>(d_streamConfig);
	} catch (const std::exception &e) {
		logger.Error("could not reconnect", slog::Err(e));
		scheduleReconnect();
		return;
	}
	Lock lock{d_reconfiguration};
	if (d_closing.load() == true) {
		return;
	}
	d_streamPipeline = std::move(newStream);
	d_streamPipeline->SetState(GST_STATE_PLAYING);
}

void VideoOutputImpl::scheduleReconnect() {
	if (d_reconnectionSchedule != 0) {
		return;
	}
	if (d_reconnections.load() >= d_timeout.MaxRetries) {
		d_logger.Warn("Not reconnecting as maximum reconnection reached");
		return;
	}

	auto timeout = d_timeout.ForRetry(d_reconnections.load());

	d_logger.Info(
	    "scheduling reconnect",
	    slog::Int("reconnections", d_reconnections.load()),
	    slog::Duration("timeout", timeout.ToChrono())
	);
	d_reconnectionSchedule = g_timeout_add(
	    guint(timeout.Milliseconds()),
	    [](gpointer userdata) {
		    auto self = reinterpret_cast<VideoOutputImpl *>(userdata);
		    std::thread([self]() { self->reconnectStream(); }).detach();
		    self->d_reconnectionSchedule = 0;
		    return G_SOURCE_REMOVE;
	    },
	    this
	);
}

} // namespace artemis
} // namespace fort
