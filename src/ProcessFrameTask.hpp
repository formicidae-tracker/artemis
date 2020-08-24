#pragma once

#include <tbb/concurrent_queue.h>

#include <opencv2/core.hpp>

#include <fort/hermes/FrameReadout.pb.h>

#include "Task.hpp"
#include "Options.hpp"
#include "FrameGrabber.hpp"
#include "ObjectPool.hpp"

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
	                 const VideoOutputTaskPtr & videoOuput,
	                 const UserInterfaceTaskPtr & userInterface,
	                 const ConnectionPtr & connection,
	                 const FullFrameExportTaskPtr & fullFrameExporter,
	                 const cv::Size & inputResolution);

	virtual ~ProcessFrameTask();

	void Run() override;

	void QueueFrame( const Frame::Ptr & );
	void CloseFrameQueue();
private :
	typedef tbb::concurrent_bounded_queue<Frame::Ptr> FrameQueue;



	void ProcessFrameMandatory(const Frame::Ptr & frame );
	void ProcessFrame(const Frame::Ptr & frame);
	void DropFrame(const Frame::Ptr & frame);


	void Detect(const Frame::Ptr & frame,
	            hermes::FrameReadout & m);

	cv::Rect GetROIAt(int x, int y,
	                  const cv::Size & bound);

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

	size_t DownscaledImagePerCycle() const;

	std::shared_ptr<hermes::FrameReadout> PrepareMessage(const Frame::Ptr & frame);

	bool ShouldProcess(uint64_t ID);

	const Options          d_options;

	FrameQueue             d_frameQueue;
	VideoOutputTaskPtr     d_videoOutput;
	UserInterfaceTaskPtr   d_userInterface;

	ConnectionPtr          d_connection;

	FullFrameExportTaskPtr d_fullFrameExport;


	ObjectPool<cv::Mat>               d_framePool;
	ObjectPool<hermes::FrameReadout>  d_messagePool;
	std::shared_ptr<cv::Mat>          d_downscaled;
	const size_t                      d_maximumThreads;
	size_t                            d_actualThreads;

	ApriltagDetectorPtr               d_detector;

	Time                              d_nextFrameExport;
	Time                              d_nextAntCatalog;
	std::set<uint32_t>                d_exportedID;
};

} // namespace artemis
} // namespace fort
