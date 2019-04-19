#pragma once


#include "GradientClusterizer.h"

#include <Eigen/StdVector>

namespace maytags {



class QuadFitter {
public :
	class Quad {
	public:
		bool ReversedBorder;

		Eigen::Matrix3d H,Hinv;
		Eigen::Vector2d Corners[4];

		EIGEN_MAKE_ALIGNED_OPERATOR_NEW;
	};
	typedef std::vector<Quad,Eigen::aligned_allocator<Quad> > ListOfQuads;

	ListOfQuads Quads;
	void FitQuads(const cv::Mat & image,
	              size_t minTagWidth,
	              bool normalBorder,
	              bool reversedBorder,
	              size_t minClusterPixel,
	              GradientClusterizer::ClusterMap & clusters);

	void PrintDebug(const cv::Size & size, cv::Mat & image);
};

}
