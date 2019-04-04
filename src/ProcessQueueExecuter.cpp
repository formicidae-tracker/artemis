
#include "ProcessQueueExecuter.h"

#include <glog/logging.h>

ProcessQueueExecuter::ProcessQueueExecuter(asio::io_service & service, size_t workers)
	: d_service(service)
	, d_maxWorkers(workers)
	, d_nbActiveWorkers(0) {
}

ProcessQueueExecuter::~ProcessQueueExecuter() {
	DLOG(INFO) << "Cleaning ProcessQueueExecutor";
}

void ProcessQueueExecuter::Start(ProcessQueue & queue,const Frame::Ptr & frame) {
	std::lock_guard<std::mutex> lock(d_mutex);
	if ( d_next ){
		throw Overflow();
	}

	if (!IsDoneUnsafe()) {
		d_next = frame;
		d_nextBegin = queue.begin();
		d_nextEnd = queue.end();
		return;
	}

	Init(queue.begin(),queue.end(),frame);

	SpawnCurrentUnsafe();
}

std::string ProcessQueueExecuter::State() {
	std::ostringstream os;
	std::lock_guard<std::mutex> lock(d_mutex);
	os << (d_end - d_current) << " remaining, " << d_nbActiveWorkers << " active";
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

bool ProcessQueueExecuter::IsDoneUnsafe() {
	if ( d_nbActiveWorkers > 0 ) {
		return false;
	}

	if (d_current == d_end ) {
		return true;
	}
	return false;
}


void ProcessQueueExecuter::SpawnCurrentUnsafe() {
	auto jobs = (*d_current)->Prepare(d_maxWorkers,d_frame->ToCV().size());
	d_nbActiveWorkers += jobs.size();

	*(d_currentResult+1) = *d_currentResult;

	for(auto & j : jobs) {
		d_service.post([this,j](){
				j(d_frame,*d_currentResult,d_message,*(d_currentResult+1));
				{
					std::lock_guard<std::mutex> lock(d_mutex);
					if ( --d_nbActiveWorkers != 0 ) {
						//still unfinished work
						return;
					}

					if (++d_current == d_end){

						if ( d_next ) {

							Init(d_nextBegin,d_nextEnd,d_next);
							d_next.reset();
							SpawnCurrentUnsafe();
							return;

						}
						//clean current
						d_frame.reset();
						// no more job to do
						return;
					}

					++d_currentResult;
					SpawnCurrentUnsafe();
				}
			});
	}
}
