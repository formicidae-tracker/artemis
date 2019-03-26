#include "WaitGroupUTest.h"

#include <thread>
#include <chrono>
#include <vector>
#include "WaitGroup.h"


TEST(WaitGroupUTest,TestWaiting) {
	WaitGroup wg;

	const size_t nbWorkers = 3;

	std::vector<std::thread> workers;
	bool flags[nbWorkers];
	workers.reserve(nbWorkers);



	for (size_t i = 0 ; i < nbWorkers; ++i) {
		flags[i] = false;
		wg.Add(1);
		workers.push_back(std::thread([&wg,&flags,i](){
					std::this_thread::sleep_for(std::chrono::milliseconds(2));
					wg.Done();
					flags[i] = true;
				}));
	}
	wg.Wait();
	for ( size_t i = 0 ; i < nbWorkers; ++i ) {
		EXPECT_EQ(flags[i], true);
		workers[i].join();
	}
}


TEST(WaitGroupUtest,TestPolling) {
	WaitGroup wg;
	EXPECT_EQ(wg.IsDone(),true);
	wg.Add(1);
	EXPECT_EQ(wg.IsDone(), false);
	wg.Done();
	EXPECT_EQ(wg.IsDone(), true);
}
