#pragma once

#include "FrameGrabber.hpp"

namespace fort {
namespace artemis {
class VideoOutputOptions;

class VideoOutputImpl;

class VideoOutput {
public:
	VideoOutput(
	    const VideoOutputOptions &options,
	    const Size               &inputResolution,
	    float                     FPS
	);
	~VideoOutput();
	VideoOutput(VideoOutput &&) noexcept;
	VideoOutput &operator=(VideoOutput &&) noexcept;

	// Close the stream and wait for it to be closed.
	void Close();

	void PushFrame(const Frame::Ptr &frame);

	struct Stats {
		uint64_t Processed{0}, Dropped{0};
	};

	Stats GetStats() const;

private:
	std::unique_ptr<VideoOutputImpl> d_impl;
};
} // namespace artemis
} // namespace fort
