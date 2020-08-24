#include "StubFrameGrabber.hpp"

#include <thread>

#include <opencv2/highgui/highgui.hpp>

#include <unistd.h>

#include <glog/logging.h>

namespace fort {
namespace artemis {

StubFrame::StubFrame(const cv::Mat & mat, uint64_t ID)
	: d_mat(mat.clone())
	, d_timestamp(Time().MonotonicValue() / 1000)
	, d_ID(ID) {
}

StubFrame::~StubFrame() {}

void * StubFrame::Data() {
	return d_mat.data;
}

size_t StubFrame::Width() const {
	return d_mat.cols;
}

size_t StubFrame::Height() const {
	return d_mat.rows;
}

uint64_t StubFrame::Timestamp() const {
	return d_timestamp;
}

uint64_t StubFrame::ID() const {
	return d_ID;
}

const cv::Mat & StubFrame::ToCV() {
	return d_mat;
}


StubFrameGrabber::StubFrameGrabber(const std::string & path,
                                   double FPS)
	: d_ID(0)
	, d_timestamp(0)
	, d_period(1.0e9 / FPS) {
	d_image = cv::imread(path,0);
	if ( d_image.data == NULL ) {
		throw std::runtime_error("Could not load '" + path + "'");
	}
}

StubFrameGrabber::~StubFrameGrabber() {
}

void StubFrameGrabber::Start() {
	d_last = Time::Now().Add(-d_period);
}

void StubFrameGrabber::Stop() {
}

cv::Size StubFrameGrabber::Resolution() const {
	return d_image.size();
}

Frame::Ptr StubFrameGrabber::NextFrame() {
	auto toWait = d_last.Add(d_period).Sub(Time::Now());
	if ( toWait > 0 ) {
		usleep(toWait.Microseconds());
	}

	Frame::Ptr res = std::make_shared<StubFrame>(d_image,d_ID);
	d_ID += 1;
	d_last = res->Time();
	return res;
}

} // namespace artemis
} // namespace fort
