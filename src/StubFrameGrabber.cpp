#include "StubFrameGrabber.h"

#include <thread>

#include <opencv2/imgcodecs.hpp>

StubFrame::StubFrame(const cv::Mat & mat, uint64_t timestamp, uint64_t ID)
	: d_mat(mat.clone())
	, d_timestamp(timestamp)
	, d_ID(ID) {
}

StubFrame::~StubFrame() {}

void * StubFrame::Data() {
	return const_cast<void*>(reinterpret_cast<const void *>(d_mat.datastart));
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


StubFrameGrabber::StubFrameGrabber(const std::string & path)
	: d_ID(0)
	, d_timestamp(0) {
	d_image = cv::imread(path,0);
}

StubFrameGrabber::~StubFrameGrabber() {
}

void StubFrameGrabber::Start() {
	d_ID = 0;
	d_timestamp = 0;
	d_last = clock::now();
}

void StubFrameGrabber::Stop() {
}

Frame::Ptr StubFrameGrabber::NextFrame() {
	d_last += std::chrono::milliseconds(100);
	std::this_thread::sleep_until(d_last);

	Frame::Ptr res = std::make_shared<StubFrame>(d_image,d_timestamp,d_ID);
	d_timestamp += 100000;
	d_ID += 1;
	return res;
}
