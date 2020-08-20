#include "FrameGrabber.hpp"


namespace fort {
namespace artemis {

Frame::Frame() {
	d_time = fort::artemis::Time::Now();
}

Frame::~Frame() {}

FrameGrabber::~FrameGrabber() {}


const fort::artemis::Time & Frame::Time() const {
	return d_time;
}


} // namespace artemis
} // namespace fort
