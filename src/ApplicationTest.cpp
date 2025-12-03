#include "Application.hpp"
#include <gtest/gtest.h>

namespace fort {
namespace artemis {
class ApplicationTest : public ::testing::Test {};

TEST_F(ApplicationTest, FormatVersionCompatibleWithSemver) {
	struct TestData {
		std::string Describe, SHA1, Expected;
	};

	std::vector<TestData> testData = {
	    {"v1.2.3", "abcdef0", "artemis v1.2.3\nSHA1: abcdef0\n"},
	    {"v0.1.0-alpha", "1234567", "artemis v0.1.0-alpha\nSHA1: 1234567\n"},
	    {"v2.0.0-rc1-1-gdeadbee",
	     "deadbeef",
	     "artemis v2.0.0-rc1+1-gdeadbee\nSHA1: deadbeef\n"},
	};
	for (const auto &data : testData) {
		std::ostringstream oss;
		Application::formatVersion(
		    oss,
		    data.Describe.c_str(),
		    data.SHA1.c_str()
		);
		EXPECT_EQ(oss.str(), data.Expected);
	}
}

} // namespace artemis
} // namespace fort
