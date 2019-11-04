#pragma once

#include "FrameGrabber.h"
#include "CameraConfiguration.h"

#include <EGrabber.h>

#include <opencv2/core/core.hpp>
#include <mutex>



class EuresysFrame : public Frame,public Euresys::ScopedBuffer {
public :
	EuresysFrame(Euresys::EGrabber<Euresys::CallbackOnDemand> & grabber, const Euresys::NewBufferData &, uint64_t & lastFrame, uint64_t & toAdd);
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
	friend class EuresysFrameGrabbero;
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

	std::mutex         d_mutex;
	Frame::Ptr         d_frame;
	uint64_t           d_lastFrame;
	uint64_t           d_toAdd;
};
