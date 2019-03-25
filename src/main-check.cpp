#include <gtest/gtest.h>
#include <glog/logging.h>

int main(int argc, char ** argv) {
	::testing::InitGoogleTest(&argc, argv);
	FLAGS_stderrthreshold = 4;

	::google::InitGoogleLogging(argv[0]);
	return RUN_ALL_TESTS();
}
