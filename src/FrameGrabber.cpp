#include "FrameGrabber.h"


FrameGrabber::FrameGrabber(Euresys::EGenTL & gentl,
                           const CameraConfiguration & cameraConfig,
                           const EventManager::Ptr & eManager)
	: Euresys::EGrabber<Euresys::CallbackSingleThread>(gentl)
	, d_manager(eManager) {

	using namespace Euresys;

	setFloat<RemoteModule>("AcquisitionFrameRate",cameraConfig.FPS);
	setString<DeviceModule>("CameraControlMethod","RG");
	setInteger<DeviceModule>("ExposureTime",cameraConfig.ExposureTime);
	setInteger<DeviceModule>("StrobeDuration",cameraConfig.StrobeDuration);

	setString<InterfaceModule>("LineSelector","IOUT11");
	setString<InterfaceModule>("LineInverter","True");
	setString<InterfaceModule>("LineSource","Device0Stribe");

	enableEvent<NewBufferData>();
	reallocBuffers(4);
}

void FrameGrabber::Start() {
	start();
}

FrameGrabber::~FrameGrabber() {
}

const FrameGrabber::BufferPtr & FrameGrabber::CurrentFrame() {
	return d_currentFrame;
}

void FrameGrabber::onNewBufferEvent(const Euresys::NewBufferData &data) {
	d_currentFrame = std::make_shared<Euresys::ScopedBuffer>(*this,data);
	d_manager->Signal(EventManager::FRAME_READY);
}
