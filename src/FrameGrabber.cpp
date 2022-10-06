#include "FrameGrabber.hpp"


namespace fort {
namespace artemis {

Frame::Frame() {
	d_time = fort::Time::Now();
}

Frame::~Frame() {}

FrameGrabber::~FrameGrabber() {}


const fort::Time & Frame::Time() const {
	return d_time;
}


} // namespace artemis
} // namespace fort
