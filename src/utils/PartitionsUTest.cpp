#include "PartitionsUTest.h"

#include "Partitions.h"


TEST_F(PartitionsUTest,PartitionEqually) {
	struct TestData {
		cv::Rect  Base;
		size_t    Number;
		Partition Expected;
	};

	std::vector<TestData> testdata = {
		{
			.Base = cv::Rect(0,0,200,100),
			.Number = 1,
			.Expected = {
				cv::Rect(0,0,200,100),
			},
		},
		{
			.Base = cv::Rect(0,0,200,100),
			.Number = 2,
			.Expected = {
				cv::Rect(0,0,100,100),
				cv::Rect(100,0,100,100),
			},
		},
		{
			.Base = cv::Rect(0,0,200,100),
			.Number = 3,
			.Expected = {
				cv::Rect(0,0,66,100),
				cv::Rect(66,0,67,100),
				cv::Rect(133,0,67,100),
			},
		},
		{
			.Base = cv::Rect(0,0,100,100),
			.Number = 3,
			.Expected = {
				cv::Rect(0,0,33,100),
				cv::Rect(33,0,67,50),
				cv::Rect(33,50,67,50),
			},
		},
		{
			.Base = cv::Rect(0,0,200,100),
			.Number = 5,
			.Expected = {
				cv::Rect(0,0,40,100),
				cv::Rect(40,0,80,50),
				cv::Rect(40,50,80,50),
				cv::Rect(120,0,80,50),
				cv::Rect(120,50,80,50),
			},
		},
	};

	for( auto const & d : testdata ) {
		Partition res;
		PartitionRectangle(d.Base, d.Number, res);

		EXPECT_EQ(res.size(),d.Expected.size());

		for (size_t i = 0; i < std::min(res.size(),d.Expected.size()); ++i ) {
			EXPECT_EQ(res[i],d.Expected[i]);
		}
	}

}


TEST_F(PartitionsUTest,MarginAdding) {
	struct TestData {
		cv::Rect  Base;
		size_t    Margin;
		Partition Partitions;
		Partition Expected;
	};

	std::vector<TestData> testdata = {
		{
			.Base = cv::Rect(0,0,200,100),
			.Margin = 5,
			.Partitions = {
				cv::Rect(0,0,40,100),
				cv::Rect(40,0,80,50),
				cv::Rect(40,50,80,50),
				cv::Rect(120,0,80,50),
				cv::Rect(120,50,80,50),
			},
			.Expected = {
				cv::Rect(0,0,45,100),
				cv::Rect(35,0,90,55),
				cv::Rect(35,45,90,55),
				cv::Rect(115,0,85,55),
				cv::Rect(115,45,85,55),
			},
		},
	};

	for( auto const & d : testdata ) {
		Partition res;
		for (auto const & p : d.Partitions ) {
			res.push_back(p);
		}
		AddMargin(d.Base.size(), d.Margin, res);
		EXPECT_EQ(res.size(),d.Expected.size());

		for (size_t i = 0; i < std::min(res.size(),d.Expected.size()); ++i ) {
			EXPECT_EQ(res[i],d.Expected[i]);
		}
	}
}
