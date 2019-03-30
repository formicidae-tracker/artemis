#pragma once

#include <vector>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>

#include "WaitGroup.h"

#include <opencv2/core.hpp>


class EventManager;
typedef std::shared_ptr<EventManager> EventManagerPtr;


class WorkerPool {
public:
	WorkerPool(const EventManagerPtr & eventManager, size_t nbWorkers);


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
	typedef std::vector<std::unique_ptr<Worker> > Pool;

	EventManagerPtr d_eventManager;
	WaitGroup       d_waitGroup;
	Pool      d_workers;
};
