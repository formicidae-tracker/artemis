#pragma once

#include "FrameGrabber.h"

#include <EGrabber.h>

#include "EventManager.h"

#include <opencv2/core.hpp>



struct CameraConfiguration {
	CameraConfiguration()
		: FPS(20.0)
		, ExposureTime(2500)
		, StrobeDuration(2000)
		, StrobeOffset(0)
		, Slave(false) {}

	double FPS;
	uint32_t ExposureTime;
	uint32_t StrobeDuration;
	int32_t  StrobeOffset;
	bool     Slave;
};


class EuresysFrame : public Frame,public Euresys::ScopedBuffer {
public :
	EuresysFrame(Euresys::EGrabber<Euresys::CallbackSingleThread> & grabber, const Euresys::NewBufferData &);
	virtual ~EuresysFrame();


	virtual void * Data() const;
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


class EuresysFrameGrabber : public FrameGrabber,public Euresys::EGrabber<Euresys::CallbackSingleThread> {
public :
	typedef std::shared_ptr<Euresys::ScopedBuffer> BufferPtr;

	EuresysFrameGrabber(Euresys::EGenTL & gentl,
	                    const CameraConfiguration & cameraConfig,
	                    const EventManager::Ptr & eManager);

	virtual ~EuresysFrameGrabber();

	virtual void Start();

	virtual const Frame::Ptr & CurrentFrame();

private:

	virtual void onNewBufferEvent(const Euresys::NewBufferData &data);

	EventManager::Ptr d_manager;
	Frame::Ptr d_currentFrame;
};
