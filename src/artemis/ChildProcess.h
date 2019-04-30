#pragma once

#include <common/InterprocessBuffer.h>
#include <thread>
#include <mutex>
#include "utils/CPUMap.h"
class ChildProcess {
public:
	ChildProcess(CPUID cpuid, const std::string & command, const std::string & arg);
	~ChildProcess();

	void Terminate();
	bool Ended();
	int Status();
	CPUID CPU();
private:
	int d_childPID;
	std::mutex  d_mutex;
	bool        d_terminated;
	int         d_code;
	std::thread d_waitThread;
	CPUID       d_cpu;
};
