#include "ComponentConnecterUTest.h"

#include "Thresholder.h"
#include "ComponentConnecter.h"
#include "GradientClusterizer.h"
#include "QuadFitter.h"
#include "ImageResource.h"

using namespace maytags;

void ExpectEqual(const Eigen::Vector2d & a, const Eigen::Vector2d & b) {
	if ( (std::abs(a.x()-b.x()) > 0.1) || (std::abs(a.y()-b.y()) > 0.1) ) {
		ADD_FAILURE() << a.transpose() << " =!= " << b.transpose();
	}
}

TEST_F(ComponentConnecterUTest, Image) {
	auto input = LOAD_IMAGE(src_maytags_data_debug_preprocess_pxm);
	auto threshold = LOAD_IMAGE(src_maytags_data_debug_threshold_pxm);
	auto expectedSegmentation = LOAD_IMAGE(src_maytags_data_debug_segmentation_pxm);
	auto expectedClusters = LOAD_IMAGE(src_maytags_data_debug_clusters_pxm);

	Detector::QuadThresholdConfig config;
	config.MinimumPixelPerCluster = 5;
	config.MaximumNumberOfMaxima = 10;
	config.CriticalCornerAngleRadian = 10 * M_PI / 180.0;
	config.MaxLineMSE = 10.0;
	config.MinimumBWDifference = 30;
	Thresholder th;
	th.Threshold(input.Image(),config.MinimumBWDifference);
	ImageResource::ExpectEqual(threshold.Image(),th.Thresholded);


	ComponentConnecter cc;

	cc.ConnectComponent(threshold.Image());

	cv::Mat debugSegmentation;
	cc.PrintDebug(threshold.Image().size(),debugSegmentation);

	ImageResource::ExpectEqual(expectedSegmentation.Image(),debugSegmentation);

	GradientClusterizer gc;
	gc.GradientCluster(threshold.Image(),cc,25);

	cv::Mat debugCluster;
	gc.PrintDebug(threshold.Image().size(),debugCluster);

	ImageResource::ExpectEqual(expectedClusters.Image(),debugCluster);



	QuadFitter expected;
	expected.Quads.resize(2);
	expected.Quads[1].Corners[0] = Eigen::Vector2d(98.023689270019531,79.592376708984375);
	expected.Quads[1].Corners[1] = Eigen::Vector2d(104.148109436035156,85.645683288574219);
	expected.Quads[1].Corners[2] = Eigen::Vector2d(92.280052185058594,90.468368530273438);
	expected.Quads[1].Corners[3] = Eigen::Vector2d(89.000450134277344,77.864562988281250);
	expected.Quads[0].Corners[0] = Eigen::Vector2d(129.867385864257812,81.201515197753906);
	expected.Quads[0].Corners[1] = Eigen::Vector2d(104.276916503906250,119.425476074218750);
	expected.Quads[0].Corners[2] = Eigen::Vector2d(65.589057922363281,93.655731201171875);
	expected.Quads[0].Corners[3] = Eigen::Vector2d(91.968605041503906,56.273712158203125);

	QuadFitter qf;


	qf.FitQuads(input.Image(), config, 10, true, false, gc.Clusters);

	EXPECT_EQ(expected.Quads.size(),qf.Quads.size());
	for(size_t i = 0; i < std::min(expected.Quads.size(),qf.Quads.size()); ++i) {
		ExpectEqual(expected.Quads[i].Corners[0],qf.Quads[i].Corners[0]);
		ExpectEqual(expected.Quads[i].Corners[1],qf.Quads[i].Corners[1]);
		ExpectEqual(expected.Quads[i].Corners[2],qf.Quads[i].Corners[2]);
		ExpectEqual(expected.Quads[i].Corners[3],qf.Quads[i].Corners[3]);

		qf.Quads[i].ComputeHomography();
		for(size_t c =0; c < 4; ++c) {
			Eigen::Vector2d corner;
			qf.Quads[i].Project(QuadFitter::Quad::NormalizedCorners[c],corner);
			ExpectEqual(qf.Quads[i].Corners[c],corner);
		}
	}

	cv::Mat debugQuadFit;
	cv::Mat expectedQuadFit;
	qf.PrintDebug(input.Image().size(),debugQuadFit);
	expected.PrintDebug(input.Image().size(),expectedQuadFit);

	ImageResource::ExpectEqual(expectedQuadFit,debugQuadFit);





}
