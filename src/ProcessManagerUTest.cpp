#include "ProcessManagerUTest.h"

#include "ProcessManager.h"
#include "ResizeProcess.h"

#include <gmock/gmock.h>
#include <glog/logging.h>

using ::testing::AtLeast;
using ::testing::_;
using ::testing::Return;

void ProcessManagerUTest::SetUp() {
	d_manager = EventManager::Create();
}

void ProcessManagerUTest::TearDown() {
	d_manager.reset();
}



TEST_F(ProcessManagerUTest,Initialization) {
	EXPECT_THROW({
			ProcessManager({std::make_shared<ResizeProcess>(10,10)},d_manager,0);
		},std::invalid_argument);

	EXPECT_THROW({
			ProcessManager({},d_manager,1);
		},std::invalid_argument);

	EXPECT_THROW({
			ProcessManager({std::make_shared<ResizeProcess>(10,10)},EventManagerPtr(),1);
		},std::invalid_argument);
}


class StubProcessDefinition : public ProcessDefinition {
public:
	virtual ~StubProcessDefinition() {}

	MOCK_METHOD2(Prepare, std::vector<ProcessFunction>(size_t nbProcess, const cv::Size & size));
	MOCK_METHOD3(Finalize, void (const cv::Mat & upstream, fort::FrameReadout & readout, cv::Mat & result));

};

TEST_F(ProcessManagerUTest,TestPipeline) {
	auto p1 = std::make_shared<StubProcessDefinition>();
	auto p2 = std::make_shared<StubProcessDefinition>();

	EXPECT_CALL(*p1,Finalize(_,_,_)).Times(AtLeast(1));
	EXPECT_CALL(*p1,Prepare(3,_))
		.Times(AtLeast(1))
		.WillRepeatedly(Return(std::vector<ProcessFunction>({
						[](const Frame::Ptr &, const fort::FrameReadout &, const cv::Mat & upstream){},
							[](const Frame::Ptr &, const fort::FrameReadout &, const cv::Mat & upstream){},
								[](const Frame::Ptr &, const fort::FrameReadout &, const cv::Mat & upstream){}
})));
	EXPECT_CALL(*p2,Finalize(_,_,_)).Times(AtLeast(1));
	EXPECT_CALL(*p1,Prepare(3,_))
		.Times(AtLeast(1))
		.WillRepeatedly(Return(std::vector<ProcessFunction>()));

	ProcessManager pm({p1,p2},
	                  d_manager,
	                  3);

	pm.Start(Frame::Ptr(),std::make_shared<cv::Mat>(),std::make_shared<fort::FrameReadout>());
	bool done = false;
	while(!done) {
		if (d_manager->NextEvent() == EventManager::PROCESS_NEED_REFRESH ) {
			done = pm.Done();
		}
	}
}
