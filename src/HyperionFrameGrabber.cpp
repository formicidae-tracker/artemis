#include <endian.h>
#include <linux/can.h>
#include <linux/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <memory>
#include <stdexcept>

#include "HyperionFrameGrabber.hpp"
#include "utils/Defer.hpp"
#include "utils/PosixCall.hpp"
#include <mvIMPACT_CPP/mvIMPACT_acquire.h>

#include <glog/logging.h>

namespace fort {

namespace artemis {

constexpr static size_t WIDTH  = 6576;
constexpr static size_t HEIGHT = 4384;

namespace acq = mvIMPACT::acquire;

static acq::DeviceManager deviceManager;

HyperionFrameGrabber::HyperionFrameGrabber(
    int index, const CameraOptions &options
)
    : d_device{deviceManager.getDevice(index)}
    , d_acquisitionTimeout(1500 / options.FPS) {

	if (options.FPS > 5.0) {
		throw std::out_of_range(
		    "Camera FPS (" + std::to_string(options.FPS) + ") should be <= 5.0"
		);
	}

	d_socket = openCANSocket();

	d_pulseLength = options.StrobeDuration;
	d_pulsePeriod = double(fort::Duration::Second.Nanoseconds()) / options.FPS;
	sendHeliosTriggerMode(d_pulsePeriod, d_pulseLength, d_socket, 1);
	d_lastSend = Time::Now();

	// if (d_device->acquisitionStartStopBehaviour.read() != acq::assbUser) {
	// 	LOG(INFO) << "[mvHYPERION] user acquisition start/stop";
	// 	d_device->acquisitionStartStopBehaviour.write(acq::assbUser);
	// }

	d_device->open();

	acq::CameraDescriptionManager cdm(d_device);

	d_description = cdm.cameraDescriptionCameraLink(0);
	d_description->aoiWidth.write(WIDTH);
	d_description->aoiHeight.write(HEIGHT);
	d_description->bitsPerPixel.write(8);

	d_description->pixelsPerCycle.write(2);
	d_description->tapsYGeometry.writeS("2YE");

	d_stats = std::make_unique<acq::Statistics>(d_device);
	d_intf  = std::make_unique<acq::FunctionInterface>(d_device);

	;
	for (auto err = acq::DMR_NO_ERROR; err == acq::DMR_NO_ERROR;
	     err = static_cast<acq::TDMR_ERROR>(d_intf->imageRequestSingle())) {
		++d_requestCount;
	}
}

HyperionFrameGrabber::~HyperionFrameGrabber() {
	try {
		sendHeliosTriggerMode(0, 0, d_socket, 1);
	} catch (const std::exception &e) {
		LOG(ERROR) << "Could not reset helios trigger mode: " << e.what();
	}
}

void HyperionFrameGrabber::Start() {
	d_intf->acquisitionStart();
	d_fixer = details::Uint32TimestampFixer{};
}

void HyperionFrameGrabber::Stop() {
	d_intf->acquisitionStop();
}

void HyperionFrameGrabber::AbordPending() {
	d_stop.store(true);
}

Frame::Ptr HyperionFrameGrabber::NextFrame() {
	while (true) {
		auto now = Time::Now();
		if (Time::Now().Sub(d_lastSend) >= 5 * Duration::Second) {
			d_lastSend = now;
			try {
				sendHeliosTriggerMode(
				    d_pulsePeriod,
				    d_pulseLength,
				    d_socket,
				    1
				);
			} catch (const std::exception &e) {
				LOG(ERROR) << "could not resend helios pulse mode: "
				           << e.what();
			}
		}

		auto idx = d_intf->imageRequestWaitFor(d_acquisitionTimeout);

		if (d_stop.load() == true) {
			return Frame::Ptr{};
		}

		if (d_intf->isRequestNrValid(idx) == false) {
			LOG(ERROR) << "invalid request :"
			           << acq::ImpactAcquireException::getErrorCodeAsString(idx
			              );
			continue;
		}
		try {
			auto res = std::make_shared<HyperionFrame>(idx, *d_intf, d_fixer);
			return res;
		} catch (const std::runtime_error &e) {
			LOG(ERROR) << "Could not create frame: " << e.what();
			continue;
		}
	}
}

cv::Size HyperionFrameGrabber::Resolution() const {
	return {WIDTH, HEIGHT};
}

HyperionFrame::HyperionFrame(
    int                                   idx,
    mvIMPACT::acquire::FunctionInterface &interface,
    details::Uint32TimestampFixer        &fixer
)
    : d_interface{interface}
    , d_index{idx}
    , d_request{d_interface.getRequest(d_index)} {

	if (d_request->isOK() == false) {
		d_interface.imageRequestUnlock(d_index);
		throw std::runtime_error{"invalid request"};
	}

	for (auto it = d_request->getInfoIterator(); it.isValid(); ++it) {
		if (it.isProp() == false) {
			continue;
		}
		acq::Property prop(it);
		if (prop.name() == "FrameID") {
			d_ID = std::atoi(prop.readS().c_str());
		} else if (prop.name() == "TimeStamp_us") {
			d_timestamp = fixer.FixTimestamp(prop.readS());
		}
	}

	auto buf = d_request->getImageBufferDesc().getBuffer();
	d_data   = buf->vpData;
	d_width  = buf->iWidth;
	d_height = buf->iHeight;

	d_mat = cv::Mat(d_height, d_width, CV_8U, d_data);
}

HyperionFrame::~HyperionFrame() {
	d_request->unlock();
	d_interface.imageRequestSingle();
	// d_interface.imageRequestUnlock(d_index);
}

void *HyperionFrame::Data() {
	return d_data;
}

size_t HyperionFrame::Width() const {
	return d_width;
}

size_t HyperionFrame::Height() const {
	return d_height;
}

uint64_t HyperionFrame::Timestamp() const {
	return d_timestamp;
};

uint64_t HyperionFrame::ID() const {
	return d_ID;
};

const cv::Mat &HyperionFrame::ToCV() {
	return d_mat;
};

void bindSocketToIfName(int s, const std::string &ifname) {
	struct ifreq ifr;
	strcpy(ifr.ifr_name, ifname.c_str());
	if (ioctl(s, SIOCGIFINDEX, &ifr) != 0) {
		throw std::system_error(
		    errno,
		    ARTEMIS_SYSTEM_CATEGORY(),
		    "Could not find interface '" + ifname + "'"
		);
	}
	struct sockaddr_can addr;
	addr.can_family  = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;
	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		throw std::system_error(
		    errno,
		    ARTEMIS_SYSTEM_CATEGORY(),
		    "Could not bind to interface '" + ifname + "'"
		);
	}
}

int HyperionFrameGrabber::openCANSocket(const CanConfig &config) {
	auto s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
	if (s < 0) {
		throw std::system_error(
		    errno,
		    ARTEMIS_SYSTEM_CATEGORY(),
		    "could not open socket"
		);
	}
	bindSocketToIfName(s, config.IfName);
	return s;
}

void HyperionFrameGrabber::sendHeliosTriggerMode(
    fort::Duration period, fort::Duration length, int socket, uint8_t nodeID
) {

	struct can_frame frame;

	struct ArkeHeliosTriggerConfig {
		uint16_t Period_hecto_us;
		uint16_t Pulse_us;
		int16_t  CameraDelay_us;
	} __attribute__((packed));

	ArkeHeliosTriggerConfig trigger = {
	    .Period_hecto_us = htole16(period.Microseconds() / 100),
	    .Pulse_us        = htole16(length.Microseconds()),
	    .CameraDelay_us  = 0,
	};

	frame.can_id  = (0b10 << 9) | (0x36 << 3) | (nodeID & 0x07);
	frame.can_dlc = sizeof(ArkeHeliosTriggerConfig);
	memcpy(frame.data, &trigger, sizeof(ArkeHeliosTriggerConfig));
	if (write(socket, &frame, sizeof(struct can_frame)) < 0) {
		throw std::system_error(
		    errno,
		    ARTEMIS_SYSTEM_CATEGORY(),
		    "could not send helios trigger can frame"
		);
	}
}

namespace details {

uint64_t Uint32TimestampFixer::FixTimestamp(const std::string &value) {
	auto newTimestamp = uint32_t(std::atoi(value.c_str()));

	if (this->Last == nullptr) {
		this->Last      = std::make_unique<uint32_t>();
		*this->Last     = newTimestamp;
		this->Timestamp = newTimestamp;
		return this->Timestamp;
	}

	uint32_t udiff = newTimestamp - *this->Last;
	this->Timestamp += uint64_t(udiff);
	*this->Last = newTimestamp;
	return this->Timestamp;
}
} // namespace details

} // namespace artemis
} // namespace fort
