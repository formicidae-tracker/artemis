#include "EuresysFrameGrabber.h"


EuresysFrameGrabber::EuresysFrameGrabber(Euresys::EGenTL & gentl,
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

void EuresysFrameGrabber::Start() {
	start();
}

EuresysFrameGrabber::~EuresysFrameGrabber() {
}

const Frame::Ptr & EuresysFrameGrabber::CurrentFrame() {
	return d_currentFrame;
}

void EuresysFrameGrabber::onNewBufferEvent(const Euresys::NewBufferData &data) {
	d_currentFrame = std::make_shared<EuresysFrame>(*this,data);
	d_manager->Signal(Event::FRAME_READY);
}

EuresysFrame::EuresysFrame(Euresys::EGrabber<Euresys::CallbackSingleThread> & grabber, const Euresys::NewBufferData & data)
	: Euresys::ScopedBuffer(grabber,data)
	, d_width(getInfo<size_t>(GenTL::BUFFER_INFO_WIDTH))
	, d_height(getInfo<size_t>(GenTL::BUFFER_INFO_HEIGHT))
	, d_timestamp(getInfo<uint64_t>(GenTL::BUFFER_INFO_TIMESTAMP))
	, d_ID(getInfo<uint64_t>(GenTL::BUFFER_INFO_FRAMEID))
	, d_mat(d_height,d_width,CV_8U,getInfo<void*>(GenTL::BUFFER_INFO_BASE)) {
}

EuresysFrame::~EuresysFrame() {}

size_t EuresysFrame::Width() const {
	return d_width;
}
size_t EuresysFrame::Height() const {
	return d_height;
}

uint64_t EuresysFrame::Timestamp() const {
	return d_timestamp;
}

uint64_t EuresysFrame::ID() const {
	return d_ID;
}

const cv::Mat & EuresysFrame::ToCV() {
	return d_mat;
}
