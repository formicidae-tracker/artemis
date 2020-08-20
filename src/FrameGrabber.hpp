#pragma once

#include <memory>

#include "Time.hpp"



namespace cv{
class Mat;
}

namespace fort {
namespace artemis {

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
	const fort::artemis::Time & Time() const;
private:

	fort::artemis::Time d_time;
};

class FrameGrabber {
public :
	typedef std::shared_ptr<FrameGrabber> Ptr;

	virtual ~FrameGrabber();

	virtual void Start() = 0;
	virtual void Stop() = 0;

	virtual Frame::Ptr NextFrame() = 0;

	virtual std::pair<int32_t,int32_t> GetResolution() = 0;
};

} // namespace artemis
} // namespace fort
