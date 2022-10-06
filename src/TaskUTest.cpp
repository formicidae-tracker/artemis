#include "TaskUTest.hpp"

#include "Task.hpp"

#include <gtest/gtest.h>
#include <unistd.h>

namespace fort {
namespace artemis {

class SleepTask : public Task {
public:
	virtual ~SleepTask() {}

	void Run() override {
		usleep(100);
	}
};

TEST_F(TaskUTest,Niceness) {
	std::vector<size_t> n = {1,0,2};
	SleepTask t;
	for ( const auto & niceness : n ) {
		EXPECT_NO_THROW({
				auto th = Task::Spawn(t,niceness);
				th.join();
			}) << "For niceness " << niceness;
	}

}

} // namespace artemis
} // namespace fort
