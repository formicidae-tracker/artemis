#pragma once

#include "Options.hpp"
#include <memory>

#include <mvIMPACT_CPP/mvIMPACT_acquire.h>

#include "FrameGrabber.hpp"

namespace fort {
namespace artemis {

class HyperionFrame : public Frame {
public:
	HyperionFrame(int index, mvIMPACT::acquire::FunctionInterface &interface);
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
	virtual ~HyperionFrameGrabber() = default;

	void       Start() override;
	void       Stop() override;
	Frame::Ptr NextFrame() override;

	cv::Size Resolution() const override;

private:
	std::unique_ptr<mvIMPACT::acquire::Device>      d_device;
	mvIMPACT::acquire::CameraDescriptionCameraLink *d_description;

	std::unique_ptr<mvIMPACT::acquire::Statistics>        d_stats;
	std::unique_ptr<mvIMPACT::acquire::FunctionInterface> d_intf;

	size_t d_requestCount = 0;
};
} // namespace artemis
} // namespace fort
