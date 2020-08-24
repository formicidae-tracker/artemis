#pragma once

#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/io_context.hpp>

#include <tbb/concurrent_queue.h>

#include <mutex>

#include "Task.hpp"

#include "Options.hpp"
namespace fort {
namespace artemis {

class VideoOutputTask : public Task {
public:
	VideoOutputTask(const VideoOutputOptions & options,
	                boost::asio::io_context & context);

	virtual ~VideoOutputTask();

	void Run() override;

	void QueueFrame(const std::shared_ptr<cv::Mat> & image,
	                const Time & frameTime,
	                const uint64_t frameID);

	void CloseQueue();



private :
	void OverlayTime(cv::Mat & frame,
	                 const Time & time);

	void OutputData(const std::shared_ptr<cv::Mat> & framePtr,
	                uint64_t frameID);

	void MarkDone();

	void OutputDataWithoutHeader(const std::shared_ptr<cv::Mat> & framePtr);

	typedef std::tuple<std::shared_ptr<cv::Mat>,
	                   Time,
	                   uint64_t> FrameData;
	typedef tbb::concurrent_bounded_queue<FrameData> InboundQueue;

	boost::asio::posix::stream_descriptor d_stream;
	InboundQueue                          d_queue;

	std::mutex d_mutex;
	bool       d_done;
	const bool d_addHeader;

	std::vector<uint64_t> d_headerData;
};

} // namespace artemis
} // namespace fort
