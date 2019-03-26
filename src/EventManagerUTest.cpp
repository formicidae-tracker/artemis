#include "EventManagerUTest.h"

#include <csignal>

void EventManagerUTest::SetUp() {
	d_manager = EventManager::Create();
}


void EventManagerUTest::TearDown() {
}


TEST_F(EventManagerUTest,IsSingleton){
	EXPECT_THROW({
			EventManager::Create();
		}, std::runtime_error);
}


TEST_F(EventManagerUTest,EventReception) {
	EventManager::Event e;

	EXPECT_NO_THROW({
			d_manager->Signal(EventManager::FRAME_READY);
			e = d_manager->NextEvent();
		});

	EXPECT_EQ(e,EventManager::FRAME_READY);
}


TEST_F(EventManagerUTest,SIGINTEmitsQuitEvent) {
	std::raise(SIGINT);
	auto e = d_manager->NextEvent();
	EXPECT_EQ(e,EventManager::QUIT);
}


TEST_F(EventManagerUTest,EventFormatting) {
	std::map<EventManager::Event,std::string> testdata;

	testdata[EventManager::NONE] = "Event.NONE";
	testdata[EventManager::QUIT] = "Event.QUIT";
	testdata[EventManager::FRAME_READY] = "Event.FRAME_READY";
	testdata[EventManager::PROCESS_NEED_REFRESH] = "Event.PROCESS_NEED_REFRESH";
	testdata[(EventManager::Event)255] = "<unknown EVENT>";

	for ( auto & kv : testdata) {
		std::ostringstream oss;
		oss << kv.first;
		EXPECT_EQ(oss.str(),kv.second);
	}
}
