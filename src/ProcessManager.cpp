#include "ProcessManager.h"

#include "EventManager.h"
#include "ProcessDefinition.h"

ProcessManager::Worker::Worker()
	: d_quit(false)
	, d_workThread(&ProcessManager::Worker::Loop,this){
}

ProcessManager::Worker::~Worker() {
	d_quit.store(true);
	{
		std::lock_guard<std::mutex> lock(d_mutex);
		d_signal.notify_all();
	}
	d_workThread.join();
}


void ProcessManager::Worker::StartJob(WaitGroup & wg, const EventManager::Ptr & eventManager, const Job & j) {
	std::lock_guard<std::mutex> lock(d_mutex);

	d_job = std::make_shared<Job>([&wg,j,eventManager]{
			j();
			wg.Done();
			eventManager->Signal(EventManager::PROCESS_NEED_REFRESH);
		});

	wg.Add(1);
	d_signal.notify_all();
}

void ProcessManager::Worker::Loop() {
	while(d_quit.load() == false) {
		std::shared_ptr<Job> toDo;
		{
			std::unique_lock<std::mutex> lock(d_mutex);
			d_signal.wait(lock,  [this,&toDo]() -> bool {
					if ( d_quit.load() == true ) {
						return true;
					}
					if (!d_job ) {
						return false;
					}
					toDo = d_job;
					d_job.reset();
					return true;
				});
			if (d_quit.load() == true ) {
				return;
			}
		}
		(*toDo)();
	}

}

ProcessManager::ProcessManager(const std::vector<ProcessDefinitionPtr> & processes,
                               const EventManagerPtr & eventManager,
                               size_t nbWorkers)
	: d_processes(processes)
	, d_eventManager(eventManager) {
	if (nbWorkers == 0 ) {
		throw std::invalid_argument("ProcessManager: Need at least 1 worker");
	}
	if (processes.empty() ) {
		throw std::invalid_argument("ProcessManager: empty process pipeline");
	}
	if ( !eventManager ) {
		throw std::invalid_argument("ProcessManager: empty EventManager reference");
	}

	d_intermediaryResults.resize(d_processes.size() + 1);
	d_workers.reserve(nbWorkers);
	for ( size_t i = 0; i < nbWorkers; ++i ) {
		d_workers.push_back(std::unique_ptr<Worker>(new Worker()));
	}
	d_currentProcess = d_processes.cend();
}


void ProcessManager::Start(const FramePtr & buffer,
                           const std::shared_ptr<cv::Mat> & finalFrame,
                           const FrameReadoutPtr & frameResult) {
	d_currentFrame = buffer;
	d_currentFinalFrame = finalFrame;
	d_currentFrameReadout = frameResult;
	d_currentProcess = d_processes.cbegin();
	d_intermediaryResults.back() = *finalFrame;

	SpawnNext();
}


void ProcessManager::SpawnNext() {
	if (d_currentProcess == d_processes.cend() || d_finalized == false ) {
		return;
	}
	d_finalized = false;



	auto jobs = (*d_currentProcess)->Prepare(d_workers.size(),
	                                         d_currentFrame->ToCV().size());




	if (jobs.size() == 0) {
		SpawnFinalize();
		return;
	}
	if (jobs.size() > d_workers.size()) {
		throw std::runtime_error("Too many jobs defined");
	}

	size_t idx = d_currentProcess - d_processes.cbegin();

	for (size_t i = 0; i < jobs.size(); ++i) {
		d_workers[i]->StartJob(d_waitGroup,d_eventManager,[this,jobs,i,idx]() {
				(jobs[i])(d_currentFrame,*d_currentFrameReadout,d_intermediaryResults[idx]);
			});
	}
}


void ProcessManager::SpawnFinalize() {
	if (d_currentProcess == d_processes.cend() || d_finalized == true ) {
		return;
	}
	d_finalized = true;

	size_t idx = d_currentProcess - d_processes.cbegin();

	d_workers[0]->StartJob(d_waitGroup,d_eventManager,[this,idx](){
			(*d_currentProcess)->Finalize(d_intermediaryResults[idx],
			                              *d_currentFrameReadout,
			                              d_intermediaryResults[idx+1]);
		});

}

bool ProcessManager::Done() {
	if (d_waitGroup.IsDone() == false ) {
		return false;
	}
	if ( d_finalized == false ) {
		SpawnFinalize();
	}

	if (d_currentProcess == d_processes.cend() || ++d_currentProcess == d_processes.cend()) {
		d_currentFrame.reset();
		d_currentFinalFrame.reset();
		d_currentFrameReadout.reset();
		d_intermediaryResults.back() = cv::Mat();
		return true;
	}

	SpawnNext();

	return false;
}
