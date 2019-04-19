#include "ComponentConnecterUTest.h"

#include "ComponentConnecter.h"
#include "GradientClusterizer.h"

#include "ImageResource.h"


TEST_F(ComponentConnecterUTest, Image) {
	auto threshold = LOAD_IMAGE(src_maytags_data_debug_threshold_pxm);
	auto expectedSegmentation = LOAD_IMAGE(src_maytags_data_debug_segmentation_pxm);
	auto expectedClusters = LOAD_IMAGE(src_maytags_data_debug_clusters_pxm);
	maytags::ComponentConnecter cc;

	cc.ConnectComponent(threshold.Image());


	cv::Mat debugSegmentation;
	cc.PrintDebug(threshold.Image().size(),debugSegmentation);

	expectedSegmentation.ExpectEqual(debugSegmentation);


	maytags::GradientClusterizer gc;
	gc.GradientCluster(threshold.Image(),cc,25);

	cv::Mat debugCluster;
	gc.PrintDebug(threshold.Image().size(),debugCluster);

	expectedClusters.ExpectEqual(debugCluster);
}
