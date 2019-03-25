#include "Apriltag2Process.h"

#include <apriltag/tag16h5.h>
#include <apriltag/tag25h7.h>
#include <apriltag/tag25h9.h>
#include <apriltag/tag36artoolkit.h>
#include <apriltag/tag36h10.h>
#include <apriltag/tag36h11.h>

#include <cmath>

Apriltag2Process::Apriltag2Process(const std::string & tagFamily)
	: d_family(OpenFamily(tagFamily))
	, d_detector(apriltag_detector_create(),apriltag_detector_destroy) {
	apriltag_detector_add_family(d_detector.get(),d_family.get());
	d_detector->nthreads = 1;
}

Apriltag2Process::~Apriltag2Process() {}


Apriltag2Process::FamilyPtr Apriltag2Process::OpenFamily(const std::string & name ) {
	struct FamilyInterface {
		typedef apriltag_family_t* (*Constructor) ();
		typedef void (*Destructor) (apriltag_family_t *);
		Constructor c;
		Destructor  d;
	};

	static std::map<std::string,FamilyInterface > familyFactory = {
		{"16h5",{.c = tag16h5_create, .d=tag16h5_destroy}},
		{"25h7",{.c =tag25h7_create, .d=tag25h7_destroy}},
		{"25h9",{.c =tag25h9_create, .d=tag25h9_destroy}},
		{"36artoolkit",{.c =tag36artoolkit_create, .d=tag36artoolkit_destroy}},
		{"36h10",{.c =tag36h10_create, .d=tag36h10_destroy}},
		{"36h11",{.c =tag36h11_create, .d=tag36h11_destroy}}
	};

	auto fi = familyFactory.find(name);
	if (fi == familyFactory.end() ) {
		throw std::runtime_error("Could not find tag family '"+ name +"'");
	}
	return FamilyPtr(fi->second.c(),fi->second.d);
}



cv::Rect computeROI(size_t i, size_t nbROI, const cv::Size & imageSize, size_t margin) {
	size_t bandWidth = imageSize.width/nbROI;
	size_t leftMargin = (i == 0 ) ? 0 : margin;
	size_t rightMargin = (i == (nbROI - 1) ) ? 0 : margin;
	cv::Point2d roiOrig ((i*bandWidth) - leftMargin ,0);
	cv::Size roiSize(bandWidth + leftMargin + rightMargin, imageSize.height);
	return cv::Rect(roiOrig,roiSize);
}


std::vector<ProcessFunction> Apriltag2Process::Prepare(size_t nbProcess, const cv::Size & size) {
	d_results.resize(nbProcess);
	d_offsets.resize(nbProcess);


	std::vector<ProcessFunction> toReturn;
	toReturn.reserve(nbProcess);
	for (size_t i = 0; i < nbProcess; ++i) {
		d_results[i] = NULL;
		size_t margin = 75;
		cv::Rect roi = computeROI(i,nbProcess,size,margin);
		d_offsets[i] = roi.x;

		toReturn.push_back([this,i,roi](const Frame::Ptr & frame,
		                                                       const fort::FrameReadout & readout,		                                                                            const cv::Mat & upstream) {
			                   cv::Mat withROI(frame->ToCV(),roi);
			                   image_u8_t img = {
				                   .width = withROI.cols,
				                   .height = withROI.rows,
				                   .stride = (int32_t) withROI.step[0],
				                   .buf = withROI.data
			                   };

			                   d_results[i] = apriltag_detector_detect(d_detector.get(),&img);

		                   });
	}
	return toReturn;
}

void Apriltag2Process::Finalize(const cv::Mat &, fort::FrameReadout & readout, cv::Mat & ) {
	std::map<uint32_t,apriltag_detection_t*> results;

	for(size_t i = 0; i < d_results.size(); ++i ) {
		for( int j = 0; j < zarray_size(d_results[i]); ++j ) {
			apriltag_detection_t * q;
			zarray_get(d_results[i],j,&q);
			if (results.count((uint32_t)q->id) != 0 ) {
				continue;
			}
			results[q->id] = q;
			q->c[0] += d_offsets[i];
		}
	}

	readout.clear_ants();
	for ( auto & kv : results ) {
		auto a = readout.add_ants();
		a->set_id(kv.second->id);
		a->set_x(kv.second->c[0]);
		a->set_y(kv.second->c[1]);
		//TODO: compute angle!!!
		a->set_theta(0.0);
	}

	for(size_t i = 0 ; i < d_results.size(); ++ i ) {
		apriltag_detections_destroy(d_results[i]);
	}

	d_results.clear();
	d_offsets.clear();
}
