#pragma once

#include <video/gstreamer.hpp>

#include <VideoOutput.hpp>

#include <Options.hpp>

#include "MetadataHandler.hpp"

namespace fort {
namespace artemis {
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
	gulong    d_streamParseSrcProbe{0};

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

} // namespace artemis
} // namespace fort
