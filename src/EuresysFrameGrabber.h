#pragma once

#include "FrameGrabber.h"

#include <EGrabber.h>

#include <opencv2/core/core.hpp>

struct CameraConfiguration {
	CameraConfiguration()
		: FPS(20.0)
		, StrobeDuration(2000)
		, StrobeDelay(0)
		, Slave(false) {}

	double FPS;
	uint32_t StrobeDuration;
	int32_t  StrobeDelay;
	bool     Slave;
};


class EuresysFrame : public Frame,public Euresys::ScopedBuffer {
public :
	EuresysFrame(Euresys::EGrabber<Euresys::CallbackOnDemand> & grabber, const Euresys::NewBufferData &);
	virtual ~EuresysFrame();


	virtual void * Data();
	virtual size_t Width() const;
	virtual size_t Height() const;
	virtual uint64_t Timestamp() const;
	virtual uint64_t ID() const;
	const cv::Mat & ToCV();
private :
	size_t d_width,d_height;
	uint64_t d_timestamp,d_ID;
	cv::Mat d_mat;
};


class EuresysFrameGrabber : public FrameGrabber,public Euresys::EGrabber<Euresys::CallbackOnDemand> {
public :
	typedef std::shared_ptr<Euresys::ScopedBuffer> BufferPtr;

	EuresysFrameGrabber(Euresys::EGenTL & gentl,
	                    const CameraConfiguration & cameraConfig);

	virtual ~EuresysFrameGrabber();


	virtual void Start();
	virtual void Stop();
	virtual Frame::Ptr NextFrame();

private:

	virtual void onNewBufferEvent(const Euresys::NewBufferData &data);

	Frame::Ptr         d_frame;
};
