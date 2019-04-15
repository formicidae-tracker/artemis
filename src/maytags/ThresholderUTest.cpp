#include "ThresholderUTest.h"

#include "Thresholder.h"

#include "ImageResource.h"

void ThresholderUTest::SetUp() {

}

void ThresholderUTest::TearDown() {

}


TEST_F(ThresholderUTest,Image) {
	auto input = LOAD_IMAGE(src_maytags_data_debug_preprocess_pxm);
	auto result = LOAD_IMAGE(src_maytags_data_debug_threshold_pxm);



	maytags::Thresholder th;
	th.Threshold(input.Image(),30);


	result.ExpectEqual(th.Thresholded);


}
