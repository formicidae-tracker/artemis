#include "QuadFitter.h"

using namespace maytags;


bool comparePoint(const GradientClusterizer::Point & a, const GradientClusterizer::Point & b) {
	return a.Slope > b.Slope;
}

void QuadFitter::FitQuads(const cv::Mat & image,
                          size_t minTagWidth,
                          bool normalBorder,
                          bool reversedBorder,
                          size_t minClusterPixel,
                          GradientClusterizer::ClusterMap & clustermap) {

	for(auto & kv : clustermap ) {
		size_t clusterSize = kv.second.size();
		if ( clusterSize < minClusterPixel ) {
			continue;
		}


		if (clusterSize > 3*(2*image.cols+2*image.rows)) {
			continue;
		}

		double xMin(std::numeric_limits<double>::max());
		double xMax(std::numeric_limits<double>::min());
		double yMin(std::numeric_limits<double>::max());
		double yMax(std::numeric_limits<double>::min());

		for (auto const & p : kv.second ) {
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
			continue;
		}
		Eigen::Vector2d center((xMax + xMin) * 0.5 + 0.5118,
		                       (yMax + yMin) * 0.5 - 0.028581);

		//seems a magical unexplained formula :/
		const static float quadrants[2][2] = {{-1*(2 << 15), 0}, {2*(2 << 15), 2 << 15}};
		double dot = 0;
		for(auto  & p : kv.second ) {
			Eigen::Vector2d delta = p.Position - center;
			dot += delta.dot(p.Gradient);

			float quadrant = quadrants[(delta.y() > 0) ? 1 : 0][ (delta.x() > 0) ? 1 : 0];
			if ( delta.y() < 0 ) {
				delta *= -1;
			}

			if ( delta.x() < 0 ) {
				double tmp = delta.x();
				delta.x() = delta.y();
				delta.y() = -tmp;
			}
			p.Slope = quadrant + delta.y() / delta.x();
		}

		Quad quad;
		quad.ReversedBorder = dot < 0.0;
		if (reversedBorder == false && quad.ReversedBorder == true) {
			continue;
		}

		if (normalBorder == false && quad.ReversedBorder == false) {
			continue;
		}

		std::sort(kv.second.begin(),kv.second.end(),&comparePoint);
		//removes duplicate.

	}


}
