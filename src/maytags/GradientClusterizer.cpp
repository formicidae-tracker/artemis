#include "GradientClusterizer.h"

#include <Eigen/Core>
#include <map>
#include "ColorLabeller.h"


using namespace maytags;

void GradientClusterizer::GradientCluster(const cv::Mat & binaryImage, ComponentConnecter & cc, size_t minClusterSize) {
	Clusters.clear();

	for ( int y = 0; y < binaryImage.rows-1; ++y ) {
		for (int x = 1; x < binaryImage.cols-1; ++x) {
			uint8_t v0 = binaryImage.at<uint8_t>(y,x);
			if (v0 == 127 ) {
				continue;
			}

			uint64_t rep0 = cc.GetRepresentative(REPRESENTATIVE_ID(x,y,binaryImage.cols));
			if ( cc.GetSize(rep0) < minClusterSize ) {
				continue;
			}

#define do_connect(dx,dy) do {	  \
				uint8_t v1 = binaryImage.at<uint8_t>(y+dy,x+dx); \
				if ( (v0+v1) != 255 ) { \
					break; \
				} \
				uint64_t rep1 = cc.GetRepresentative(REPRESENTATIVE_ID(x+dx,y+dy,binaryImage.cols)); \
				if ( cc.GetSize(rep1) < minClusterSize ) { \
					break; \
				} \
				uint64_t clusterID; \
				if (rep0 < rep1 ) { \
					clusterID = (rep1 << 32) + rep0; \
				} else { \
					clusterID = (rep0 << 32) + rep1; \
				} \
				if(Clusters.count(clusterID) == 0 ) { \
					Clusters[clusterID] = ListOfPoint(); \
				}\
				Clusters[clusterID].push_back(Point{ \
						.Position = Eigen::Vector2d(x+(double)dx/2.0,y+(double)dy/2.0), \
							.Gradient= Eigen::Vector2d(dx*(v1-v0),dy*(v1-v0)), \
							.Slope = 0.0, \
							}); \
			}while(0)

			do_connect(1,0);
			do_connect(0,1);

			do_connect(-1,1);
			do_connect(1,1);

#undef do_connect

		}
	}



}


void GradientClusterizer::PrintDebug(const cv::Size & size,cv::Mat & image) {
	ColorLabeller labeller;
	image = cv::Mat(size,CV_8UC3);

	for(auto const & kv : Clusters ) {
		cv::Vec3b color  = labeller.Color(kv.first);
		for (auto const & p : kv.second ) {
			image.at<cv::Vec3b>(p.Position.y(),p.Position.x()) = color;
		}
	}
}
