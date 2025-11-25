
#pragma once

#include "FrameGrabber.hpp"
#include "fort/time/Time.hpp"

namespace fort {
namespace artemis {
class VideoOutputOptions;

class VideoOutputImpl;

class VideoOutput {
public:
	struct ExponentialTimeoutConfig {
		Duration Base       = 500 * Duration::Millisecond;
		Duration Max        = 5 * Duration::Minute;
		size_t   MaxRetries = 40;
		float    Multiplier = 1.5;

		Duration ForRetry(size_t retry) const;
	};

	struct Config {
		float FPS = 10.0;
		Size  InputResolution;
		bool  LeakyPush = true;

		ExponentialTimeoutConfig ConnectionTimeout;

		bool   EnforceStreamVideoRate = false;
		size_t InputBuffer            = 1;
	};

	VideoOutput(const VideoOutputOptions &options, const Config &config);
	~VideoOutput();
	VideoOutput(VideoOutput &&) noexcept;
	VideoOutput &operator=(VideoOutput &&) noexcept;

	// Close the stream and wait for it to be closed.
	void Close();

	size_t InflightBufferSize() const;

	bool PushFrame(const Frame::Ptr &frame);

	struct Stats {
		uint64_t Processed{0}, Dropped{0}, Reconnections{0};
	};

	Stats GetStats() const;

private:
	std::unique_ptr<VideoOutputImpl> d_impl;
};
} // namespace artemis
} // namespace fort
