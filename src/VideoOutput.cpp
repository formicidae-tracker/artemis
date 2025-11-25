#include "VideoOutput.hpp"

#include <video/VideoOutputImpl.hpp>

namespace fort {
namespace artemis {

VideoOutput::VideoOutput(
    const VideoOutputOptions &options, const Config &config
)
    : d_impl{std::make_unique<VideoOutputImpl>(options, config)} {}

VideoOutput::~VideoOutput()                                  = default;
VideoOutput::VideoOutput(VideoOutput &&) noexcept            = default;
VideoOutput &VideoOutput::operator=(VideoOutput &&) noexcept = default;

void VideoOutput::Close() {
	d_impl.reset();
}

bool VideoOutput::PushFrame(const Frame::Ptr &frame) {
	return d_impl->PushFrame(frame);
}

VideoOutput::Stats VideoOutput::GetStats() const {
	return d_impl->GetStats();
}

size_t VideoOutput::InflightBufferSize() const {
	return d_impl->InflightBufferSize();
}

Duration VideoOutput::ExponentialTimeoutConfig::ForRetry(size_t n) const {
	return std::min(
	    int(Base.Milliseconds() * std::pow(Multiplier, n)) *
	        Duration::Millisecond,
	    Max
	);
}

} // namespace artemis
} // namespace fort
