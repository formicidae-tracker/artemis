#pragma once


#include <EGrabber.h>

#include "EventManager.h"




struct CameraConfiguration {
	double FPS;
	uint32_t ExposureTime;
	uint32_t StrobeDuration;
	int32_t  StrobeOffset;
};

class FrameGrabber : public Euresys::EGrabber<Euresys::CallbackSingleThread> {
public :
	typedef std::shared_ptr<Euresys::ScopedBuffer> BufferPtr;

	FrameGrabber(Euresys::EGenTL & gentl,
	             const CameraConfiguration & cameraConfig,
	             const EventManager::Ptr & eManager);

	~FrameGrabber();

	void Start();

	const BufferPtr & CurrentFrame();

private:

	virtual void onNewBufferEvent(const Euresys::NewBufferData &data);

	EventManager::Ptr d_manager;
	BufferPtr d_currentFrame;
};
