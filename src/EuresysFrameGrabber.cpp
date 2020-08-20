#include "EuresysFrameGrabber.hpp"

#include <glog/logging.h>
#include <regex>

namespace fort {
namespace artemis {

EuresysFrameGrabber::EuresysFrameGrabber(const CameraOptions & options)
	: d_grabber(d_egentl)
	, d_lastFrame(0)
	, d_toAdd(0)
	, d_width(0)
	, d_height(0) {

	using namespace Euresys;

	std::string ifID = d_grabber.getString<InterfaceModule>("InterfaceID");
	std::regex slaveRx("df-camera");
	bool isMaster = !std::regex_search(ifID,slaveRx) ;

	if ( isMaster == true ) {
		d_width = d_grabber.getInteger<RemoteModule>("Width");
		d_height = d_grabber.getInteger<RemoteModule>("Height");
		DLOG(INFO) << "LineSelector: IOUT11";
		d_grabber.setString<InterfaceModule>("LineSelector","IOUT11");
		DLOG(INFO) << "LineInverter: True";
		d_grabber.setString<InterfaceModule>("LineInverter","True");
		DLOG(INFO) << "LineSource: Device0Strobe";
		d_grabber.setString<InterfaceModule>("LineSource","Device0Strobe");

		DLOG(INFO) << "CameraControlMethod: RC";
		d_grabber.setString<DeviceModule>("CameraControlMethod","RC");

		//This is a big hack allowing to have the camera controlled by the
		//framegrabber. We set it to pulse mode and double the frequency.
		d_grabber.setString<DeviceModule>("ExposureReadoutOverlap","True");
		DLOG(INFO) << "AcquisitionFrameRate: " << options.FPS;
		d_grabber.setInteger<DeviceModule>("CycleMinimumPeriod",1e6/(2*options.FPS));
		d_grabber.setString<DeviceModule>("CxpLinkConfiguration","CXP6_X4");
		d_grabber.setString<DeviceModule>("CxpTriggerMessageFormat","Toggle");

		d_grabber.setInteger<DeviceModule>("ExposureTime",6000);
		DLOG(INFO) << "StrobeDuration: " << options.StrobeDuration;
		d_grabber.setInteger<DeviceModule>("StrobeDuration",options.StrobeDuration.Microseconds());

		DLOG(INFO) << "StrobeDelay: " << options.StrobeDelay;
		d_grabber.setInteger<DeviceModule>("StrobeDelay",options.StrobeDelay.Microseconds());

		d_grabber.setString<RemoteModule>("ExposureMode","Edge_Triggerred_Programmable");
	} else {
		if (options.SlaveWidth == 0 || options.SlaveHeight == 0 ) {
			throw std::runtime_error("Camera resolution is not specified in DF mode");
		}

		d_grabber.setInteger<RemoteModule>("Width",options.SlaveWidth);
		d_grabber.setInteger<RemoteModule>("SlaveHeight",options.SlaveHeight);

	}


	DLOG(INFO) << "Enable Event";
	d_grabber.enableEvent<NewBufferData>();
	DLOG(INFO) << "Realloc Buffer";
	d_grabber.reallocBuffers(4);
}

void EuresysFrameGrabber::Start() {
	DLOG(INFO) << "Starting framegrabber";
	d_grabber.start();
}

void EuresysFrameGrabber::Stop() {
	DLOG(INFO) << "Stopping framegrabber";
	d_grabber.stop();
	DLOG(INFO) << "Framegrabber stopped";
}


EuresysFrameGrabber::~EuresysFrameGrabber() {
	DLOG(INFO) << "Cleaning FrameGrabber";
}

std::pair<int32_t,int32_t> EuresysFrameGrabber::GetResolution() {
	return std::make_pair(d_width,d_height);
}

Frame::Ptr EuresysFrameGrabber::NextFrame() {
	d_grabber.processEvent<Euresys::NewBufferData>(1000);
	//why ???
	Frame::Ptr res = d_frame;
	d_frame.reset();
	return res;
}

void EuresysFrameGrabber::onNewBufferEvent(const Euresys::NewBufferData &data) {
	std::unique_lock<std::mutex> lock(d_mutex);
	d_frame = std::make_shared<EuresysFrame>(d_grabber,data,d_lastFrame,d_toAdd);
}

EuresysFrame::EuresysFrame(Euresys::EGrabber<Euresys::CallbackOnDemand> & grabber,
                           const Euresys::NewBufferData & data,
                           uint64_t & lastFrame,
                           uint64_t & toAdd )
	: d_euresysBuffer(grabber,data)
	, d_width(d_euresysBuffer.getInfo<size_t>(GenTL::BUFFER_INFO_WIDTH))
	, d_height(d_euresysBuffer.getInfo<size_t>(GenTL::BUFFER_INFO_HEIGHT))
	, d_timestamp(d_euresysBuffer.getInfo<uint64_t>(GenTL::BUFFER_INFO_TIMESTAMP))
	, d_ID(d_euresysBuffer.getInfo<uint64_t>(GenTL::BUFFER_INFO_FRAMEID))
	, d_mat(d_height,d_width,CV_8U,d_euresysBuffer.getInfo<void*>(GenTL::BUFFER_INFO_BASE)) {
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
	return d_euresysBuffer.getInfo<void*>(GenTL::BUFFER_INFO_BASE);
}

} // namespace artemis
} // namespace fort
