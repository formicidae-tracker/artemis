#pragma once

#include "Options.hpp"
#include <memory>

#include <mvIMPACT_CPP/mvIMPACT_acquire.h>
#include <slog++/Logger.hpp>

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

class HyperionFrameGrabber;

class HyperionFrame : public Frame {
public:
	HyperionFrame(
	    int                                          index,
	    const std::shared_ptr<HyperionFrameGrabber> &framegrabber,
	    details::Uint32TimestampFixer               &fixer
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
	ImageU8          ToImageU8() override;

private:
	std::weak_ptr<HyperionFrameGrabber> d_framegraber;
	int                                 d_index;

	void    *d_data;
	uint64_t d_timestamp, d_ID;
	int      d_width, d_height;
};

class HyperionFrameGrabber
    : public FrameGrabber,
      public std::enable_shared_from_this<HyperionFrameGrabber> {

private:
	HyperionFrameGrabber(int deviceIndex, const CameraOptions &options);

public:
	static std::shared_ptr<HyperionFrameGrabber>
	Create(int deviceIndex, const CameraOptions &options);
	virtual ~HyperionFrameGrabber();

	HyperionFrameGrabber(const HyperionFrameGrabber &)            = delete;
	HyperionFrameGrabber(HyperionFrameGrabber &&)                 = delete;
	HyperionFrameGrabber &operator=(const HyperionFrameGrabber &) = delete;
	HyperionFrameGrabber &operator=(HyperionFrameGrabber &&)      = delete;

	void       Start() override;
	void       Stop() override;
	void       AbordPending() override;
	Frame::Ptr NextFrame() override;

	Size Resolution() const override;

private:
	friend class HyperionFrame;

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

	slog::Logger<2> d_logger;

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
	std::atomic<size_t>           d_inprocessFrame{0};
};
} // namespace artemis
} // namespace fort
