#pragma once

#include "Detector.h"

#include "GradientClusterizer.h"

#include <Eigen/StdVector>

namespace maytags {



class QuadFitter {
public :
	class Quad {
	public:
		Eigen::Vector2d Corners[4];
		Eigen::Matrix3d H;

		bool ReversedBorder;
		void ComputeHomography();

		void Project(const Eigen::Vector2d & p, Eigen::Vector2d & res);
		EIGEN_MAKE_ALIGNED_OPERATOR_NEW;
		const static Eigen::Vector2d NormalizedCorners[4];
	};
	typedef std::vector<Quad,Eigen::aligned_allocator<Quad> > ListOfQuads;

	QuadFitter();

	ListOfQuads Quads;
	void FitQuads(const cv::Mat & image,
	              const Detector::QuadThresholdConfig & config,
	              size_t minTagWidth,
	              bool normalBorder,
	              bool reversedBorder,
	              GradientClusterizer::ClusterMap & clusters);
	void PrintDebug(const cv::Size & size, cv::Mat & image);

private:
	Detector::QuadThresholdConfig d_config;


	void FitQuad(const cv::Mat & image,
	             size_t minTagWidth,
	             bool normalBorder,
	             bool reversedBorder,
	             GradientClusterizer::ListOfPoint & cluster);

	class Moment {
	public:
		Eigen::Vector2d Moment1;
		Eigen::Vector3d Moment2;
		double Weight;
		EIGEN_MAKE_ALIGNED_OPERATOR_NEW;
	};
	typedef std::vector<Moment,Eigen::aligned_allocator<Moment> > CumulativeMoment;

	void ComputeCumulativeMoment(const cv::Mat & image, const GradientClusterizer::ListOfPoint & cluster, CumulativeMoment & result);

	typedef std::vector<Eigen::Vector4d,Eigen::aligned_allocator<Eigen::Vector4d> > QuadLines;

	void FindSegmentMaxima(const CumulativeMoment & moments, QuadLines & lines);

	void FitLine(const CumulativeMoment & moments, size_t start, size_t end, Eigen::Vector4d * line, double * err, double * mse);

	Eigen::VectorXd d_smoothKernel;


};



}
