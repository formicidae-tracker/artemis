#pragma once

#include <mutex>


#include "ProcessDefinition.h"

#include <common/InterprocessBuffer.h>
#include "ChildProcess.h"
#include <condition_variable>

#include "hermes/FrameReadout.pb.h"
#include "Connection.h"
#include <utils/Partitions.h>
#include <utils/CPUMap.h>

class Frame;
typedef std::shared_ptr<Frame> FramePtr;

class ProcessQueueExecuter  {
public:
	class Overflow : public std::runtime_error {
	public:
		Overflow() : std::runtime_error("Process Overflow") {}
		virtual ~Overflow() {}
	};

	ProcessQueueExecuter(const cv::Size & size,
	                     const DetectionConfig & config,
	                     const std::vector<CPUID> cpus,
	                     const PreTagQueue & preTagQueue,
	                     const PostTagQueue & postTagQueue,
	                     const Connection::Ptr & connection);
	virtual ~ProcessQueueExecuter();

	void Push(const Frame::Ptr & current);
	void Loop();
	void Stop();
	std::string State();

	void RestartBrokenChilds();

	static std::string FindOrionPath();

private:
	void ReconciliateTag();
	std::string                     d_orion;
	Partition                       d_partition;
	PreTagQueue                     d_preTagQueue;
	PostTagQueue                    d_postTagQueue;
	Connection::Ptr                 d_connection;


	InterprocessManager::Ptr        d_manager;
	std::vector<std::unique_ptr<InterprocessBuffer> > d_buffers;
	std::vector<std::unique_ptr<ChildProcess> >      d_children;

	std::mutex                      d_mutex;
	std::condition_variable         d_start;
	bool                            d_quit;

	Frame::Ptr                      d_frame;
	Frame::Ptr                      d_next;
	size_t                          d_respawn;
};
