#include "UtilsUTest.hpp"

#include <opencv2/core.hpp>

#include "Utils.hpp"

TEST_F(UtilsUTest,DoNotFail) {
	struct TestData {
		cv::Size Source,Dest;
		cv::Point Center;
		cv::Rect Expected;
	};

	std::vector<TestData> testdata =
		{
		 {{100,100},{50,50},{50,50},{0,0,50,50}},
		 {{100,100},{50,50},{20,50},{5,0,45,50}},
		 {{100,100},{50,50},{50,20},{0,5,50,45}},
		 {{100,100},{50,50},{20,20},{5,5,45,45}},
		 {{100,100},{50,50},{0,50},{25,0,25,50}},
		 {{100,100},{50,50},{50,0},{0,25,50,25}},
		 {{100,100},{50,50},{0,0},{25,25,25,25}},
		 {{100,100},{50,50},{80,50},{0,0,45,50}},
		 {{100,100},{50,50},{50,80},{0,0,50,45}},
		 {{100,100},{50,50},{80,80},{0,0,45,45}},
		 {{100,100},{50,50},{100,50},{0,0,25,50}},
		 {{100,100},{50,50},{50,100},{0,0,50,25}},
		 {{100,100},{50,50},{100,100},{0,0,25,25}},
		};

	for ( const auto & d : testdata ) {
		cv::Mat source = cv::Mat(d.Source,CV_8UC1);
		source.setTo(255);
		cv::Mat res;
		try {
			res = fort::artemis::GetROICenteredAtBlackBorder(source,d.Center,d.Dest);
		} catch (const std::exception & e) {
			ADD_FAILURE() << "could not get roi: " << e.what();
			continue;
		}

		EXPECT_EQ(res.size(),d.Dest);
		for ( int y = 0; y < d.Dest.height; ++y) {
			for ( int x = 0; x < d.Dest.width; ++x) {
				auto p = cv::Point(x,y);
				EXPECT_EQ(res.at<uint8_t>(p),d.Expected.contains(p) ? 255 : 0) << " at point " << p;
			}
		}
	}



}
