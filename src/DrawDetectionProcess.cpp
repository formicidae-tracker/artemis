#include "DrawDetectionProcess.h"

#include <opencv2/imgproc/imgproc.hpp>

#include <Eigen/Geometry>

#include <glog/logging.h>


DrawDetectionProcess::DrawDetectionProcess()
	: d_highlighted(NO_HIGHLIGHT) {
}

DrawDetectionProcess::~DrawDetectionProcess() {
}


void DrawDetectionProcess::SetHighlighted(uint32_t highlighted ) {
	LOG(INFO) << "Highlighting " << highlighted;
	d_highlighted = highlighted;
}

std::vector<ProcessFunction> DrawDetectionProcess::Prepare(size_t maxProcess, const cv::Size &) {
	std::vector<ProcessFunction> res;
	for( size_t i = 0 ; i< maxProcess; ++i) {
		res.push_back([this,i,maxProcess](const Frame::Ptr & frame,
		                             const cv::Mat &,
		                             fort::hermes::FrameReadout & readout,
		                             cv::Mat & result) {
			              double ratio = double(result.rows) / double(frame->ToCV().rows);
			              DrawAnts(i,maxProcess,readout,result,ratio,d_highlighted);

		              });
	}
	return res;
}



void DrawDetectionProcess::DrawAnts(size_t start, size_t stride, const fort::hermes::FrameReadout & readout,cv::Mat & result, double ratio, uint32_t highlighted) {
	for (size_t i = start; i < readout.tags_size(); i += stride ) {
		DrawAnt(readout.tags(i),result,ratio, highlighted);
	}
}

void DrawDetectionProcess::DrawAnt(const fort::hermes::Tag & a, cv::Mat & result,double ratio, uint32_t highlighted) {
	cv::Vec3b foreground(0xff,0xff,0xff);
	static cv::Vec3b background(0x00,0x00,0x00);
	static cv::Scalar circleColor = cv::Scalar(0x00,0x00,0xff);
	static cv::Scalar circleHighlight = cv::Scalar(0xff,0xff,0x00);

	Eigen::Vector2d center(a.x(),a.y());
	center *= ratio;
	if ( a.id() == highlighted ) {
		foreground = cv::Vec3b(0xff,0xff,0x00);
		cv::circle(result,cv::Point(center.x(),center.y()),6,circleHighlight,-1);
	} else {
		cv::circle(result,cv::Point(center.x(),center.y()),3,circleColor,-1);
	}

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



	for( const auto & c : os.str() ) {
		if ( c < '0' || c > '9' ) {
			continue;
		}
		std::vector<uint8_t> g = glyphs[c - '0'];
		for(int y = -1; y < 6; ++y ) {
			for(int x = -1; x < 4; ++x) {
				int ix = center.x() + x;
				int iy = center.y() + y;
				size_t idx = y * 3 + x;
				if ( ix < 0 || ix > result.cols || iy < 0 || iy > result.rows) {
					continue;
				}
				if (x < 0 || x >= 3 || y < 0 || y >= 5 || g[idx] == 0 ) {
					result.at<cv::Vec3b>(iy,ix) = background;
				} else {
					result.at<cv::Vec3b>(iy,ix) = foreground;
				}
			}
		}
		center.x() += 5;
	}


}
