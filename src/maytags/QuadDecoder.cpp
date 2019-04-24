#include "QuadDecoder.h"

#include "Graymodel.h"

#include <Eigen/Geometry>

#include <opencv2/imgproc.hpp>

using namespace maytags;

const Family::Ptr & QuadDecoder::FamilyPtr() {
	return d_family;
}

QuadDecoder::QuadDecoder(const Family::Ptr & family)
	: d_family(family)
	, d_decoder(family,2) {

	d_samplerStart[0] = Eigen::Vector2d(-0.5,0.5);
	d_samplerStart[1] = Eigen::Vector2d(0.5,0.5);
	d_samplerStart[2] = Eigen::Vector2d(family->WidthAtBorder+0.5,0.5);
	d_samplerStart[3] = Eigen::Vector2d(family->WidthAtBorder-0.5,0.5);
	d_samplerStart[4] = Eigen::Vector2d(0.5,-0.5);
	d_samplerStart[5] = Eigen::Vector2d(0.5,0.5);
	d_samplerStart[6] = Eigen::Vector2d(0.5,family->WidthAtBorder+0.5);
	d_samplerStart[7] = Eigen::Vector2d(0.5,family->WidthAtBorder-0.5);


	d_samplerDelta[0] = Eigen::Vector2d(0,1);
	d_samplerDelta[1] = Eigen::Vector2d(0,1);
	d_samplerDelta[2] = Eigen::Vector2d(0,1);
	d_samplerDelta[3] = Eigen::Vector2d(0,1);
	d_samplerDelta[4] = Eigen::Vector2d(1,0);
	d_samplerDelta[5] = Eigen::Vector2d(1,0);
	d_samplerDelta[6] = Eigen::Vector2d(1,0);
	d_samplerDelta[7] = Eigen::Vector2d(1,0);


	d_isWhite[0] = true;
	d_isWhite[1] = false;
	d_isWhite[2] = true;
	d_isWhite[3] = false;
	d_isWhite[4] = true;
	d_isWhite[5] = false;
	d_isWhite[6] = true;
	d_isWhite[7] = false;


	d_sampled = cv::Mat(d_family->TotalWidth,d_family->TotalWidth,CV_64FC1);
	d_sharpened = cv::Mat(d_family->TotalWidth,d_family->TotalWidth,CV_64FC1);
	d_kernel = cv::Mat(3,3,CV_64F);
	d_kernel.at<double>(0,0) =  0.0; 	d_kernel.at<double>(0,1) = -1.0; d_kernel.at<double>(0,2) =  0.0;
	d_kernel.at<double>(1,0) = -1.0; 	d_kernel.at<double>(1,1) =  4.0; d_kernel.at<double>(1,2) = -1.0;
	d_kernel.at<double>(2,0) =  0.0; 	d_kernel.at<double>(2,1) = -1.0; d_kernel.at<double>(2,2) =  0.0;
	d_sampled = d_sampled + 0.25 * d_sharpened;
	//call it once to avoid a big impact on first execution
	//cv::filter2D(d_sampled,d_sharpened,-1,d_kernel,cv::Point(-1,-1),0,cv::BORDER_CONSTANT);
}


double ValueAt(const cv::Mat & image,const Eigen::Vector2d & pos) {
	int x1 = floor(pos.x() - 0.5);
	int x2 = ceil(pos.x() - 0.5);
	double x = pos.x() - 0.5 - x1;
	int y1 = floor(pos.y() - 0.5);
	int y2 = ceil(pos.y() - 0.5);
	double y = pos.y() - 0.5 - y1;
	if (x1 < 0 || x2 >= image.cols || y1 < 0 || y2 >= image.rows) {
        return -1.0;
    }
	return image.at<uint8_t>(y1,x1)*(1-x)*(1-y) +
		image.at<uint8_t>(y1,x2)*x*(1-y) +
		image.at<uint8_t>(y2,x1)*(1-x)*y +
		image.at<uint8_t>(y2,x2)*x*y;
}


void Filter2d(const cv::Mat & src, cv::Mat & dst, const cv::Mat & kernel) {
	int xOffset = (kernel.cols - 1)/2;
	int yOffset = (kernel.rows - 1)/2;
	for ( int y = 0; y < src.rows; ++y ) {
		for ( int x = 0; x < src.cols; ++x) {
			dst.at<double>(y,x) = 0.0;
			for ( int ky = 0 ; ky < kernel.rows; ++ky ) {
				for ( int kx = 0; kx < kernel.cols; ++kx ) {
					int ix = x + kx - xOffset ;
					int iy = y + ky - yOffset ;

					if ( ix < 0 || ix >= src.cols || iy < 0 || iy >= src.rows ) {
						continue;
					}
					dst.at<double>(y,x) += src.at<double>(iy,ix) * kernel.at<double>(ky,kx);
				}
			}
		}
	}
}

