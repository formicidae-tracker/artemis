#pragma once

#include <opencv2/core.hpp>

#include <chrono>

#include "FrameGrabber.h"

class StubFrame : public Frame {
public :
	StubFrame(const cv::Mat & mat, uint64_t timestamp, uint64_t ID);
	virtual ~StubFrame();


	virtual void * Data();
	virtual size_t Width() const;
	virtual size_t Height() const;
	virtual uint64_t Timestamp() const;
	virtual uint64_t ID() const;
	const cv::Mat & ToCV();
private :
	uint64_t d_timestamp,d_ID;
	cv::Mat d_mat;
};


class StubFrameGrabber : public FrameGrabber {
public :
	StubFrameGrabber(const std::string & path);

	virtual ~StubFrameGrabber();

	virtual cv::Size  FrameSize();

	virtual void Start();
	virtual void Stop();
	virtual Frame::Ptr NextFrame();

private:
	typedef std::chrono::high_resolution_clock clock;
	typedef clock::time_point time;
	cv::Mat  d_image;
	uint64_t d_ID,d_timestamp;
	time     d_last;
};
