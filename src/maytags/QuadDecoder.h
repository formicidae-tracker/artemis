#pragma once

#include "QuadFitter.h"
#include "QuickTagDecoder.h"


namespace maytags {

class QuadDecoder {
public:
	QuadDecoder(const Family::Ptr & family);

	bool Decode(const cv::Mat & image, const QuadFitter::Quad & quad, double decodeSharpening, Detection & detection);
	EIGEN_MAKE_ALIGNED_OPERATOR_NEW
private:
	const static size_t NbSamplers = 8;
	Eigen::Vector2d d_samplerStart[NbSamplers];
	Eigen::Vector2d d_samplerDelta[NbSamplers];
	bool            d_isWhite[NbSamplers];

	Family::Ptr     d_family;
	QuickTagDecoder d_decoder;
	cv::Mat         d_sampled;
	cv::Mat         d_kernel;
	cv::Mat         d_sharpened;
};



}
