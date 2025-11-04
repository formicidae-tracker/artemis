#pragma once

#include <slog++/Logger.hpp>

#include <fort/hermes/FrameReadout.pb.h>

#include <boost/asio/io_context.hpp>

#include "FrameGrabber.hpp"
#include "ImageU8.hpp"
#include "Options.hpp"
#include "Task.hpp"

#include "fort/utils/ObjectPool.hpp"
#include "readerwriterqueue.h"
#include "taskflow/core/executor.hpp"

namespace cv {
class Mat;
}

namespace fort {
namespace artemis {

class VideoOutputTask;
typedef std::shared_ptr<VideoOutputTask> VideoOutputTaskPtr;
class UserInterfaceTask;
typedef std::shared_ptr<UserInterfaceTask> UserInterfaceTaskPtr;
class Connection;
typedef std::shared_ptr<Connection> ConnectionPtr;
class FullFrameExportTask;
typedef std::shared_ptr<FullFrameExportTask> FullFrameExportTaskPtr;
class ApriltagDetector;
typedef std::shared_ptr<ApriltagDetector> ApriltagDetectorPtr;

class ProcessFrameTask : public Task {
public:
	ProcessFrameTask(
	    const Options           &options,
	    boost::asio::io_context &context,
	    const Size              &inputResolution
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
	    boost::asio::io_context  &context,
	    bool                      legacyMode
	);

	void
	SetUpDetection(const Size &inputResolution, const ApriltagOptions &options);

	void SetUpCataloguing(const Options &options);

	void SetUpUserInterface(
	    const Size    &workingresolution,
	    const Size    &fullresolution,
	    const Options &options
	);

	void SetUpConnection(
	    const LetoOptions &options, boost::asio::io_context &context
	);

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

	void CatalogAnt(
	    Frame::Ptr                            frame,
	    std::shared_ptr<hermes::FrameReadout> m,
	    tf::Runtime                          &rt
	);

	void ExportFullFrame(const Frame::Ptr &frame);

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
		std::filesystem::path NewAntOutputDir;
		size_t                NewAntROISize;

		Config(const Options &options)
		    : UUID{options.Process.UUID}
		    , FrameStride{options.Process.FrameStride}
		    , FrameIDs{options.Process.FrameIDs()}
		    , ImageRenewPeriod{options.ImageRenewPeriod}
		    , NewAntOutputDir{options.NewAntOutputDir}
		    , NewAntROISize{options.NewAntROISize} {}
	};

	using ImagePool =
	    utils::ObjectPool<ImageU8, std::function<ImageU8 *(int, int)>>;
	using MessagePool = utils::ObjectPool<hermes::FrameReadout>;

	struct ProcessedData {
		artemis::Frame::Ptr                   Frame;
		ImageU8                               AsImage;
		std::shared_ptr<hermes::FrameReadout> Readout;
		ImagePool::ObjectPtr                  Full, Zoomed;
	};

	Config d_config;

	FrameQueue d_frameQueue;

	UserInterfaceTaskPtr d_userInterface;

	ConnectionPtr d_connection;

	MessagePool::Ptr d_messagePool = MessagePool::Create();
	ImagePool::Ptr   d_imagePool =
	    ImagePool::Create([](int w, int h) -> ImageU8 * {
		    return new ImageU8{w, h, nullptr, w};
	    });

	const size_t        d_maximumThreads;
	std::atomic<size_t> d_actualThreads;

	ApriltagDetectorPtr d_detector;

	Time               d_nextFrameExport;
	Time               d_nextAntCatalog;
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
