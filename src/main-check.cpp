#include <gmock/gmock.h>
#include <glog/logging.h>

int main(int argc, char **argv) {
	::testing::InitGoogleMock(&argc, argv);

	return RUN_ALL_TESTS();
}
