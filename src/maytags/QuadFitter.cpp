#include "QuadFitter.h"

#include <Eigen/Dense>

#include "ColorLabeller.h"

#include <opencv2/imgproc.hpp>

//#include <iostream>
//#include <iomanip>

using namespace maytags;

const Eigen::Vector2d QuadFitter::Quad::NormalizedCorners[4] = {
	Eigen::Vector2d(-1,-1),
	Eigen::Vector2d(1,-1),
	Eigen::Vector2d(1,1),
	Eigen::Vector2d(-1,1),
};


void QuadFitter::Quad::Project(const Eigen::Vector2d & p, Eigen::Vector2d & res) {
	Eigen::Vector3d homogenous(p.x(),p.y(),1.0);
	//no aliasing for multiplication with matrices and scalar
	homogenous = H * homogenous;
	homogenous /= homogenous.z();
	res = homogenous.block<2,1>(0,0);
}

void QuadFitter::Quad::ComputeHomography() {
	Eigen::Matrix<double,8,8> A;
	Eigen::Matrix<double,8,1> B;
	for(size_t i = 0; i < 4; ++i) {
		B.block<2,1>(2*i,0) = Corners[i];
		A.block<2,8>(2*i,0) <<
			NormalizedCorners[i].x() , 0 , - NormalizedCorners[i].x() * Corners[i].x(), NormalizedCorners[i].y(), 0 , -NormalizedCorners[i].y() * Corners[i].x(), 1 , 0,
			0  , NormalizedCorners[i].x(), - NormalizedCorners[i].x() * Corners[i].y(), 0 , NormalizedCorners[i].y(), -NormalizedCorners[i].y() * Corners[i].y(), 0 , 1;
	}

	H(2,2) = 1.0;
	Eigen::Map< Eigen::Matrix<double,8,1> > x(H.data());
	x = Eigen::PartialPivLU<Eigen::Matrix<double,8,8> >(A).solve(B);
}

QuadFitter::QuadFitter() {
	double sigma = 1.0;
	double cutoff = 0.05;

	int filterSize = std::sqrt(-std::log(cutoff)*2*sigma*sigma)+1;
	filterSize = 2*filterSize +1;

	d_smoothKernel = Eigen::VectorXd(filterSize);

	for ( int i = 0; i < filterSize; ++i ) {
		int j = i - filterSize / 2;
		d_smoothKernel(i) = std::exp(-j*(double)j/(2.0*sigma*sigma));
	}

}

bool ascendingSlope(const GradientClusterizer::Point & a, const GradientClusterizer::Point & b) {
	return a.Slope < b.Slope;
}


inline double atan2_sort(double y, double x) {
	if (std::abs(x) < 1e-8 ) {
		if ( (x < 0.0 && y < 0.0) || ( x >= 0.0 && y >= 0.0 ) ){
			return std::numeric_limits<double>::max();
		}
		return std::numeric_limits<double>::min();
	}
	return y/x;
}


