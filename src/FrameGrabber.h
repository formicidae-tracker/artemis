#pragma once

#include <memory>

namespace cv{
class Mat;
}

class Frame {
public:
	typedef std::shared_ptr<Frame> Ptr;

	virtual ~Frame();

	virtual void * Data() const =0;
	virtual size_t Width() const =0;
	virtual size_t Height() const =0;
	virtual uint64_t Timestamp() const =0;
	virtual uint64_t ID() const =0;
	virtual const cv::Mat & ToCV() =0;
};

class FrameGrabber {
public :
	virtual ~FrameGrabber();

	virtual void Start() = 0;

	virtual const Frame::Ptr & CurrentFrame() = 0;
};
