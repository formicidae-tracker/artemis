#pragma once

#include <mutex>

#include "ProcessDefinition.h"

#include "hermes/FrameReadout.pb.h"

#include <asio/io_service.hpp>


class Frame;
typedef std::shared_ptr<Frame> FramePtr;

class ProcessQueueExecuter  {
public:
	ProcessQueueExecuter(asio::io_service & service, size_t maxWorkers);
	virtual ~ProcessQueueExecuter();

	void Start(ProcessQueue & queue, const Frame::Ptr & current);
	bool IsDone();

private:
	void SpawnCurrent();
	bool IsDoneUnsafe();


	asio::io_service &     d_service;
	const size_t           d_maxWorkers;
	std::mutex             d_mutex;
	size_t                 d_nbActiveWorkers;
	ProcessQueue::iterator d_current,d_end;
	fort::FrameReadout     d_message;
	Frame::Ptr             d_frame;

	typedef std::vector<cv::Mat> ResultPool;

	ResultPool           d_results;
	ResultPool::iterator d_currentResult;
};
