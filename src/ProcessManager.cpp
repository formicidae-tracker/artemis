#include "ProcessManager.h"

#include "EventManager.h"


ProcessManager::Worker::Worker()
	: d_quit(false)
	, d_workThread(&ProcessManager::Worker::Loop,this){
}

ProcessManager::Worker::~Worker() {
	d_quit.store(true);
	d_workThread.join();
}


void ProcessManager::Worker::StartJob(WaitGroup & wg, const EventManager::Ptr & eventManager, Job & j) {
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
					if (!d_job) {
						return false;
					}
					toDo = d_job;
					d_job.reset();
					return true;
				});
		}
		(*toDo)();
	}

}
