#include "ProcessQueueExecuterUTest.h"

#include <condition_variable>
#include "ProcessDefinition.h"

#include "ProcessQueueExecuter.h"

#include <glog/logging.h>

void ProcessQueueExecuterUTest::SetUp() {
	d_threads.clear();
	d_threads.reserve(3);
	d_work = std::make_shared<asio::io_service::work>(d_service);


	for(size_t i = 0; i < 3; ++i) {
		d_threads.push_back(std::thread([this]{d_service.run();}));
	}

}

void ProcessQueueExecuterUTest::TearDown() {
	d_work.reset();
	d_service.stop();
	for ( auto & t : d_threads) {
		t.join();
	}
}

class BarrierProcessDefinition : public ProcessDefinition {
public:
	BarrierProcessDefinition(size_t maxParallel): d_maxParallel(maxParallel),d_current(0), d_done(false),d_waiting(true) {}
	virtual ~BarrierProcessDefinition(){};

	void StartOne() {
		std::lock_guard<std::mutex> lock(d_mutexStart);
		d_waiting = false;
		d_signalStart.notify_one();
	}


	void WaitOneFinish() {
		std::unique_lock<std::mutex> lock(d_mutexEnd);
		d_signalEnd.wait(lock,[this]() {
				if ( d_done == false ) {
					return false;
				}
				d_done = false;
				return true;
			});
	}

	size_t Current() {
		std::lock_guard<std::mutex> lock(d_mutexContent);
		return d_current;
	}


	std::vector<ProcessFunction> Prepare(size_t maxWorker, const cv::Size & size) {
		d_current = d_maxParallel;
		return std::vector<ProcessFunction>(d_maxParallel,[this] (const Frame::Ptr & f, const cv::Mat & upstream, fort::FrameReadout & m, cv::Mat & result){
				std::unique_lock<std::mutex> lock(d_mutexStart);
				d_signalStart.wait(lock,[this]() -> bool {
						if ( d_waiting == true ) {
							return false;
						}
						d_waiting = true;
						return true;
					});
				{
					std::lock_guard<std::mutex> lockContent(d_mutexContent);
					--d_current;
				}
				std::lock_guard<std::mutex> lockEnd(d_mutexEnd);
				d_done = true;
				d_signalEnd.notify_one();
			});
	}

private:
	size_t                  d_maxParallel,d_current;
	bool                    d_waiting,d_done;
	std::mutex              d_mutexStart,d_mutexEnd,d_mutexContent;
	std::condition_variable d_signalStart,d_signalEnd;

};


class StubFrame : public Frame {
public:
	StubFrame() {}
	virtual ~StubFrame() {}
	void * Data() { return NULL; }
	size_t Width() const { return 0; }
	size_t Height() const { return 0; }
	uint64_t Timestamp() const { return 0; }
	uint64_t ID() const { return 0; }
	const cv::Mat & ToCV() { return d_mat; }
private :
	cv::Mat d_mat;
};

TEST_F(ProcessQueueExecuterUTest,ExecuteProcessQueue) {
	std::vector<std::shared_ptr<BarrierProcessDefinition> > barriers;
	barriers.push_back(std::make_shared<BarrierProcessDefinition>(1));
	barriers.push_back(std::make_shared<BarrierProcessDefinition>(2));
	barriers.push_back(std::make_shared<BarrierProcessDefinition>(3));
	barriers.push_back(std::make_shared<BarrierProcessDefinition>(2));
	barriers.push_back(std::make_shared<BarrierProcessDefinition>(1));

	ProcessQueue pq;
	for (auto const & b : barriers ) {
		pq.push_back(b);
	}


	ProcessQueueExecuter pe(d_service,3);



	auto expect_state([&barriers,&pe](bool done,size_t b0,size_t b1,size_t b2, size_t b3, size_t b4) {
			EXPECT_EQ(pe.IsDone(),done);
			EXPECT_EQ(barriers[0]->Current(),b0);
			EXPECT_EQ(barriers[1]->Current(),b1);
			EXPECT_EQ(barriers[2]->Current(),b2);
			EXPECT_EQ(barriers[3]->Current(),b3);
			EXPECT_EQ(barriers[4]->Current(),b4);
		});


	expect_state(true,0,0,0,0,0);


	pe.Start(pq, std::make_shared<StubFrame>());
	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	expect_state(false,1,0,0,0,0);

	barriers[0]->StartOne();
	barriers[0]->WaitOneFinish();
	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	expect_state(false,0,2,0,0,0);



	barriers[1]->StartOne();
	barriers[1]->WaitOneFinish();
	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	expect_state(false,0,1,0,0,0);

	barriers[1]->StartOne();
	barriers[1]->WaitOneFinish();
	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	expect_state(false,0,0,3,0,0);



	barriers[2]->StartOne();
	barriers[2]->WaitOneFinish();
	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	expect_state(false,0,0,2,0,0);

	barriers[2]->StartOne();
	barriers[2]->WaitOneFinish();
	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	expect_state(false,0,0,1,0,0);

	barriers[2]->StartOne();
	barriers[2]->WaitOneFinish();
	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	expect_state(false,0,0,0,2,0);



	barriers[3]->StartOne();
	barriers[3]->WaitOneFinish();
	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	expect_state(false,0,0,0,1,0);

	barriers[3]->StartOne();
	barriers[3]->WaitOneFinish();
	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	expect_state(false,0,0,0,0,1);


	barriers[4]->StartOne();
	barriers[4]->WaitOneFinish();
	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	expect_state(true,0,0,0,0,0);



}
