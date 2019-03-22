#pragma once

#include <condition_variable>

class WaitGroup {
public:
	WaitGroup();
	~WaitGroup();

	void Add(int i);
	void Done();
	void Wait();

private :
	int d_value;
	std::mutex d_mutex;
	std::condition_variable d_signal;
};
