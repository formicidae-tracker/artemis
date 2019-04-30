#pragma once

#include <memory>

#include <sys/time.h>

#include <opencv2/core.hpp>


class Frame {
public:
	typedef std::shared_ptr<Frame> Ptr;

	Frame();
	virtual ~Frame();

	virtual void * Data() = 0;
	virtual size_t Width() const = 0;
	virtual size_t Height() const = 0;
	virtual uint64_t Timestamp() const = 0;
	virtual uint64_t ID() const = 0;
	virtual const cv::Mat & ToCV() = 0;
	const struct timeval & Time() const;
private:

	struct timeval d_time;
};

class FrameGrabber {
public :
	virtual ~FrameGrabber();

	virtual cv::Size  FrameSize() = 0;

	virtual void Start() = 0;
	virtual void Stop() = 0;

	virtual Frame::Ptr NextFrame() = 0;
};
