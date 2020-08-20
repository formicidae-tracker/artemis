#include "FrameGrabber.h"


namespace fort {
namespace artemis {

Frame::Frame() {
	gettimeofday(&d_time,NULL);
}

Frame::~Frame() {}

FrameGrabber::~FrameGrabber() {}


const struct timeval & Frame::Time() const {
	return d_time;
}


} // namespace artemis
} // namespace fort
