#pragma once

#include "ProcessDefinition.h"

#include <apriltag/apriltag.h>

#include <mutex>

#include "CommonTypes.h"

#include <asio/io_service.hpp>
#include <asio/strand.hpp>
#include <asio/streambuf.hpp>


class AprilTag2Detector {
public:
	typedef std::shared_ptr<AprilTag2Detector> Ptr;

	struct Config {
		Config()
			: Family("36h11")
			, NewAntROISize(1000)
			, QuadDecimate(1.0)
			, QuadSigma(0.0)
			, RefineEdges(false)
			, NoRefineDecode(false)
			, RefinePose(false)
			, QuadMinClusterPixel(5)
			, QuadMaxNMaxima(10)
			, QuadCriticalRadian(0.174533)
			, QuadMaxLineMSE(10.0)
			, QuadMinBWDiff(5)
			, QuadDeglitch(false) {
		}


		std::string Family;
		size_t      NewAntROISize;
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
		bool        QuadDeglitch;
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
		TagMerging(const AprilTag2Detector::Ptr & parent);

		TagMerging(const TagMerging & ) = delete;
		TagMerging & operator=(const TagMerging &) = delete;


		AprilTag2Detector::Ptr d_parent;
		friend class AprilTag2Detector;
	};

	// Finalizes the detection in multiple threads
	// * Serialize the message in a buffer
	// * Extract New detected ID, copy the new image around the ant,
	//   and sends them to a new AntBuffer
	class Finalization : public ProcessDefinition {
	public :
		virtual ~Finalization();

		virtual std::vector<ProcessFunction> Prepare(size_t maxProcess, const cv::Size &);
	private :
		Finalization(const AprilTag2Detector::Ptr & parent,
		             asio::io_service & service,
		             const std::string & address,
		             const std::string & savepath,
		             size_t newAntROISize);

		Finalization(const Finalization & ) = delete;
		Finalization & operator=(const Finalization &) = delete;


		void SerializeMessage(const fort::FrameReadout & message);
		void CheckForNewAnts( const Frame::Ptr & ptr,
		                      const fort::FrameReadout & readout,
		                      size_t start=0,
		                      size_t stride=1);

		void ScheduleReconnect();


		AprilTag2Detector::Ptr d_parent;
		friend class AprilTag2Detector;



		asio::io_service & d_service;

		std::string                            d_address;
		std::shared_ptr<asio::ip::tcp::socket> d_socket;
		asio::strand                           d_strand;

		typedef RingBuffer<asio::streambuf,16> BufferPool;
		BufferPool::Consumer::Ptr d_consumer;
		BufferPool::Producer::Ptr d_producer;
		std::mutex                d_mutex;
		std::string               d_savePath;
		std::set<int32_t>         d_known;
		const size_t              d_newAntROISize;

	};


	static ProcessQueue Create(const Config & config,
	                           asio::io_service & service,
	                           const std::string & address,
	                           const std::string & savePath);

	~AprilTag2Detector();


private:
	friend class ROITagDetection;
	friend class TagMerging;
	friend class Finalization;

	AprilTag2Detector(const Config & config);

	typedef std::unique_ptr<apriltag_family_t, std::function <void (apriltag_family_t *) > > FamilyPtr;
	static FamilyPtr OpenFamily(const std::string & name);
	FamilyPtr d_family;
	std::unique_ptr<apriltag_detector_t,std::function<void (apriltag_detector_t*)> > d_detector;

	std::vector<zarray_t*> d_results;
	std::vector<size_t> d_offsets;
};
