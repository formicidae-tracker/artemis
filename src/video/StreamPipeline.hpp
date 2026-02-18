#pragma once

#include "Options.hpp"
#include "VideoOutput.hpp"
#include "video/BusManagedPipeline.hpp"
#include "video/gstreamer.hpp"
#include <glib.h>
#include <slog++/Logger.hpp>

namespace fort {
namespace artemis {

class StreamPipeline : public BusManagedPipeline {
public:
	struct Config {
		std::string           Host;
		std::string           Hostname;
		std::function<void()> OnStreamError;
		Size                  InputResolution;
		size_t                Height;
		size_t                InputBuffer;
		double                FPS;
		bool                  EnforceVideoRate;
		int                   Bitrate_Kb = 1000;
		std::string           Address() const;
	};

	StreamPipeline(const Config &config);
	virtual ~StreamPipeline();

	StreamPipeline(const StreamPipeline &)            = delete;
	StreamPipeline(StreamPipeline &&)                 = delete;
	StreamPipeline &operator=(const StreamPipeline &) = delete;
	StreamPipeline &operator=(StreamPipeline &&)      = delete;

	bool PushFrame(const Frame::Ptr &frame);

protected:
	void OnMessage(GstBus *bus, GstMessage *message) override;

private:
	friend class VideoOutputImpl;

	static std::string    buildPipelineDescription(const Config &config);
	slog::Logger<1>       d_logger;
	std::function<void()> d_onStreamError;
	std::atomic<bool>     d_closing{false};

	GstElementPtr           d_inputSrc;
	std::optional<uint64_t> d_firstTimestamp_us;
};
} // namespace artemis
} // namespace fort
