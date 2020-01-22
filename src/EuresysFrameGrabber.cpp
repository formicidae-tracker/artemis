#include "EuresysFrameGrabber.h"

#include <glog/logging.h>
#include <regex>

EuresysFrameGrabber::EuresysFrameGrabber(Euresys::EGenTL & gentl,
                                         const CameraConfiguration & cameraConfig)
	: Euresys::EGrabber<Euresys::CallbackOnDemand>(gentl)
	, d_lastFrame(0)
	, d_toAdd(0)
	, d_width(0)
	, d_height(0) {

	using namespace Euresys;

	std::string ifID = getString<InterfaceModule>("InterfaceID");
	std::regex slaveRx("df-camera");
	bool isMaster = !std::regex_search(ifID,slaveRx) ;

	if ( isMaster == true ) {
		d_width = getInteger<RemoteModule>("Width");
		d_height = getInteger<RemoteModule>("Height");
		DLOG(INFO) << "LineSelector: IOUT11";
		setString<InterfaceModule>("LineSelector","IOUT11");
		DLOG(INFO) << "LineInverter: True";
		setString<InterfaceModule>("LineInverter","True");
		DLOG(INFO) << "LineSource: Device0Strobe";
		setString<InterfaceModule>("LineSource","Device0Strobe");

		DLOG(INFO) << "CameraControlMethod: RC";
		setString<DeviceModule>("CameraControlMethod","RC");

		//This is a big hack allowing to have the camera controlled by the
		//framegrabber. We set it to pulse mode and double the frequency.
		setString<DeviceModule>("ExposureReadoutOverlap","True");
		DLOG(INFO) << "AcquisitionFrameRate: " << cameraConfig.FPS;
		setInteger<DeviceModule>("CycleMinimumPeriod",1e6/(2*cameraConfig.FPS));
		setString<DeviceModule>("CxpLinkConfiguration","CXP6_X4");
		setString<DeviceModule>("CxpTriggerMessageFormat","Toggle");

		setInteger<DeviceModule>("ExposureTime",6000);
		DLOG(INFO) << "StrobeDuration: " << cameraConfig.StrobeDuration;
		setInteger<DeviceModule>("StrobeDuration",cameraConfig.StrobeDuration);

		DLOG(INFO) << "StrobeDelay: " << cameraConfig.StrobeDuration;
		setInteger<DeviceModule>("StrobeDelay",cameraConfig.StrobeDelay);

		setString<RemoteModule>("ExposureMode","Edge_Triggerred_Programmable");
	} else {
		if (cameraConfig.Width == 0 || cameraConfig.Height == 0 ) {
			throw std::runtime_error("Camera resolution is not specified in DF mode");
		}

		setInteger<RemoteModule>("Width",cameraConfig.Width);
		setInteger<RemoteModule>("Height",cameraConfig.Height);

	}


	DLOG(INFO) << "Enable Event";
	enableEvent<NewBufferData>();
	DLOG(INFO) << "Realloc Buffer";
	reallocBuffers(4);
}

void EuresysFrameGrabber::Start() {
	DLOG(INFO) << "Starting framegrabber";
	start();
}

void EuresysFrameGrabber::Stop() {
	DLOG(INFO) << "Stopping framegrabber";
	stop();
	DLOG(INFO) << "Framegrabber stopped";
}


EuresysFrameGrabber::~EuresysFrameGrabber() {
	DLOG(INFO) << "Cleaning FrameGrabber";
}

std::pair<int32_t,int32_t> EuresysFrameGrabber::GetResolution() {
	return std::make_pair(d_width,d_height);
}

Frame::Ptr EuresysFrameGrabber::NextFrame() {
	processEvent<Euresys::NewBufferData>(1000);
	Frame::Ptr res = d_frame;
	d_frame.reset();
	return res;
}

void EuresysFrameGrabber::onNewBufferEvent(const Euresys::NewBufferData &data) {
	std::unique_lock<std::mutex> lock(d_mutex);
	d_frame = std::make_shared<EuresysFrame>(*this,data,d_lastFrame,d_toAdd);
}

EuresysFrame::EuresysFrame(Euresys::EGrabber<Euresys::CallbackOnDemand> & grabber, const Euresys::NewBufferData & data, uint64_t & lastFrame, uint64_t & toAdd )
	: Euresys::ScopedBuffer(grabber,data)
	, d_width(getInfo<size_t>(GenTL::BUFFER_INFO_WIDTH))
	, d_height(getInfo<size_t>(GenTL::BUFFER_INFO_HEIGHT))
	, d_timestamp(getInfo<uint64_t>(GenTL::BUFFER_INFO_TIMESTAMP))
	, d_ID(getInfo<uint64_t>(GenTL::BUFFER_INFO_FRAMEID))
	, d_mat(d_height,d_width,CV_8U,getInfo<void*>(GenTL::BUFFER_INFO_BASE)) {
	if ( d_ID == 0 && lastFrame != 0 ) {
		toAdd += lastFrame + 1;
	}
	lastFrame = d_ID;
	d_ID += toAdd;
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