void QuadFitter::FitQuad(const cv::Mat & image,
                         size_t minTagWidth,
                         bool normalBorder,
                         bool reversedBorder,
                         GradientClusterizer::ListOfPoint & cluster) {
	if ( cluster.size() < d_config.MinimumPixelPerCluster ) {
		// std::cerr << "Not enough pixel in cluster: " << cluster.size() << std::endl;
		return;
	}


	if (cluster.size() > 3*(2*image.cols+2*image.rows)) {
		// std::cerr << "Too many pixel in cluster: " << cluster.size() << std::endl;
		return;
	}

	double xMin(std::numeric_limits<double>::max());
	double xMax(std::numeric_limits<double>::min());
	double yMin(std::numeric_limits<double>::max());
	double yMax(std::numeric_limits<double>::min());

	for (auto const & p : cluster ) {
		if (p.Position.x() > xMax ) {
			xMax = p.Position.x();
		} else if (p.Position.x() < xMin ) {
			xMin = p.Position.x();
		}

		if (p.Position.y() > yMax ) {
			yMax = p.Position.y();
		} else if (p.Position.y() < yMin ) {
			yMin = p.Position.y();
		}
	}

	if ( (xMax-xMin) * (yMax - yMin) < minTagWidth ) {
		return;
	}
	Eigen::Vector2d center((xMax + xMin) * 0.5 + 0.05118,
	                       (yMax + yMin) * 0.5 - 0.028581);
	// std::cerr << " + center is : " << center.transpose() << std::endl;
	double dot = 0;
	for(auto & p : cluster ) {
		Eigen::Vector2d delta = p.Position - center;
		dot += delta.dot(p.Gradient);

		p.Slope = std::atan2(delta.y(),delta.x());
		//p.Slope = atan2_sort(delta.y(),delta.x());
	}

	Quad quad;
	quad.ReversedBorder = dot < 0.0;
	if (reversedBorder == false && quad.ReversedBorder == true) {
		// std::cerr << " + reversed border disabled." << std::endl;
		return;
	}

	if (normalBorder == false && quad.ReversedBorder == false) {
		// std::cerr << " + normal border disabled." << std::endl;
		return;
	}

	std::sort(cluster.begin(),cluster.end(),ascendingSlope);
	// std::cerr << " + before removing duplicates:" << cluster.size() << std::endl;
	//removes duplicate.
	auto last = cluster.begin();
	for ( auto iter = cluster.begin() + 1; iter != cluster.end(); ) {
		if ( last->Position == iter->Position ) {
			iter = cluster.erase(iter);
		} else {
			last = iter;
			++iter;
		}
	}
	// std::cerr << " + after removing duplicates:" << cluster.size() << std::endl;
	if ( cluster.size() < d_config.MinimumPixelPerCluster ) {
		// std::cerr << " + too few pixel left:" << cluster.size() << std::endl;
		return;
	}
	CumulativeMoment moments;

	ComputeCumulativeMoment(image,cluster,moments);

	QuadLines lines;
	FindSegmentMaxima(moments, lines);
	if ( lines.size() != 4 ) {
		// std::cerr << " + no quad found." << std::endl;
		return;
	}


	for ( size_t i = 0; i < 4; ++i ) {
		// std::cerr << " + line: " << lines[i].transpose() << std::endl;
		size_t j = (i+1)%4;
		Eigen::Matrix2d A;
		A << lines[i].w(), -(lines[j].w()),
			-(lines[i].z()), lines[j].z();
		Eigen::Vector2d B = lines[j].block<2,1>(0,0) - lines[i].block<2,1>(0,0);
		double det = A.determinant();
		if ( std::abs(det) < 1e-3 )  {
			// std::cerr << " + Could not solve corner." << std::endl;
			return;
		}
		Eigen::Vector2d W(A(1,1),-A(0,1));
		W /= det;
		quad.Corners[i] = lines[i].block<2,1>(0,0) + W.dot(B) * A.block<2,1>(0,0);
	}
	// std::cerr << " + last moments: " << std::fixed << std::setprecision(15) << moments[moments.size()-1].Moment2.transpose() << " " << moments[moments.size()-2].Moment2.transpose() << std::endl;

	double area = 0.0;
	Eigen::Vector3d sides;
	for (size_t i = 0; i < 3; ++i) {
		size_t j = (i+1)%3;
		sides[i] = (quad.Corners[j] - quad.Corners[i]).norm();
	}
	double p = sides.sum()/2;
	area += std::sqrt((p-sides.array()).prod()*p);
	size_t idxs[4] = {2,3,0,2};
	for (size_t i = 0; i < 3; ++i) {
		size_t idxa = idxs[i];
		size_t idxb = idxs[i+1];
		sides[i] = (quad.Corners[idxb]  - quad.Corners[idxa]).norm();
	}
	p = sides.sum()/2;
	area += std::sqrt((p-sides.array()).prod()*p);

	if (area < minTagWidth * minTagWidth ) {
		// std::cerr << " + Area is too small: " << area << std::endl;
		return;
	}

	//TODO : Check angles !!!

	Quads.push_back(quad);

}



