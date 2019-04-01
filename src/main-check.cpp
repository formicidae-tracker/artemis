#include <gmock/gmock.h>
#include <glog/logging.h>

int main(int argc, char ** argv) {
	::testing::InitGoogleMock(&argc, argv);
	FLAGS_stderrthreshold = 0;

	::google::InitGoogleLogging(argv[0]);
	google::InstallFailureSignalHandler();
	return RUN_ALL_TESTS();
}
