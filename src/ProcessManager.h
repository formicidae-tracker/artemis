#pragma once

#include <vector>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>

#include "WaitGroup.h"

class ProcessDefinition;
typedef std::shared_ptr<ProcessDefinition> ProcessDefinitionPtr;

class Frame;
typedef std::shared_ptr<Frame> FramePtr;

namespace fort {
class FrameReadout;
}
typedef std::shared_ptr<fort::FrameReadout> FrameReadoutPtr;

class EventManager;
typedef std::shared_ptr<EventManager> EventManagerPtr;

namespace cv {
class Mat;
}

class ProcessManager {
public:
	ProcessManager(const std::vector<ProcessDefinitionPtr> & processes,
	               const EventManagerPtr & eventManager,
	               size_t nbWorkers);


	void Start(const FramePtr & buffer, cv::Mat & finalFrame, fort::FrameReadout & frameResult);
	bool Done();

private :
	typedef std::function<void()> Job;
	class Worker {
	public :
		Worker();
		~Worker();
		void StartJob(WaitGroup & wg, const EventManagerPtr & eventManager, Job & j);
	private:
		void Loop();

		std::mutex d_mutex;
		std::condition_variable d_signal;
		std::shared_ptr<Job> d_job;
		std::atomic<bool> d_quit;
		std::thread d_workThread;

	};


	std::vector<ProcessDefinitionPtr> d_processes;
	EventManagerPtr d_eventManager;
	WaitGroup waitGroup;

};
