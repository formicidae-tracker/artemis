#include "WaitGroupUTest.h"

#include <thread>
#include <chrono>
#include <vector>
#include "WaitGroup.h"

TEST(WaitGroupUTest,TestWaiting) {
	using namespace std::chrono_literals;
	WaitGroup wg;

	const size_t nbWorkers = 3;

	std::vector<std::thread> workers;
	bool flags[nbWorkers];
	workers.reserve(nbWorkers);



	for (size_t i = 0 ; i < nbWorkers; ++i) {
		flags[i] = false;
		wg.Add(1);
		workers.push_back(std::thread([&wg,&flags,i](){
					std::this_thread::sleep_for(2ms);
					wg.Done();
					flags[i] = true;
				}));
	}

	wg.Wait();
	for ( size_t i = 0 ; i < nbWorkers; ++i ) {
		EXPECT_EQ(flags[i], true);
	}
}
