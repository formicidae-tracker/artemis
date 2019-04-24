#pragma once

#include "Family.h"

#include <opencv2/core.hpp>

namespace maytags {


class Detection {
public :
	Eigen::Vector2d Center;
	Eigen::Vector2d Corners[4];
	Eigen::Matrix3d H;

	size_t ID;
	size_t Hamming;
	double Goodness;
	double DecisionMargin;
	EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};



class Implementation;

class Detector  {
public :
	class QuadThresholdConfig {
	public:
		QuadThresholdConfig();
		size_t MinimumPixelPerCluster;
		size_t MaximumNumberOfMaxima;
		double CriticalCornerAngleRadian;
		double CosCriticalRadian;

		double MaxLineMSE;

		size_t MinimumBWDifference;
	};
	class Config {
	public :
		Config();

		std::vector<Family::Ptr> Families;

		double QuadDecimate;
		double QuadBlurSigma;
		bool RefineEdges;
		double DecodeSharpening;

		QuadThresholdConfig QuadConfig;
	};

	typedef std::vector<Detection,Eigen::aligned_allocator<Detection> > ListOfDetection;

	Detector(const Config & config);
	~Detector();

	void Detect(const cv::Mat & grayscale, ListOfDetection & detections);
private:
	std::unique_ptr<Implementation>  d_implementation;
};


} //namespace maytags