struct LocalMaximum {
	size_t Idx;
	double Error;
};


inline bool descendingMaximum ( const LocalMaximum & a, const LocalMaximum & b) {
	return a.Error > b.Error;
}

inline bool ascendingIndices ( const LocalMaximum & a, const LocalMaximum & b) {
	return a.Idx < b.Idx;
}



void QuadFitter::FindSegmentMaxima(const CumulativeMoment & moments,QuadLines & lines) {
	lines.clear();
	lines.reserve(4);
	const int size = moments.size();
	int kernelSize = std::min(20,size/12);

	if ( kernelSize < 2 ) {
		// std::cerr << "    + KernelSize too small: " << kernelSize << std::endl;
		return;
	}
	Eigen::VectorXd errors(size);

	for(int i = 0; i < size; ++i ) {
		int start = (i + size - kernelSize) % size;
		int end = (i + kernelSize) % size;
		FitLine(moments,(size_t)start,(size_t)end,NULL,&(errors(i)),NULL);
	}


	Eigen::VectorXd smoothed(size);
	size_t filterSize = d_smoothKernel.rows();
	for (int i = 0; i < size; ++i) {
		smoothed(i) = 0.0;
		for(int j = 0; j < filterSize; ++j) {
			int idx = (i + j - filterSize/2 + size) % size;
			smoothed(i) += errors(idx) * d_smoothKernel(j);
		}
	}

	std::vector<LocalMaximum> maxima;
	maxima.reserve(size);
	for(int i = 0; i < size; ++i) {
		if ( smoothed(i) > smoothed((i+1)%size) && smoothed(i) > smoothed((i+size-1)%size) ) {
			maxima.push_back({.Idx = (size_t)i, .Error = smoothed(i)});
		}
	}
	// std::cerr << "    + Maxima : " << maxima.size() << std::endl;

	if (maxima.size() < 4 ) {
		// std::cerr << "    + Too few maxima: " << maxima.size() << std::endl;
		return;
	}

	if ( maxima.size() > d_config.MaximumNumberOfMaxima ) {
		// std::cerr << "    + maxima unsorted: " << maxima[0].Error << " " << maxima[1].Error << std::endl;
		std::sort(maxima.begin(),maxima.end(),descendingMaximum);
		// std::cerr << "    + maxima sorted: " << maxima[0].Error << " " << maxima[1].Error << std::endl;
		maxima.resize(d_config.MaximumNumberOfMaxima);
		std::sort(maxima.begin(),maxima.end(),ascendingIndices);
		// std::cerr << "    + maxima resorted: " << maxima[0].Idx << " " << maxima[1].Idx << std::endl;
	}

	//now we have some candidates for corners, we should just find the best fits !!
	double err[4];
	double mse[4];
	Eigen::Vector4d params[4];

	double bestError = std::numeric_limits<double>::max();
	lines.resize(4);

	for (size_t m0 = 0; m0 < maxima.size() - 3; ++m0 ) {
		size_t idx0 = maxima[m0].Idx;
		for ( size_t m1 = m0 + 1; m1 < maxima.size() - 2; ++m1) {
			int idx1 = maxima[m1].Idx;
			FitLine(moments,idx0,idx1,&(params[0]),&(err[0]),&(mse[0]));
			if (mse[0] > d_config.MaxLineMSE ) {
				// std::cerr << "    + line0 mse: " << mse[0] << std::endl;
				continue;
			}

			for ( size_t m2 = m1 + 1; m2 < maxima.size() - 1; ++m2 ) {
				int idx2 = maxima[m2].Idx;

				FitLine(moments,idx1,idx2,&(params[1]),&(err[1]),&(mse[1]));
				if (mse[1] > d_config.MaxLineMSE ) {
					// std::cerr << "    + line1 mse: " << mse[1] << std::endl;
					continue;
				}

				double dot = params[0].block<2,1>(2,0).dot(params[1].block<2,1>(2,0));
				if ( std::abs(dot) > d_config.CosCriticalRadian ) {
					// std::cerr << "    + angle 0-1: " << dot << std::endl;
					continue;
				}

				for ( size_t m3 = m2 + 1; m3 < maxima.size(); ++m3 ) {
					int idx3 = maxima[m3].Idx;
					FitLine(moments,idx2,idx3,&(params[2]),&(err[2]),&(mse[2]));
					if ( mse[2] > d_config.MaxLineMSE ) {
						// std::cerr << "    + line2 mse: " << mse[2] << std::endl;
						continue;
					}
					// dot = params[1].block<2,1>(2,0).dot(params[2].block<2,1>(2,0));
					// if ( std::abs(dot) > d_config.CosCriticalRadian ) {
					// 	std::cerr << "    + angle 1-2: " << dot << std::endl;
					// 	continue;
					// }


					FitLine(moments,idx3,idx0,&(params[3]),&(err[3]),&(mse[3]));
					if ( mse[3] > d_config.MaxLineMSE ) {
						// std::cerr << "    + line3 mse: " << mse[3] << std::endl;
						continue;
					}

					// dot = params[2].block<2,1>(2,0).dot(params[3].block<2,1>(2,0));
					// if ( std::abs(dot) > d_config.CosCriticalRadian ) {
					// 	// std::cerr << "    + angle 2-3: " << dot << std::endl;
					// 	continue;
					// }
					// dot = params[3].block<2,1>(2,0).dot(params[0].block<2,1>(2,0));
					// if ( std::abs(dot) > d_config.CosCriticalRadian ) {
					// 	// std::cerr << "    + angle 0-3: " << dot << std::endl;
					// 	continue;
					// }


					double sumErr = err[0] + err[1] + err[2] + err[3];
					if ( sumErr < bestError ) {
						bestError = sumErr;
						// std::cerr << " + points " << idx0 << " " << idx1 << " " << idx2 << " " << idx3 << std::endl;
						lines[0] = params[0];
						lines[1] = params[1];
						lines[2] = params[2];
						lines[3] = params[3];
					}
				}
			}
		}
	}

	if ( (bestError == std::numeric_limits<double>::max()) ||
	     (bestError / size >= d_config.MaxLineMSE) ) {
		lines.clear();
		return;
	}

}

