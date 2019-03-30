#include "EuresysFrameGrabber.h"


EuresysFrameGrabber::EuresysFrameGrabber(Euresys::EGenTL & gentl,
                                         const CameraConfiguration & cameraConfig)
	: Euresys::EGrabber<Euresys::CallbackOnDemand>(gentl) {

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

void EuresysFrameGrabber::Stop() {
	start();
}


EuresysFrameGrabber::~EuresysFrameGrabber() {
}

Frame::Ptr EuresysFrameGrabber::NextFrame() {
	processEvent<Euresys::NewBufferData>(1000);
	Frame::Ptr res = d_frame;
	d_frame.reset();
	return res;
}

void EuresysFrameGrabber::onNewBufferEvent(const Euresys::NewBufferData &data) {
	d_frame = std::make_shared<EuresysFrame>(*this,data);
}

EuresysFrame::EuresysFrame(Euresys::EGrabber<Euresys::CallbackOnDemand> & grabber, const Euresys::NewBufferData & data)
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

void * EuresysFrame::Data() {
	return getInfo<void*>(GenTL::BUFFER_INFO_BASE);
}
