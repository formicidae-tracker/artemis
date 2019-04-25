
#include "ProcessQueueExecuter.h"

#include <glog/logging.h>

#include "utils/PosixCall.h"




ProcessQueueExecuter::WaitGroup::WaitGroup() : d_onWait(0) {

}
ProcessQueueExecuter::WaitGroup::~WaitGroup() {
}

void ProcessQueueExecuter::WaitGroup::Add(int i) {
	std::lock_guard<std::mutex> lock(d_mutex);
	if (d_onWait + i < 0 ) {
		throw std::runtime_error("WaitGroup: could not go negative");
	}
	d_onWait += i;
	if (d_onWait == 0 ) {
		d_signal.notify_all();
	}
}

void ProcessQueueExecuter::WaitGroup::Done() {
	Add(-1);
}

int ProcessQueueExecuter::WaitGroup::Remaining() {
	return d_onWait;
}

void ProcessQueueExecuter::WaitGroup::Wait() {
	std::unique_lock<std::mutex> lock(d_mutex);
	d_signal.wait(lock,[this]()->bool {
			return d_onWait == 0;
		});
}


ProcessQueueExecuter::WorkerThread::WorkerThread(const WaitGroup::Ptr & wg,CPUID cpuid)
	: d_hasAJob(false)
	, d_quit(false)
	, d_waitGroup(wg) {
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(cpuid,&cpuset);
	DLOG(INFO) << "Spawning WorkerThread on CPU " << cpuid;

	d_thread = std::thread(&WorkerThread::Loop,this);

	// p_call(pthread_setaffinity_np,d_thread.native_handle(),sizeof(cpu_set_t),&cpuset);
}

ProcessQueueExecuter::WorkerThread::~WorkerThread() {
	Stop();
	d_thread.join();
}

void ProcessQueueExecuter::WorkerThread::Post(std::function<void()> job) {
	std::lock_guard<std::mutex> lock(d_mutex);
	if ( d_hasAJob == true ) {
		throw std::runtime_error("WorkerThread: Process overflow");
	}
	d_job = job;
	d_hasAJob = true;
	DLOG(INFO) << "WorkerThread: Notify";
	d_signal.notify_all();
}

void ProcessQueueExecuter::WorkerThread::Loop() {
	for (;;) {
		{
			std::unique_lock<std::mutex> lock(d_mutex);
			d_signal.wait(lock,[this]()->bool {
					if(d_quit) {
						return true;
					}
					return d_hasAJob;
				});
		}
		if ( d_quit == true ) {
			return;
		}
		DLOG(INFO) << "WorkerThread: Processing job";
		d_job();
		{
			std::lock_guard<std::mutex> lock(d_mutex);
			d_hasAJob = false;
		}
		if (d_waitGroup) {
			d_waitGroup->Done();
		}
	}

}


void ProcessQueueExecuter::WorkerThread::Stop() {
	std::lock_guard<std::mutex> lock(d_mutex);
	d_quit = true;
	d_signal.notify_all();
}




ProcessQueueExecuter::ProcessQueueExecuter(const std::vector<CPUID> & cpus)
	: d_waitGroup(std::make_shared<WaitGroup>())
	, d_quit(false) {

	for ( auto cpu : cpus) {
		d_workers.push_back(std::make_shared<WorkerThread>(d_waitGroup,cpu));
	}

}

ProcessQueueExecuter::~ProcessQueueExecuter() {
	DLOG(INFO) << "Cleaning ProcessQueueExecutor";
	Stop();
	d_workers.clear();
}


void ProcessQueueExecuter::Loop() {
	for (;;) {
		{
			std::unique_lock<std::mutex> lock(d_mutex);
			d_signal.wait(lock,[this]()->bool {
					if ( d_quit == true ) {
						return true;
					}
					return d_frame && (d_current != d_end);
				});
		}
		if ( d_quit ) {
			return;
		}
		auto jobs = (*d_current)->Prepare(d_workers.size(),d_frame->ToCV().size());
		d_waitGroup->Add(jobs.size());
		*(d_currentResult+1) = *d_currentResult;
		for (size_t i = 0; i  < std::min(d_workers.size(),jobs.size()); ++i) {
			d_workers[i]->Post([&jobs,i,this]() { jobs[i](d_frame,*d_currentResult,d_message,*(d_currentResult+1)); });
		}
		d_waitGroup->Wait();
		{
			std::lock_guard<std::mutex> lock(d_mutex);
			++d_currentResult;
			++d_current;
			if ( d_current != d_end ) {
				continue;
			}
			if ( d_next ) {
				Init(d_nextBegin,d_nextEnd,d_next);
				d_next.reset();
				continue;
			}
			d_frame.reset();
		}
	}
}

void ProcessQueueExecuter::Start(ProcessQueue & queue,const Frame::Ptr & frame) {
	std::lock_guard<std::mutex> lock(d_mutex);
	if ( d_next ){
		throw Overflow();
	}
	if ( d_frame ) {
		d_next = frame;
		d_nextBegin = queue.begin();
		d_nextEnd = queue.end();
		return;
	}


	Init(queue.begin(),queue.end(),frame);
	d_signal.notify_all();
}

void ProcessQueueExecuter::Stop() {
	std::lock_guard<std::mutex> lock(d_mutex);
	for(auto const & w : d_workers) {
		w->Stop();
	}
	d_quit = true;
}

std::string ProcessQueueExecuter::State() {
	std::ostringstream os;
	std::lock_guard<std::mutex> lock(d_mutex);
	os << (d_end - d_current) << " remaining, " << d_waitGroup->Remaining() << " active";
	return os.str();
}

void ProcessQueueExecuter::Init(const ProcessQueue::iterator & begin,
                                const ProcessQueue::iterator & end,
                                const Frame::Ptr & frame) {
	d_current = begin;
	d_end = end;

	d_frame = frame;
	d_results.resize((end-begin)+1);
	d_currentResult = d_results.begin();
}
