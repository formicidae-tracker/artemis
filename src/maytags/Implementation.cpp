#include "Implementation.h"

#include <map>


using namespace maytags;

Implementation::Implementation(const Detector::Config & config)
	: d_config(config) {
	if ( d_config.Families.empty() ) {
		throw std::invalid_argument("maytags::Detector::Config need at least one family");
	}
	d_minTagWidth = std::numeric_limits<size_t>::max();
	d_reversedBorder = false;
	d_normalBorder = false;
	for(auto const & f : config.Families ) {
		if ( f->WidthAtBorder < d_minTagWidth ) {
			d_minTagWidth = f->WidthAtBorder;
		}
		if ( f->ReversedBorder == true ) {
			d_reversedBorder = true;
		} else {
			d_normalBorder = true;
		}
		d_decoders.push_back(QuadDecoder(f));
	}
}

Implementation::~Implementation() {
}

void Implementation::Detect(const cv::Mat & grayscale, Detector::ListOfDetection & detections) {
	if (grayscale.channels() != CV_8U ) {
		throw std::invalid_argument("Only working with 8 bit monochrome images");
	}

	detections.clear();

	d_thresholder.Threshold(grayscale, d_config.QuadConfig.MinimumBWDifference);
	d_connecter.ConnectComponent(d_thresholder.Thresholded);
	d_clusterizer.GradientCluster(d_thresholder.Thresholded,d_connecter,d_config.QuadConfig.MinimumPixelPerCluster);

	d_fitter.FitQuads(grayscale,
	                  d_config.QuadConfig,
	                  d_minTagWidth,
	                  d_normalBorder,
	                  d_reversedBorder,
	                  d_clusterizer.Clusters);


	std::map<uint16_t,Detection> allDetections;

	for ( auto  & q : d_fitter.Quads ) {
		q.ComputeHomography();
		Detection d;

		for ( auto & decoder : d_decoders ) {
			if ( decoder.Decode(grayscale,q,d_config.DecodeSharpening,d) == false ) {
				continue;
			}

			auto search = allDetections.find(d.ID);
			if ( search == allDetections.end() ) {
				allDetections.emplace(std::make_pair(d.ID,d));
				break;
			}

			if ( search->second.Hamming > d.Hamming ) {
				search->second = d;
				break;
			} else if ( search->second.Hamming < d.Hamming ) {
				break;
			}

			if ( search->second.DecisionMargin < d.DecisionMargin ) {
				search->second = d;
				break;
			}

			break;

		}

	}

	detections.reserve(allDetections.size());
	for ( auto const & d : allDetections ) {
		detections.push_back(d.second);
	}

}
