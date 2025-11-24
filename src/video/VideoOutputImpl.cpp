#include "VideoOutputImpl.hpp"
#include "video/FilePipeline.hpp"
#include "video/StreamPipeline.hpp"
#include "video/gstreamer.hpp"

#include <cpptrace/exceptions.hpp>
#include <glib.h>
#include <gst/app/gstappsrc.h>
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
    : d_streamConfig{
			.Host = options.Host,
			.Hostname = GetHostname(),
			.OnStreamError = [this]() { onStreamError();},
			.InputResolution = config.InputResolution,
			.Height = std::clamp(options.StreamHeight,240UL,1080UL),
			.InputBuffer = config.InputBuffer,
			.FPS = config.FPS,
			.EnforceVideoRate = config.EnforceStreamVideoRate,
		}
	, d_logger{slog::With(slog::String("task","VideoOutput"))}
	, d_reconnectTimeout{config.ConnectionTimeout} {

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
	if (d_startingTimestamp_us.has_value() == false) {
		d_startingTimestamp_us = frame->Timestamp();
	}

	auto buffer = FilePipeline::PrepareFrame(
	    frame,
	    d_filePipeline,
	    d_startingTimestamp_us.value()
	);

	bool res;
	if (d_filePipeline) {
		res = d_filePipeline->PushBuffer(buffer.get());
	}
	Lock lock{d_reconfiguration};
	if (d_streamPipeline) {
		d_streamPipeline->PushBuffer(buffer.get());
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
	d_streamPipeline.reset();
	d_logger.Info(
	    "disconnected",
	    slog::Int("reconnections", d_reconnections.load()),
	    slog::Duration(
	        "reconnection_timeout",
	        std::chrono::nanoseconds{d_reconnectTimeout.Nanoseconds()}
	    )
	);
}

void VideoOutputImpl::reconnectStream() {
	d_reconnections.fetch_add(1);
	d_logger.DInfo(
	    "starting connecting",
	    slog::Int("reconnections", d_reconnections.load())
	);
	Defer {
		d_logger.Info(
		    "connecting",
		    slog::Int("reconnections", d_reconnections.load())
		);
	};
	std::unique_ptr<StreamPipeline> newStream;

	try {
		newStream = std::make_unique<StreamPipeline>(d_streamConfig);
	} catch (const std::exception &e) {
		d_logger.Error("could not reconnect", slog::Err(e));
		scheduleReconnect();
		return;
	}
	Lock lock{d_reconfiguration};
	d_streamPipeline = std::move(newStream);
}

void VideoOutputImpl::scheduleReconnect() {
	if (d_reconnectionSchedule != 0) {
		return;
	}
	d_logger.Info(
	    "scheduling reconnect",
	    slog::Int("reconnections", d_reconnections.load())
	);
	d_reconnectionSchedule = g_timeout_add(
	    guint(d_reconnectTimeout.Milliseconds()),
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
