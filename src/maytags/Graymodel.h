#pragma once

#include <Eigen/Core>

namespace maytags {

class Graymodel {
public:
	Graymodel();

	void Add(const Eigen::Vector2d & position, double gray);
	void Solve();
	double Interpolate(const Eigen::Vector2d & position);


	Eigen::Matrix3d d_A;
	Eigen::Vector3d d_B,d_C;

	//No fixed Vectorizable size;

};

}
