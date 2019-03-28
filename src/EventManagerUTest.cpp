#include "EventManagerUTest.h"

#include <csignal>

void EventManagerUTest::SetUp() {
	d_manager = EventManager::Create(d_service);
}


void EventManagerUTest::TearDown() {
}


TEST_F(EventManagerUTest,IsSingleton){
	EXPECT_THROW({
			EventManager::Create(d_service);
		}, std::runtime_error);
}


TEST_F(EventManagerUTest,EventReception) {
	Event e;

	EXPECT_NO_THROW({
			d_manager->Signal(Event::FRAME_READY);
			e = d_manager->NextEvent();
		});

	EXPECT_EQ(e,Event::FRAME_READY);
}


TEST_F(EventManagerUTest,SIGINTEmitsQuitEvent) {
	std::raise(SIGINT);
	auto e = d_manager->NextEvent();
	EXPECT_EQ(e,Event::QUIT);
}


TEST_F(EventManagerUTest,EventFormatting) {
	std::map<Event,std::string> testdata;

	testdata[Event::QUIT] = "Event.QUIT";
	testdata[Event::FRAME_READY] = "Event.FRAME_READY";
	testdata[Event::PROCESS_NEED_REFRESH] = "Event.PROCESS_NEED_REFRESH";
	testdata[Event::NEW_ANT_DISCOVERED]= "Event.NEW_ANT_DISCOVERED";
	testdata[Event::NEW_READOUT]= "Event.NEW_READOUT";
	testdata[(Event)255] = "<unknown EVENT>";

	for ( auto & kv : testdata) {
		std::ostringstream oss;
		oss << kv.first;
		EXPECT_EQ(oss.str(),kv.second);
	}
}
