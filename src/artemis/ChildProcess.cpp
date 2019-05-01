#include "ChildProcess.h"

#include <utils/PosixCall.h>
#include <sys/wait.h>

#include <glog/logging.h>

ChildProcess::ChildProcess(CPUID cpuid,const std::string & command, const std::string & args)
	: d_childPID(0)
	, d_terminated(false)
	, d_code(-1)
	, d_cpu(cpuid){
	cpu_set_t  mask;
	CPU_ZERO(&mask);
	CPU_SET(cpuid, &mask);
	int res = fork();
	if ( res < 0 ) {
		throw ARTEMIS_SYSTEM_ERROR(fork,errno);
	}
	if (res == 0 ) {
		d_childPID = 0;
		try {
			close(STDOUT_FILENO);
			close(STDERR_FILENO);
			//			p_call(sched_setaffinity,0,sizeof(mask),&mask);
			p_call(execlp,command.c_str(),command.c_str(),args.c_str(),(char*)NULL);
		} catch (const std::exception & e ) {
			LOG(ERROR) << "Could not start process: " << e.what();
		}
	} else {
		d_childPID = res;
		d_waitThread = std::thread([this](){
				int status;
				for (;;) {
					int pid = waitpid(d_childPID,&status,0);
					if ( pid == -1 ) {
						LOG(ERROR) <<  ((ARTEMIS_SYSTEM_ERROR(waitpid,errno)).what());
						continue;
					}
					if ( pid != d_childPID ) {
						LOG(ERROR) << "Mismatched PID";
						continue;
					}
					if (WIFEXITED(status) ) {
						std::lock_guard<std::mutex> lock(d_mutex);
						d_terminated = true;
						d_code = WEXITSTATUS(status);
						return;
					}

					if (WIFSIGNALED(status) ) {
						std::lock_guard<std::mutex> lock(d_mutex);
						d_terminated = true;
						d_code = WTERMSIG(status);
						return;
					}
				}
			});
	}
}

ChildProcess::~ChildProcess() {
	if ( Ended() == false ) {
		Terminate();
	}
	d_waitThread.join();
}

void ChildProcess::Terminate() {
	kill(d_childPID,SIGINT);
}

bool ChildProcess::Ended() {
	std::lock_guard<std::mutex> lock(d_mutex);
	return d_terminated;
}

int ChildProcess::Status() {
	std::lock_guard<std::mutex> lock(d_mutex);
	return d_code;
}

CPUID ChildProcess::CPU() {
	std::lock_guard<std::mutex> lock(d_mutex);
	return d_cpu;
}
