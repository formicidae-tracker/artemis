#pragma once

#include <tbb/concurrent_queue.h>

#include <opencv2/core.hpp>

#include <fort/hermes/FrameReadout.pb.h>

#include <boost/asio/io_context.hpp>

#include "Task.hpp"
#include "Options.hpp"
#include "FrameGrabber.hpp"
#include "ObjectPool.hpp"

#include "ui/UserInterface.hpp"

namespace cv {
class Mat;
}


namespace fort {
namespace artemis {

class VideoOutputTask;
typedef std::shared_ptr<VideoOutputTask>     VideoOutputTaskPtr;
class UserInterfaceTask;
typedef std::shared_ptr<UserInterfaceTask>   UserInterfaceTaskPtr;
class Connection;
typedef std::shared_ptr<Connection>          ConnectionPtr;
class FullFrameExportTask;
typedef std::shared_ptr<FullFrameExportTask> FullFrameExportTaskPtr;
class ApriltagDetector;
typedef std::shared_ptr<ApriltagDetector>    ApriltagDetectorPtr;

class ProcessFrameTask : public Task{
public:
	ProcessFrameTask(const Options & options,
	                 boost::asio::io_context & context,
	                 const cv::Size & inputResolution);

	virtual ~ProcessFrameTask();

	void Run() override;

	void QueueFrame( const Frame::Ptr & );
	void CloseFrameQueue();

	VideoOutputTaskPtr     VideoOutputTask() const;
	UserInterfaceTaskPtr   UserInterfaceTask() const;
	FullFrameExportTaskPtr FullFrameExportTask() const;


private :
	typedef tbb::concurrent_bounded_queue<Frame::Ptr> FrameQueue;

	void SetUpVideoOutputTask(const VideoOutputOptions & options,
	                          boost::asio::io_context & context,
	                          bool legacyMode);
	void SetUpDetection(const cv::Size & inputResolution,
	                    const ApriltagOptions & options);
	void SetUpCataloguing(const ProcessOptions & options);
	void SetUpUserInterface(const cv::Size & workingresolution,
	                        const cv::Size & fullresolution,
	                        const Options & options);
	void SetUpPoolObjects();

	void SetUpConnection(const NetworkOptions & options, boost::asio::io_context & context);


	void ProcessFrameMandatory(const Frame::Ptr & frame );
	void ProcessFrame(const Frame::Ptr & frame);
	void DropFrame(const Frame::Ptr & frame);


	void Detect(const Frame::Ptr & frame,
	            hermes::FrameReadout & m);

	void ResetExportedID(const Time & time);

	std::vector<std::tuple<uint32_t,double,double>> FindUnexportedID(const hermes::FrameReadout & m);

	void ExportROI(const cv::Mat & image,
	               uint64_t frameID,
	               uint32_t tagID,
	               double x,
	               double y);

	void CatalogAnt(const Frame::Ptr & frame,
	                const hermes::FrameReadout & m);

	void ExportFullFrame(const Frame::Ptr & frame);

	void DisplayFrame(const Frame::Ptr frame,
	                  const std::shared_ptr<hermes::FrameReadout> & m);

	void TearDown();

	size_t GrayscaleImagePerCycle() const;
	size_t RGBImagePerCycle() const;

	std::shared_ptr<hermes::FrameReadout> PrepareMessage(const Frame::Ptr & frame);

	bool ShouldProcess(uint64_t ID);

	double CurrentFPS(const Time & time);

	const ProcessOptions   d_options;

	FrameQueue             d_frameQueue;
	VideoOutputTaskPtr     d_videoOutput;
	UserInterfaceTaskPtr   d_userInterface;

	ConnectionPtr          d_connection;

	FullFrameExportTaskPtr d_fullFrameExport;


	ObjectPool<cv::Mat>               d_grayImagePool;
	ObjectPool<cv::Mat>               d_rgbImagePool;

	ObjectPool<hermes::FrameReadout>  d_messagePool;
	std::shared_ptr<cv::Mat>          d_downscaled;
	const size_t                      d_maximumThreads;
	size_t                            d_actualThreads;

	ApriltagDetectorPtr               d_detector;

	Time                              d_nextFrameExport;
	Time                              d_nextAntCatalog;
	std::set<uint32_t>                d_exportedID;

	cv::Size            d_workingResolution;
	cv::Rect            d_wantedROI;
	size_t              d_frameDropped;
	size_t              d_frameProcessed;
	Time                d_start;
};

} // namespace artemis
} // namespace fort
