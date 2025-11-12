#include "VideoOutput.hpp"
#include "Options.hpp"

namespace fort {

namespace artemis {

class VideoOutputImpl {
public:
	VideoOutputImpl(const VideoOutputOptions &options);

	~VideoOutputImpl();

	VideoOutputImpl(const VideoOutputImpl &)            = delete;
	VideoOutputImpl(VideoOutputImpl &&)                 = delete;
	VideoOutputImpl &operator=(const VideoOutputImpl &) = delete;
	VideoOutputImpl &operator=(VideoOutputImpl &&)      = delete;

	void PushFrame(const Frame::Ptr &frame);
};

VideoOutput::VideoOutput(const VideoOutputOptions &options)
    : d_impl{std::make_unique<VideoOutputImpl>(options)} {}

VideoOutput::~VideoOutput()                                  = default;
VideoOutput::VideoOutput(VideoOutput &&) noexcept            = default;
VideoOutput &VideoOutput::operator=(VideoOutput &&) noexcept = default;

void VideoOutput::Close() {
	d_impl.reset();
}

void VideoOutput::PushFrame(const Frame::Ptr &frame) {
	d_impl->PushFrame(frame);
}

VideoOutputImpl::VideoOutputImpl(const VideoOutputOptions &options) {}

VideoOutputImpl::~VideoOutputImpl() {}

void VideoOutputImpl::PushFrame(const Frame::Ptr &frame) {}

} // namespace artemis
} // namespace fort
