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
	ProcessManager(const std::vector<ProcessDefinitionPtr> & processes,
	               const EventManagerPtr & eventManager,
	               size_t nbWorkers);


	void Start(const FramePtr & buffer,
	           const std::shared_ptr<cv::Mat> & finalFrame,
	           const FrameReadoutPtr & frameResult);
	bool Done();

private :
	typedef std::vector<ProcessDefinitionPtr>  ProcessPipeline;
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

	void SpawnNext();
	void SpawnFinalize();

	ProcessPipeline                 d_processes;
	ProcessPipeline::const_iterator d_currentProcess;
	std::vector<cv::Mat>            d_intermediaryResults;
	bool                            d_finalized;

	EventManagerPtr d_eventManager;
	WaitGroup       d_waitGroup;
	WorkerPool      d_workers;

	FramePtr                 d_currentFrame;
	std::shared_ptr<cv::Mat> d_currentFinalFrame;
	FrameReadoutPtr          d_currentFrameReadout;
};
