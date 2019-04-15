#pragma once

#include "ComponentConnecter.h"

#include <map>

#include <Eigen/Core>
#include <Eigen/StdVector>

namespace maytags {



class GradientClusterizer {
public:
	class Point {
	public:
		Eigen::Vector2d Position,Gradient;
		double Slope;
		EIGEN_MAKE_ALIGNED_OPERATOR_NEW
	};

	typedef std::vector<Point,Eigen::aligned_allocator<Point> > ListOfPoint;

	typedef std::map<uint64_t, ListOfPoint> ClusterMap;

	void GradientCluster(const cv::Mat & binaryImage,ComponentConnecter & cc, size_t minClusterSize);

	void DebugPrint(cv::Mat & image);
private:

};


}