void QuadFitter::FitLine(const CumulativeMoment & moments, size_t start, size_t end, Eigen::Vector4d * line, double * err, double * mse) {

	Moment m;
	size_t N;

	if ( start < end ) {
		N = end - start + 1;
		m = moments[end];
		if ( start > 0 ) {
			auto const & mstart = moments[start-1];
			m.Moment1 -= mstart.Moment1;
			m.Moment2 -= mstart.Moment2;
			m.Weight  -= mstart.Weight;
		}
	} else {
		N = moments.size() + end + 1 - start;
		auto const & mstart = moments[start-1];
		auto const & mend = moments[end];
		m = moments.back();
		m.Moment1 += mend.Moment1 - mstart.Moment1;
		m.Moment2 += mend.Moment2 - mstart.Moment2;
		m.Weight  += mend.Weight - mstart.Weight;
	}

	m.Moment1 /= m.Weight;
	m.Moment2 /= m.Weight;
	m.Moment2 -=  Eigen::Vector3d(m.Moment1.x()*m.Moment1.x(),
	                              m.Moment1.x()*m.Moment1.y(),
	                              m.Moment1.y()*m.Moment1.y());

	double diff = m.Moment2.x() - m.Moment2.z();
	double discr = sqrt(diff*diff + 4*m.Moment2.y()*m.Moment2.y());
	double eigen_small = 0.5*(m.Moment2.x() + m.Moment2.z() - discr );
	if (line != NULL ) {
		line->block<2,1>(0,0) = m.Moment1;
		double eigen_large = 0.5*(m.Moment2.x() + m.Moment2.z() + discr );
		Eigen::Vector2d n1(m.Moment2.x() - eigen_large,m.Moment2.y());
		Eigen::Vector2d n2(m.Moment2.y() ,m.Moment2.z() -eigen_large);
		double M1(n1.squaredNorm()), M2(n2.squaredNorm());
		if ( M2 > M1 ) {
			n1 = n2;
			M1 = M2;
		}
		line->block<2,1>(2,0) = n1 / std::sqrt(M1);
	}

	if ( err != NULL ) {
		*err = N * eigen_small;
	}

	if ( mse != NULL ) {
		*mse = eigen_small;
	}
}


