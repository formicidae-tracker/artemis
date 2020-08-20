#pragma once

#include <thread>

namespace fort {
namespace artemis {

class Task {
public:
	virtual ~Task();

	virtual void Run() = 0;

	static std::thread Spawn(Task & task,size_t niceness);
};

} // namespace artemis
} // namespace fort
