#pragma once

#include <glib.h>

#include <cstdint>
#include <cstdlib>
#include <string>

#include <taskflow/core/executor.hpp>

#include <slog++/Attribute.hpp>
#include <slog++/Logger.hpp>

#include <fort/hermes/FrameReadout.pb.h>
#include <fort/utils/ObjectPool.hpp>

#include "FrameGrabber.hpp"
#include "ImageU8.hpp"
#include "Options.hpp"
#include "Task.hpp"

#include "readerwriterqueue.h"

namespace cv {
class Mat;
}

namespace fort {
namespace artemis {

class VideoOutputTask;
typedef std::unique_ptr<VideoOutputTask> VideoOutputTaskPtr;
class UserInterfaceTask;
typedef std::shared_ptr<UserInterfaceTask> UserInterfaceTaskPtr;
class Connection;
typedef std::unique_ptr<Connection> ConnectionPtr;
class ApriltagDetector;
typedef std::unique_ptr<ApriltagDetector> ApriltagDetectorPtr;
class VideoOutput;
typedef std::unique_ptr<VideoOutput> VideoOutputPtr;

class ProcessFrameTask : public Task {
public:
	ProcessFrameTask(
	    const Options &options,
	    GMainContext  *context,
	    const Size    &inputResolution
	);

	virtual ~ProcessFrameTask();

	void Run() override;

	void QueueFrame(const Frame::Ptr &);
	void CloseFrameQueue();

	UserInterfaceTaskPtr UserInterfaceTask() const;

private:
	typedef moodycamel::BlockingReaderWriterQueue<Frame::Ptr> FrameQueue;

	void SetUpVideoOutputTask(
	    const VideoOutputOptions &options,
	    const Size               &inputResolution,
	    float                     FPS
	);

	void
	SetUpDetection(const Size &inputResolution, const ApriltagOptions &options);

	void SetUpCataloguing(const Options &options);

	void SetUpUserInterface(
	    const Size    &workingresolution,
	    const Size    &fullresolution,
	    const Options &options
	);

	void SetUpConnection(const LetoOptions &options, GMainContext *context);

	void SetUpTaskflow();

	void ProcessFrame(const Frame::Ptr &frame);

	void DropFrame(const Frame::Ptr &frame);

	void Detect(const Frame::Ptr &frame, hermes::FrameReadout &m);

	void ResetExportedID(const Time &time);

	std::vector<std::tuple<uint32_t, double, double>>
	FindUnexportedID(const hermes::FrameReadout &m);

	void ExportROI(
	    const ImageU8 &image,
	    uint64_t       frameID,
	    uint32_t       tagID,
	    double         x,
	    double         y
	);

	void CatalogTag(
	    Frame::Ptr                            frame,
	    std::shared_ptr<hermes::FrameReadout> m,
	    tf::Runtime                          &rt
	);

	// this method copies its pointer to avoid an early invalidation of frame
	void ExportFullFrame(Frame::Ptr frame);

	void DisplayFrame(
	    const Frame::Ptr &frame, const std::shared_ptr<hermes::FrameReadout> &m
	);

	void TearDown();

	void PrepareMessage(const Frame::Ptr &frame, hermes::FrameReadout &readout);

	bool ShouldProcess(uint64_t ID);

	double CurrentFPS(const Time &time);

	// const ProcessOptions d_options;
	struct Config {

		std::string           UUID;
		size_t                FrameStride;
		std::set<uint64_t>    FrameIDs;
		Duration              ImageRenewPeriod;
		std::filesystem::path CloseUpDir;
		size_t                CloseUpSize;

		Config(const Options &options)
		    : UUID{options.Process.UUID}
		    , FrameStride{options.Process.FrameStride}
		    , FrameIDs{options.Process.FrameIDs()}
		    , ImageRenewPeriod{options.RenewPeriod}
		    , CloseUpDir{options.CloseUpOutputDir}
		    , CloseUpSize{options.CloseUpROISize} {}
	};

	using ImagePool = utils::ObjectPool<
	    ImageU8,
	    std::function<ImageU8 *()>,
	    ImageU8::OwnedMemoryDeleter>;
	using MessagePool = utils::ObjectPool<hermes::FrameReadout>;

	struct ProcessedData {
		artemis::Frame::Ptr                   Frame;
		std::shared_ptr<hermes::FrameReadout> Readout;
		std::shared_ptr<ImageU8>              Full, Zoomed;
	};

	Config d_config;

	FrameQueue d_frameQueue;

	UserInterfaceTaskPtr d_userInterface;

	ConnectionPtr d_connection;

	VideoOutputPtr d_video;

	MessagePool::Ptr d_messagePool = MessagePool::Create();
	ImagePool::Ptr   d_imagePool =
	    ImagePool::Create([this]() -> ImageU8 * {
		    size_t wanted = d_workingResolution.width() *
		                    d_workingResolution.height() * sizeof(uint8_t);
		    wanted        = (wanted + 63) & ~63;

		    auto buffer = static_cast<uint8_t *>(aligned_alloc(64, wanted));
		    auto stats  = d_imagePool->GetStats();
		    slog::DDebug(
		        "allocating ImageU8 for UserInterface",
		        slog::Int("size", wanted),
		        slog::Pointer("buffer", buffer),
		        slog::Group(
		            "pool",
		            slog::Int("allocated", stats.Allocated),
		            slog::Int("available", stats.Available)
		        )
		    );
		    return new ImageU8{
		        d_workingResolution.width(),
		        d_workingResolution.height(),
		        buffer,
		        d_workingResolution.width()
		    };
	    });

	const size_t        d_maximumThreads;
	std::atomic<size_t> d_actualThreads;

	ApriltagDetectorPtr d_detector;

	Time               d_nextFrameExport;
	Time               d_nextTagCatalog;
	std::set<uint32_t> d_exportedID;

	Size                d_workingResolution;
	Rect                d_wantedROI;
	std::atomic<size_t> d_frameDropped = 0, d_frameProcessed = 0;
	Time                d_start;

	tf::Executor    d_executor;
	slog::Logger<1> d_logger;
	ProcessedData   d_current;
	tf::Taskflow    d_taskflow;
};

} // namespace artemis
} // namespace fort