void QuadFitter::ComputeCumulativeMoment(const cv::Mat & image, const GradientClusterizer::ListOfPoint & cluster, CumulativeMoment & moments) {
	moments.reserve(cluster.size());
	for(auto const & p : cluster ) {
		Eigen::Vector2d pos(p.Position.array() + 0.5);
		double Weight = 1.0;
		int ix(pos.x()), iy(pos.y());
		if (ix > 0 && ix+1 < image.cols && iy > 0 && iy + 1 < image.rows ) {
			int16_t grad_x = (int16_t)image.at<uint8_t>(iy,ix+1) - (int16_t)image.at<uint8_t>(iy,ix-1);
			int16_t grad_y = (int16_t)image.at<uint8_t>(iy+1,ix) - (int16_t)image.at<uint8_t>(iy-1,ix);
			Weight = sqrt(grad_x * grad_x + grad_y * grad_y) + 1.0;
		}

		if ( moments.empty() ) {
			Moment m;
			m.Moment1.setZero();
			m.Moment2.setZero();
			m.Weight = 0.0;
			moments.push_back(m);
		} else {
			moments.push_back(moments.back());
		}

		moments.back().Moment1 += Weight * pos;
		moments.back().Moment2 += Weight * Eigen::Vector3d(pos.x() * pos.x(), pos.x() * pos.y(), pos.y() * pos.y());
		moments.back().Weight  += Weight;
	}
}


void QuadFitter::FitQuads(const cv::Mat & image,
                          const Detector::QuadThresholdConfig & config,
                          size_t minTagWidth,
                          bool normalBorder,
                          bool reversedBorder,
                          GradientClusterizer::ClusterMap & clustermap) {
	d_config = config;
	d_config.MinimumPixelPerCluster = std::max((size_t)25, d_config.MinimumPixelPerCluster);
	d_config.CosCriticalRadian = std::cos(d_config.CriticalCornerAngleRadian);

	for(auto & kv : clustermap ) {
		// std::cerr << "Fitting a cluster" << std::endl;
		FitQuad(image,minTagWidth,normalBorder,reversedBorder,kv.second);
		// std::cerr << "Number of quads:" << Quads.size() << std::endl;
	}


}


void QuadFitter::PrintDebug(const cv::Size & size, cv::Mat & image) {
	ColorLabeller labeller;
	image = cv::Mat(size,CV_8UC3);
	image = CV_RGB(0,0,0);
	int i = -1;

	for(auto const & q : Quads ) {
		cv::Vec3b color = labeller.Color(++i);

		for (size_t i = 0; i < 4; ++i) {
			size_t j = (i+1) % 4;
			cv::Point start(q.Corners[i].x(),q.Corners[i].y());
			cv::Point end(q.Corners[j].x(),q.Corners[j].y());
			cv::line(image,start,end,color,2);
		}

	}


}
