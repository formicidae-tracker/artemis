#include "StubFrameGrabber.hpp"

#include <thread>

#include <opencv2/highgui/highgui.hpp>

#include <unistd.h>

#include <glog/logging.h>

namespace fort {
namespace artemis {

StubFrame::StubFrame(const cv::Mat & mat, uint64_t ID)
	: d_mat(mat.clone())
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
	return Time().MonotonicValue() / 1000;
}

uint64_t StubFrame::ID() const {
	return d_ID;
}

const cv::Mat & StubFrame::ToCV() {
	return d_mat;
}


StubFrameGrabber::StubFrameGrabber(const std::vector<std::string> & paths,
                                   double FPS)
	: d_ID(0)
	, d_timestamp(0)
	, d_period(1.0e9 / FPS) {
	for ( const auto & p : paths ) {
		d_images.push_back(cv::imread(p,0));
		if ( d_images.back().data == NULL ) {
			throw std::runtime_error("Could not load '" + p + "'");
		}
		if ( d_images.back().size() != d_images[0].size() ) {
			throw std::runtime_error("'" + paths[0] + "' and '" + p + "' have different sizes");
		}
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
	return d_images.front().size();
}

Frame::Ptr StubFrameGrabber::NextFrame() {
	auto toWait = d_last.Add(d_period).Sub(Time::Now());
	if ( toWait > 0 ) {
		usleep(toWait.Microseconds());
	}

	Frame::Ptr res = std::make_shared<StubFrame>(d_images[d_ID % d_images.size()],d_ID);
	d_ID += 1;
	d_last = res->Time();
	return res;
}

} // namespace artemis
} // namespace fort
