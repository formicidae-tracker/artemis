#pragma once

#include "FrameGrabber.hpp"
#include "fort/time/Time.hpp"

namespace fort {
namespace artemis {
class VideoOutputOptions;

class VideoOutputImpl;

class VideoOutput {
public:
	struct Config {
		Duration FilePeriod = 2 * Duration::Hour;
		float    FPS        = 10.0;
		Size     InputResolution;
		bool     LeakyPush              = true;
		Duration ConnectionTimeout      = 5 * Duration::Second;
		bool     EnforceStreamVideoRate = true;
		size_t   InputBuffer            = 1;
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
