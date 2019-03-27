#pragma once

#include <vector>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>

#include "WaitGroup.h"

#include <opencv2/core.hpp>

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


class ProcessManager {
public:
	ProcessManager(const EventManagerPtr & eventManager, size_t nbWorkers);


	void Start(const std::vector<std::function < void()> > & jobs);
	bool IsDone();
	void Wait();

private :
	typedef std::function<void()> Job;
	class Worker {
	public :
		Worker();
		~Worker();
		void StartJob(WaitGroup & wg, const EventManagerPtr & eventManager, const Job & j);
	private:
		void Loop();

		std::mutex d_mutex;
		std::condition_variable d_signal;
		std::shared_ptr<Job> d_job;
		std::atomic<bool> d_quit;
		std::thread d_workThread;

	};
	typedef std::vector<std::unique_ptr<Worker> > WorkerPool;

	EventManagerPtr d_eventManager;
	WaitGroup       d_waitGroup;
	WorkerPool      d_workers;
};
