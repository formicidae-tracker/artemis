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

const Frame::Ptr & FrameGrabber::CurrentFrame() {
	return d_currentFrame;
}

void FrameGrabber::onNewBufferEvent(const Euresys::NewBufferData &data) {
	d_currentFrame = std::make_shared<Frame>(*this,data);
	d_manager->Signal(EventManager::FRAME_READY);
}

Frame::Frame(Euresys::EGrabber<Euresys::CallbackSingleThread> & grabber, const Euresys::NewBufferData & data)
	: Euresys::ScopedBuffer(grabber,data)
	, d_width(getInfo<size_t>(GenTL::BUFFER_INFO_WIDTH))
	, d_height(getInfo<size_t>(GenTL::BUFFER_INFO_HEIGHT))
	, d_timestamp(getInfo<uint64_t>(GenTL::BUFFER_INFO_TIMESTAMP))
	, d_ID(getInfo<uint64_t>(GenTL::BUFFER_INFO_FRAMEID))
	, d_mat(d_height,d_width,CV_8U,getInfo<void*>(GenTL::BUFFER_INFO_BASE)) {
}

Frame::~Frame() {}

size_t Frame::Width() const {
	return d_width;
}
size_t Frame::Height() const {
	return d_height;
}

uint64_t Frame::Timestamp() const {
	return d_timestamp;
}

uint64_t Frame::ID() const {
	return d_ID;
}

const cv::Mat & Frame::ToCV() {
	return d_mat;
}
