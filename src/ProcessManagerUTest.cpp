#include "ProcessManagerUTest.h"

#include "ProcessManager.h"
#include "ResizeProcess.h"

#include <gmock/gmock.h>
#include <glog/logging.h>

using ::testing::AtLeast;
using ::testing::_;
using ::testing::Return;

void ProcessManagerUTest::SetUp() {
	d_events = EventManager::Create();
}

void ProcessManagerUTest::TearDown() {
	d_events.reset();
}



TEST_F(ProcessManagerUTest,Initialization) {
	EXPECT_THROW({
			ProcessManager(d_events,0);
		},std::invalid_argument);

	EXPECT_THROW({
			ProcessManager(EventManagerPtr(),1);
		},std::invalid_argument);
}



TEST_F(ProcessManagerUTest,PollingPipeline) {
	ProcessManager pm(d_events,
	                  3);

	int calls[3] = {0,0,0};

	pm.Start({[&calls](){++calls[0];},[&calls](){++calls[1];},[&calls](){++calls[2];} });
	bool done = false;
	while(!done) {
		if (d_events->NextEvent() == EventManager::PROCESS_NEED_REFRESH ) {
			done = pm.IsDone();
		}
	}
	for (size_t i = 0; i < 3; ++i) {
		EXPECT_EQ(calls[i],1);
	}

	pm.Start({[&calls](){++calls[1];}});
	done = false;
	while(!done) {
		if (d_events->NextEvent() == EventManager::PROCESS_NEED_REFRESH ) {
			done = pm.IsDone();
		}
	}

	EXPECT_EQ(calls[0],1);
	EXPECT_EQ(calls[1],2);
	EXPECT_EQ(calls[2],1);


}


TEST_F(ProcessManagerUTest,BlockingPipeline) {
	auto  pm = std::make_shared<ProcessManager>(d_events,
	                                            3);

	int calls[3] = {0,0,0};

	pm->Start({[&calls](){++calls[0];},[&calls](){++calls[1];},[&calls](){++calls[2];} });
	pm->Wait();
	for (size_t i = 0; i < 3; ++i) {
		EXPECT_EQ(calls[i],1);
	}

	pm->Start({[&calls](){++calls[1];}});
	pm->Wait();
	EXPECT_EQ(calls[0],1);
	EXPECT_EQ(calls[1],2);
	EXPECT_EQ(calls[2],1);

	//needed as condition signaling is done before EventManager signaling
	pm.reset();

	size_t nbUpdate = 0;
	bool done = false;
	while(!done) {

		switch (d_events->NextEvent() ) {
		case EventManager::NONE: {
			done = true;
			break;
		}
		case EventManager::PROCESS_NEED_REFRESH : {
			++nbUpdate;
		}
		default: {}
		}

	}
	EXPECT_EQ(nbUpdate,4);

}
