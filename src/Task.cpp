#include "Task.hpp"

#include <unistd.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <sys/resource.h>

#include "utils/PosixCall.hpp"


namespace fort {
namespace artemis {

Task::~Task() {}

std::thread Task::Spawn(Task & task,size_t niceness) {
	return std::thread([&task,niceness]() {
		                   if ( niceness != 0 ) {
			                   auto tid = syscall(SYS_gettid);
			                   p_call(setpriority,PRIO_PROCESS,tid,niceness);
		                   }
		                   task.Run();
	                   });
}

} // namespace artemis
} // namespace fort
