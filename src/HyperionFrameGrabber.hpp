#pragma once

#include "Options.hpp"
#include <memory>

#include <mvIMPACT_CPP/mvIMPACT_acquire.h>

#include "FrameGrabber.hpp"

namespace fort {
namespace artemis {

namespace details {
struct Uint32TimestampFixer {
	std::unique_ptr<uint32_t> Last;

	uint64_t Timestamp = 0;

	uint64_t FixTimestamp(const std::string &value);
};

} // namespace details

class HyperionFrame : public Frame {
public:
	HyperionFrame(
	    int                                   index,
	    mvIMPACT::acquire::FunctionInterface &interface,
	    details::Uint32TimestampFixer        &timestamp
	);
	virtual ~HyperionFrame();

	HyperionFrame(const HyperionFrame &other)            = delete;
	HyperionFrame(HyperionFrame &&other)                 = delete;
	HyperionFrame &operator=(const HyperionFrame &other) = delete;
	HyperionFrame &operator=(HyperionFrame &&other)      = delete;

	virtual void    *Data() override;
	virtual size_t   Width() const override;
	virtual size_t   Height() const override;
	virtual uint64_t Timestamp() const override;
	virtual uint64_t ID() const override;
	const cv::Mat   &ToCV() override;

private:
	mvIMPACT::acquire::FunctionInterface &d_interface;
	int                                   d_index;
	mvIMPACT::acquire::Request           *d_request;

	void    *d_data;
	uint64_t d_timestamp, d_ID;
	cv::Mat  d_mat;
	int      d_width, d_height;
};

class HyperionFrameGrabber : public FrameGrabber {

public:
	HyperionFrameGrabber(int deviceIndex, const CameraOptions &options);
	virtual ~HyperionFrameGrabber();

	HyperionFrameGrabber(const HyperionFrameGrabber &)            = delete;
	HyperionFrameGrabber(HyperionFrameGrabber &&)                 = delete;
	HyperionFrameGrabber &operator=(const HyperionFrameGrabber &) = delete;
	HyperionFrameGrabber &operator=(HyperionFrameGrabber &&)      = delete;

	void       Start() override;
	void       Stop() override;
	void       AbordPending() override;
	Frame::Ptr NextFrame() override;

	cv::Size Resolution() const override;

private:
	struct CanConfig {
		uint8_t     NodeID = 0;
		std::string IfName = "slcan0";
	};

	static void sendHeliosTriggerMode(
	    fort::Duration period, fort::Duration length, int socket, uint8_t nodeID
	);

	static int openCANSocket(
	    const CanConfig & = CanConfig{.NodeID = 0, .IfName = "slcan0"}
	);

	mvIMPACT::acquire::Device                      *d_device;
	mvIMPACT::acquire::CameraDescriptionCameraLink *d_description;

	std::unique_ptr<mvIMPACT::acquire::Statistics>        d_stats;
	std::unique_ptr<mvIMPACT::acquire::FunctionInterface> d_intf;

	std::atomic<bool> d_stop = false;

	int d_acquisitionTimeout;

	size_t d_requestCount = 0;

	int            d_socket;
	fort::Duration d_pulseLength;
	fort::Duration d_pulsePeriod;
	fort::Time     d_lastSend;

	details::Uint32TimestampFixer d_fixer;
};
} // namespace artemis
} // namespace fort
