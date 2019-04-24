#pragma once

#include "Detector.h"

#include "Thresholder.h"
#include "ComponentConnecter.h"
#include "GradientClusterizer.h"
#include "QuadFitter.h"
#include "QuadDecoder.h"


namespace maytags {
class Implementation {
public :
	Implementation(const Detector::Config & config);
	~Implementation();

	void Detect(const cv::Mat & grayscale, Detector::ListOfDetection & detections);

private :
	Detector::Config d_config;

	Thresholder              d_thresholder;
	ComponentConnecter       d_connecter;
	GradientClusterizer      d_clusterizer;
	size_t                   d_minTagWidth;
	bool                     d_reversedBorder;
	bool                     d_normalBorder;
	QuadFitter               d_fitter;
	std::vector<QuadDecoder> d_decoders;

};


}
