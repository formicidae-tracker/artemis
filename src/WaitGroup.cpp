#include "WaitGroup.h"



WaitGroup::WaitGroup()
	: d_value(0) { }

WaitGroup::~WaitGroup(){}

void WaitGroup::Done(){
	Add(-1);
}

bool WaitGroup::IsDone() {
	std::lock_guard<std::mutex> lock(d_mutex);
	return d_value == 0;
}

void WaitGroup::Wait(){
	std::unique_lock<std::mutex> lock(d_mutex);
	d_signal.wait(lock,[this] { return this->d_value == 0 ; });
}

void WaitGroup::Add(int i) {
	std::lock_guard<std::mutex> lock(d_mutex);
	if ( (d_value + i) < 0 ) {
		throw std::runtime_error("WaitGroup: negative value");
	}
	d_value += i;

	if (d_value == 0 ) {
		d_signal.notify_all();
	}
}
