#include "taskflow/taskflow.hpp"
#include "taskflow/core/executor.hpp"
#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

class TaskflowTest : public ::testing::Test {};

TEST_F(TaskflowTest, CorunJoinSubtasks) {
	tf::Taskflow     taskflow;
	std::atomic<int> counter{0};

	taskflow.emplace([&counter](tf::Runtime &rt) {
		for (size_t i = 0; i < 100; ++i) {
			rt.silent_async([&counter]() {
				std::this_thread::sleep_for(std::chrono::milliseconds{10});
				counter++;
			});
		}
		rt.corun();
		EXPECT_EQ(counter.load(), 100);
	});

	tf::Executor executor;
	executor.run(taskflow).wait();
}
