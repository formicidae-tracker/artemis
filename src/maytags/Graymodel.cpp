#include "Graymodel.h"

#include <Eigen/Dense>

using namespace maytags;

Graymodel::Graymodel() {
	d_A.setZero();
	d_B.setZero();
	d_C.setZero();
}

void Graymodel::Add(const Eigen::Vector2d & position, double gray) {
	d_A(0,0) += position.x()* position.x();
	d_A(0,1) += position.x()* position.y();
	d_A(0,2) += position.x();
	d_A(1,1) += position.y()* position.y();
	d_A(1,2) += position.y();
	d_A(2,2) += 1;
	d_B += Eigen::Vector3d(position.x(),position.y(),1)*gray;
}

void Graymodel::Solve() {
	//Makes A symmetric
	d_A(1,0) = d_A(0,1);
	d_A(2,0) = d_A(0,2);
	d_A(2,1) = d_A(1,2);

	//Solve using LDLT decomposition.
	d_C = Eigen::LDLT<Eigen::Matrix3d >(d_A).solve(d_B);
}

double Graymodel::Interpolate(const Eigen::Vector2d & position) {
	return d_C.dot(Eigen::Vector3d(position.x(),position.y(),1.0));
}
