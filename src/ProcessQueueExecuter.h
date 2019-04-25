#pragma once

#include <mutex>

#include "ProcessDefinition.h"

#include "hermes/FrameReadout.pb.h"

#include <utils/CPUMap.h>

#include <thread>
#include <condition_variable>

class Frame;
typedef std::shared_ptr<Frame> FramePtr;

class ProcessQueueExecuter  {
public:
	class Overflow : public std::runtime_error {
	public:
		Overflow() : std::runtime_error("Process Overflow") {}
		virtual ~Overflow() {}
	};

	ProcessQueueExecuter(const std::vector<CPUID> & cpus);
	virtual ~ProcessQueueExecuter();



	void Start(ProcessQueue & queue, const Frame::Ptr & current);
	std::string State();

	void Loop();
	void Stop();



private:
	class WaitGroup {
	public:
		typedef std::shared_ptr<WaitGroup> Ptr;
		WaitGroup();
		~WaitGroup();

		void Add(int i);
		void Done();
		void Wait();
		int  Remaining();
	protected:
		int                     d_onWait;
		std::mutex              d_mutex;
		std::condition_variable d_signal;
	};

	class WorkerThread {
	public:
		typedef std::shared_ptr<WorkerThread> Ptr;

		WorkerThread(const WaitGroup::Ptr & group,CPUID cpuid);
		~WorkerThread();

		void Post(std::function<void()> job);
		void Stop();
		void Loop();
	private:
		std::function<void()>   d_job;
		bool                    d_hasAJob,d_quit;
		WaitGroup::Ptr          d_waitGroup;

		std::mutex              d_mutex;
		std::condition_variable d_signal;
		std::thread             d_thread;
	};

	void Init(const ProcessQueue::iterator & begin,
	          const ProcessQueue::iterator & end,
	          const Frame::Ptr & frame);

	WaitGroup::Ptr                 d_waitGroup;
	std::vector<WorkerThread::Ptr> d_workers;



	ProcessQueue::iterator d_current,d_end,d_nextBegin,d_nextEnd;
	fort::FrameReadout     d_message;

	typedef std::vector<cv::Mat> ResultPool;

	ResultPool           d_results;
	ResultPool::iterator d_currentResult;

	std::mutex                d_mutex;
	std::condition_variable   d_signal;
	bool                      d_quit;

	Frame::Ptr             d_frame,d_next;

};
