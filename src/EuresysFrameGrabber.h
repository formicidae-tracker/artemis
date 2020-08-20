#pragma once

#include "FrameGrabber.h"
#include "Options.hpp"

#include <EGrabber.h>

#include <opencv2/core/core.hpp>
#include <mutex>

namespace fort {
namespace artemis {

class EuresysFrame : public Frame {
public :
	EuresysFrame(Euresys::EGrabber<Euresys::CallbackOnDemand> & grabber,
	             const Euresys::NewBufferData &,
	             uint64_t & lastFrame,
	             uint64_t & toAdd);
	virtual ~EuresysFrame();

	virtual void * Data();
	virtual size_t Width() const;
	virtual size_t Height() const;
	virtual uint64_t Timestamp() const;
	virtual uint64_t ID() const;
	const cv::Mat & ToCV();

private :
	Euresys::ScopedBuffer d_euresysBuffer;
	size_t                d_width,d_height;
	uint64_t              d_timestamp,d_ID;
	cv::Mat               d_mat;
};


class EuresysFrameGrabber : public FrameGrabber {
public :
	typedef std::shared_ptr<Euresys::ScopedBuffer> BufferPtr;

	EuresysFrameGrabber(const CameraOptions & cameraConfig);

	virtual ~EuresysFrameGrabber();


	virtual void Start();
	virtual void Stop();
	virtual Frame::Ptr NextFrame();

	virtual std::pair<int32_t,int32_t> GetResolution();
private:

	virtual void onNewBufferEvent(const Euresys::NewBufferData &data);

	Euresys::EGenTL                              d_egentl;
	Euresys::EGrabber<Euresys::CallbackOnDemand> d_grabber;

	std::mutex         d_mutex;
	Frame::Ptr         d_frame;
	uint64_t           d_lastFrame;
	uint64_t           d_toAdd;
	int32_t            d_width,d_height;
};

} // namespace artemis
} // namespace fort
