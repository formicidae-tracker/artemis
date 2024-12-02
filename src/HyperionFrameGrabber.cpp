#include <memory>
#include <stdexcept>

#include "HyperionFrameGrabber.hpp"
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
    : d_device{deviceManager.getDevice(index)} {

	d_device->open();

	acq::CameraDescriptionManager cdm(d_device);

	d_description = cdm.cameraDescriptionCameraLink(0);
	d_description->aoiWidth.write(WIDTH);
	d_description->aoiHeight.write(HEIGHT);
	std::vector<std::string> values;
	d_description->bitsPerPixel.getTranslationDictStrings(values);
	LOG(INFO) << "got possible values: " << values.size();
	for (const auto &v : values) {
		LOG(INFO) << v;
	}
	d_description->bitsPerPixel.write(2);
	d_description->tapsYGeometry.writeS("2YE");

	d_stats = std::make_unique<acq::Statistics>(d_device);
	d_intf  = std::make_unique<acq::FunctionInterface>(d_device);

	acq::TDMR_ERROR err = acq::DMR_NO_ERROR;
	for (; err == acq::DMR_NO_ERROR;
	     err = static_cast<acq::TDMR_ERROR>(d_intf->imageRequestSingle())) {
		++d_requestCount;
	}
}

void HyperionFrameGrabber::Start() {
	d_intf->acquisitionStart();
}

void HyperionFrameGrabber::Stop() {
	d_intf->acquisitionStop();
}

Frame::Ptr HyperionFrameGrabber::NextFrame() {

	while (true) {
		auto idx = d_intf->imageRequestWaitFor(-1);

		if (d_intf->isRequestNrValid(idx) == false) {
			throw std::logic_error{
			    "invalid request :" +
			    acq::ImpactAcquireException::getErrorCodeAsString(idx)};
		}
		try {
			return std::make_shared<HyperionFrame>(idx, *d_intf);
		} catch (const std::runtime_error &e) {
			continue;
		}
	}
}

cv::Size HyperionFrameGrabber::Resolution() const {
	return {WIDTH, HEIGHT};
}

HyperionFrame::
    HyperionFrame(int idx, mvIMPACT::acquire::FunctionInterface &interface)
    : d_interface{interface}
    , d_index{idx}
    , d_request{d_interface.getRequest(d_index)} {

	if (d_request->isOK() == false) {
		d_interface.imageRequestUnlock(d_index);
		throw std::runtime_error{"invalid request"};
	}

	for (auto it = d_request->getInfoIterator(); it.isValid(); ++it) {
		std::cerr << it.name() << "-" << it.docString() << std::endl;
	}

	auto buf = d_request->getImageBufferDesc().getBuffer();
	d_data   = buf->vpData;
	d_width  = buf->iWidth;
	d_height = buf->iHeight;

	d_mat = cv::Mat(d_height, d_width, CV_8U, d_data);
}

HyperionFrame::~HyperionFrame() {
	d_interface.imageRequestUnlock(d_index);
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

} // namespace artemis
} // namespace fort
