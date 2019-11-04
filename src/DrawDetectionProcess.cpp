#include "DrawDetectionProcess.h"

#include <opencv2/imgproc/imgproc.hpp>

#include <Eigen/Geometry>



DrawDetectionProcess::DrawDetectionProcess(){
}

DrawDetectionProcess::~DrawDetectionProcess() {
}


std::vector<ProcessFunction> DrawDetectionProcess::Prepare(size_t maxProcess, const cv::Size &) {
	std::vector<ProcessFunction> res;
	for( size_t i = 0 ; i< maxProcess; ++i) {
		res.push_back([this,i,maxProcess](const Frame::Ptr & frame,
		                             const cv::Mat &,
		                             fort::hermes::FrameReadout & readout,
		                             cv::Mat & result) {
			              double ratio = double(result.rows) / double(frame->ToCV().rows);
			              DrawAnts(i,maxProcess,readout,result,ratio);

		              });
	}
	return res;
}



void DrawDetectionProcess::DrawAnts(size_t start, size_t stride, const fort::hermes::FrameReadout & readout,cv::Mat & result, double ratio) {
	for (size_t i = start; i < readout.tags_size(); i += stride ) {
		DrawAnt(readout.tags(i),result,ratio);
	}
}
void DrawDetectionProcess::DrawAnt(const fort::hermes::Tag & a, cv::Mat & result,double ratio) {
	Eigen::Vector2d center(a.x(),a.y());
	center *= ratio;
	cv::circle(result,cv::Point(center.x(),center.y()),3,cv::Scalar(0x00,0x00,0xff),-1);

	center += Eigen::Vector2d(6,-2);

	static std::vector<std::vector<uint8_t> > glyphs
		= {
		   { 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1 }, // 0
		   { 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0 }, // 1
		   { 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1 }, // 2
		   { 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1 }, // 3
		   { 1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 0, 1, 0, 0, 1 }, // 4
		   { 1, 1, 1, 1, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 1 }, // 5
		   { 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1 }, // 6
		   { 1, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1 }, // 7
		   { 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1 }, // 8
		   { 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 0, 1, 0, 0, 1 }, // 9
	};

	std::ostringstream os;
	os << a.id();
	cv::Vec3b foreground(0,0xff,0);
	for( const auto & c : os.str() ) {
		if ( c < '0' || c > '9' ) {
			continue;
		}
		std::vector<uint8_t> g = glyphs[c - '0'];
		for(size_t y = 0; y < 5; ++y ) {
			for(size_t x = 0; x < 3; ++x) {
				int ix = center.x() + x;
				int iy = center.y() + y;
				size_t idx = y * 3 + x;
				if ( ix < 0 || ix > result.cols || iy < 0 || iy > result.rows || g[idx] == 0) {
					continue;
				}
				result.at<cv::Vec3b>(iy,ix) = foreground;
			}
		}
		center.x() += 5;
	}


}
