#include "ComponentConnecterUTest.h"

#include "ComponentConnecter.h"

#include "ImageResource.h"


TEST_F(ComponentConnecterUTest, Image) {
	auto threshold = LOAD_IMAGE(src_maytags_data_debug_threshold_pxm);
	auto expected = LOAD_IMAGE(src_maytags_data_debug_segmentation_pxm);
	maytags::ComponentConnecter cc;

	cc.ConnectComponent(threshold.Image());


	cv::Mat debug;
	cc.PrintDebug(threshold.Image().size(),debug);

	expected.ExpectEqual(debug);
}
