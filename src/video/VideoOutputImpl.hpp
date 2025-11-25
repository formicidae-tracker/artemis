#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <video/gstreamer.hpp>

#include <VideoOutput.hpp>

#include <Options.hpp>

#include "video/FilePipeline.hpp"
#include "video/StreamPipeline.hpp"

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

	inline VideoOutput::Stats GetStats() const {
		if (d_filePipeline) {
			return {
			    .Processed     = d_filePipeline->d_processed.load(),
			    .Dropped       = d_filePipeline->d_dropped.load(),
			    .Reconnections = d_reconnections.load()
			};
		}
		return {
		    .Processed     = 0,
		    .Dropped       = 0,
		    .Reconnections = d_reconnections.load()
		};
	}

	inline size_t InflightBufferSize() const {
		return 1 + (d_filePipeline != nullptr ? 2 : 0) +
		       (d_streamConfig.Host.empty() ? 0 : 2);
	}

private:
	void onStreamError();

	void disconnectStream();
	void reconnectStream();
	void scheduleReconnect();

	using Mutex = std::mutex;
	using Lock  = std::lock_guard<std::mutex>;

	const VideoOutput::ExponentialTimeoutConfig d_timeout;

	const StreamPipeline::Config d_streamConfig;

	slog::Logger<1> d_logger;

	std::atomic<size_t> d_reconnections{0};
	gulong              d_reconnectionSchedule{0};

	Mutex d_reconfiguration;

	std::shared_ptr<FilePipeline>   d_filePipeline;
	std::unique_ptr<StreamPipeline> d_streamPipeline;
	std::atomic<bool>               d_closing{false};
};

} // namespace artemis
} // namespace fort
