#pragma once

#include "FrameGrabber.hpp"

namespace fort {
namespace artemis {

class StubFrame : public Frame {
public:
	StubFrame(const ImageU8 &image, uint64_t ID);
	virtual ~StubFrame();

	virtual void    *Data() override;
	virtual size_t   Width() const override;
	virtual size_t   Height() const override;
	virtual uint64_t Timestamp() const override;
	virtual uint64_t ID() const override;
	ImageU8          ToImageU8() override;

private:
	uint64_t d_ID;
	ImageU8  d_image;
};

class StubFrameGrabber : public FrameGrabber {
public:
	StubFrameGrabber(const std::vector<std::string> &paths, double FPS);

	virtual ~StubFrameGrabber();

	void       Start() override;
	void       Stop() override;
	Frame::Ptr NextFrame() override;

	void AbordPending() override {}

	Size Resolution() const override;

private:
	typedef std::chrono::high_resolution_clock clock;
	typedef clock::time_point                  time;
	std::vector<ImageU8::OwnedPtr>             d_images;
	uint64_t                                   d_ID, d_timestamp;
	Time                                       d_last;
	Duration                                   d_period;
};

} // namespace artemis
} // namespace fort