bool QuadDecoder::Decode(const cv::Mat & image, const QuadFitter::Quad & quad, double decodeSharpening, Detection & detection) {
	Graymodel white,black;
	//if (profiler != NULL) { profiler->Add("decode/init");}
	for (size_t i = 0; i< NbSamplers; ++i ) {

		for (size_t j = 0; j < d_family->WidthAtBorder; ++j ) {
			Eigen::Vector2d tagPos = (d_samplerStart[i] + j * d_samplerDelta[i]) / d_family->WidthAtBorder;
			tagPos.array() -= 0.5;
			tagPos *= 2.0;

			Eigen::Vector2d pos;
			quad.Project(tagPos,pos);

			int ix = pos.x();
			int iy = pos.y();
			if (ix < 0 || ix >= image.cols || iy < 0 || iy >= image.rows ) {
				continue;
			}
			uint8_t v = image.at<uint8_t>(iy,ix);


			if (d_isWhite[i]) {
				white.Add(tagPos,v);
			} else {
				black.Add(tagPos,v);
			}

		}

	}
	//if (profiler != NULL) { profiler->Add("decode/model/sample");}
	white.Solve();
	black.Solve();
	//if (profiler != NULL) { profiler->Add("decode/model/solve");}

	if ( ( (white.Interpolate(Eigen::Vector2d::Zero()) - black.Interpolate(Eigen::Vector2d::Zero())) < 0 ) != d_family->ReversedBorder ) {
		return false;
	}

	double blackScore(0),whiteScore(0),blackScoreCount(1.0),whiteScoreCount(1.0);
	d_sampled.setTo(0.0);

	int minCoord = (d_family->WidthAtBorder - d_family->TotalWidth) / 2;

	for (size_t i = 0; i < d_family->NumberOfBits; ++i) {
		auto const & location = d_family->BitLocation[i];
		Eigen::Vector2d pos(location.x()+0.5,location.y()+0.5);
		pos /= d_family->WidthAtBorder;
		pos.array() -= 0.5;
		pos *= 2.0;
		Eigen::Vector2d posImage;
		quad.Project(pos,posImage);
		double v = ValueAt(image,posImage);
		if (v == -1) {
			continue;
		}
		double thresh = (white.Interpolate(pos) + black.Interpolate(pos)) * 0.5;
		d_sampled.at<double>(location.y()-minCoord,location.x()-minCoord) = v - thresh;
	}

	//if (profiler != NULL) { profiler->Add("decode/sample");}
	//cv::filter2d is doing weird things, reimplement it here
	//cv::filter2D(d_sampled,d_sharpened,-1,d_kernel,cv::Point(-1,-1),0,cv::BORDER_CONSTANT);
	Filter2d(d_sampled,d_sharpened,d_kernel);

	//if (profiler != NULL) { profiler->Add("decode/sharpen/filter2d");}

	d_sampled = d_sampled + decodeSharpening * d_sharpened;

	//if (profiler != NULL) { profiler->Add("decode/sharpen");}

	Code rcode = 0;
	for (size_t i =0; i < d_family->NumberOfBits; ++i ) {
		auto const & location = d_family->BitLocation[i];
		rcode = (rcode << 1);
		double v = d_sampled.at<double>(location.y()-minCoord,location.x()-minCoord);
		if (v > 0) {
			whiteScore += v;
			whiteScoreCount++;
			rcode |= 1;
		} else {
			blackScore -= v;
			blackScoreCount++;
		}
	}
	//if (profiler != NULL) { profiler->Add("decode/decode/sample");}

	QuickTagDecoder::Entry entry(0,0,0);
	if ( d_decoder.Decode(rcode,entry) == false ) {
		return false;
	}

	//if (profiler != NULL) { profiler->Add("decode/decode");}

	detection.ID = entry.ID;
	detection.Hamming = entry.Hamming;
	detection.DecisionMargin = std::min(whiteScore/whiteScoreCount,blackScore/blackScoreCount);

	Eigen::Transform<double,2,Eigen::Affine> rot(Eigen::Rotation2D<double>(entry.Rotation * M_PI / 2.0));
	detection.H = quad.H * rot.matrix();
	Eigen::Vector3d center = detection.H * Eigen::Vector3d::Zero();
	center /= center.z();
	detection.Center = center.block<2,1>(0,0);
	for(size_t i =0; i < 4; ++i ) {
		Eigen::Vector3d c;
		c.block<2,1>(0,0) = QuadFitter::Quad::NormalizedCorners[i];
		c.z() = 1.0;
		c = detection.H * c;
		c /= c.z();
		detection.Corners[i] = c.block<2,1>(0,0);
	}
	// if (profiler != NULL) { profiler->Add("decode/cleanup");}
	return true;
}
