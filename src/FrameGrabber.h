#pragma once


#include <EGrabber.h>

#include "EventManager.h"

#include <opencv2/core.hpp>



struct CameraConfiguration {
	double FPS;
	uint32_t ExposureTime;
	uint32_t StrobeDuration;
	int32_t  StrobeOffset;
};


class Frame : public Euresys::ScopedBuffer {
public :
	typedef std::shared_ptr<Frame> Ptr;

	Frame(Euresys::EGrabber<Euresys::CallbackSingleThread> & grabber, const Euresys::NewBufferData &);
	virtual ~Frame();


	void * Data() const;
	size_t Width() const;
	size_t Height() const;
	uint64_t Timestamp() const;
	uint64_t ID() const;
	const cv::Mat & ToCV();
private :
	size_t d_width,d_height;
	uint64_t d_timestamp,d_ID;
	cv::Mat d_mat;
};


class FrameGrabber : public Euresys::EGrabber<Euresys::CallbackSingleThread> {
public :
	typedef std::shared_ptr<Euresys::ScopedBuffer> BufferPtr;

	FrameGrabber(Euresys::EGenTL & gentl,
	             const CameraConfiguration & cameraConfig,
	             const EventManager::Ptr & eManager);

	virtual ~FrameGrabber();

	void Start();

	const Frame::Ptr & CurrentFrame();

private:

	virtual void onNewBufferEvent(const Euresys::NewBufferData &data);

	EventManager::Ptr d_manager;
	Frame::Ptr d_currentFrame;
};
