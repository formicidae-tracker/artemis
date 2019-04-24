#pragma once

#include "ProcessDefinition.h"

#include <maytags/Detector.h>

#include <mutex>

#include "CommonTypes.h"

#include "Connection.h"


class AprilTag2Detector {
public:
	typedef std::shared_ptr<AprilTag2Detector> Ptr;

	struct Config {
		Config()
			: Family("36h11")
			, QuadDecimate(1.0)
			, QuadSigma(0.0)
			, RefineEdges(false)
			, QuadMinClusterPixel(5)
			, QuadMaxNMaxima(10)
			, QuadCriticalRadian(0.174533)
			, QuadMaxLineMSE(10.0)
			, QuadMinBWDiff(5) {
		}


		std::string Family;
		float       QuadDecimate;
		float       QuadSigma;
		bool        RefineEdges;
		bool        NoRefineDecode;
		bool        RefinePose;
		int         QuadMinClusterPixel;
		int         QuadMaxNMaxima;
		float       QuadCriticalRadian;
		float       QuadMaxLineMSE;
		int         QuadMinBWDiff;
	};


	// Detects AprilTags in multiple different RegionOfInterests
	class ROITagDetection : public ProcessDefinition {
	public:
		virtual ~ROITagDetection();

		virtual std::vector<ProcessFunction> Prepare(size_t maxProcess, const cv::Size &);
	private :

		ROITagDetection(const AprilTag2Detector::Ptr & parent);

		ROITagDetection(const ROITagDetection & ) = delete;
		ROITagDetection & operator=(const ROITagDetection &) = delete;

		AprilTag2Detector::Ptr d_parent;
		friend class AprilTag2Detector;
	};

	// Merges in a single thread all detection and eliminate multiple
	// match in overlapping areas)
	class TagMerging : public ProcessDefinition {
	public:
		virtual ~TagMerging();

		virtual std::vector<ProcessFunction> Prepare(size_t maxProcess, const cv::Size &);
	private:
		TagMerging(const AprilTag2Detector::Ptr & parent,
		           const Connection::Ptr & connection);

		TagMerging(const TagMerging & ) = delete;
		TagMerging & operator=(const TagMerging &) = delete;


		AprilTag2Detector::Ptr d_parent;
		Connection::Ptr        d_connection;
		friend class AprilTag2Detector;
	};


	static ProcessQueue Create(size_t maxWorker,
	                           const Config & config,
	                           const Connection::Ptr & address);

	~AprilTag2Detector();


private:
	friend class ROITagDetection;
	friend class TagMerging;
	friend class Finalization;

	AprilTag2Detector(size_t maxWorker,
	                  const Config & config);


	std::vector<std::shared_ptr<maytags::Detector> > d_detectors;


	struct Detection {
		int32_t ID;
		double X,Y,Theta;
	};

	std::vector<std::vector<Detection> > d_results;
};
