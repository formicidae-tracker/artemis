#include "Thresholder.h"



using namespace maytags;



void Thresholder::Threshold(const cv::Mat & image, uint8_t minBWDiff) {
	size_t tileWidth = image.cols / TileSize;
	size_t tileHeight = image.rows / TileSize;


	if ( image.size() != Thresholded.size() ) {
		Thresholded = cv::Mat(image.size(),CV_8U);
		d_min = cv::Mat(tileHeight,tileWidth,CV_8U);
		d_max = cv::Mat(tileHeight,tileWidth,CV_8U);
		d_minTmp = cv::Mat(tileHeight,tileWidth,CV_8U);
		d_maxTmp = cv::Mat(tileHeight,tileWidth,CV_8U);
	}


	for ( size_t ty = 0; ty < tileHeight; ++ty ) {
		for ( size_t tx = 0; tx < tileWidth; ++tx ) {
			uint8_t max(0), min(255);
			for( size_t dy = 0; dy < TileSize; ++dy ) {
				for( size_t dx = 0; dx < TileSize; ++dx ) {
					uint8_t v = image.at<uint8_t>(ty+dy,tx+dx);
					if ( v < min ) {
						min = v;
					}
					if ( v >  max ) {
						max = v;
					}
				}
			}
			d_minTmp.at<uint8_t>(ty,tx) = min;
			d_maxTmp.at<uint8_t>(ty,tx) = max;
		}
	}


	for ( int ty = 0; ty < tileHeight; ++ty ) {
		for ( int tx = 0; tx < tileWidth; ++tx ) {
			uint8_t max(0), min(255);
			for ( int dy = -1; dy < 2; ++dy ) {
				int iy(dy + ty);
				if ( iy < 0 || iy > tileHeight ) {
					continue;
				}
				for ( int dx = -1; dx < 2; ++dx ) {
					int ix(dx + tx);
					if (ix < 0 || ix > tileWidth ) {
						continue;
					}
					uint8_t v = d_maxTmp.at<uint8_t>(iy,ix);
					if ( v > max ) {
						max = v;
					}
					v = d_minTmp.at<uint8_t>(iy,ix);
					if (v < min ) {
						min =v;
					}

					d_min.at<uint8_t>(iy,ix) = min;
					d_max.at<uint8_t>(iy,ix) = max;

				}
			}
		}
	}

	for ( size_t ty = 0; ty < tileHeight; ++ty ) {
		for ( size_t tx = 0; tx < tileWidth; ++tx ) {
			uint8_t max(d_max.at<uint8_t>(ty,tx)), min(d_min.at<uint8_t>(ty,tx));
			bool lowContrast = (max-min) < minBWDiff;
			uint8_t thresh = min + (max - min) / 2;
			for( size_t dy = 0; dy < TileSize; ++dy ) {
				for( size_t dx = 0; dx < TileSize; ++dx ) {

					if ( lowContrast == true ) {
						Thresholded.at<uint8_t>(ty+dy,tx+dx) = 127;
					} else {
						if (image.at<uint8_t>(ty+dy,tx+dx) > thresh ) {
							Thresholded.at<uint8_t>(ty+dy,tx+dx) = 255;
						} else {
							Thresholded.at<uint8_t>(ty+dy,tx+dx) = 0;
						}
					}
				}
			}
		}
	}

	for ( size_t y = 0; y < image.rows; ++y ) {
		size_t xStart(0) ;
		if ( y < tileHeight * TileSize ) {
			xStart = tileWidth * TileSize;
		}

		size_t ty = y / TileSize;
		if (ty >= tileHeight ) {
			ty =  tileHeight - 1;
		}

		for (size_t x = xStart; x < image.cols; ++x ) {
			size_t tx = x / TileSize;
			if ( tx >= tileWidth ) {
				tx = tileWidth -1;
			}

			uint8_t max(d_max.at<uint8_t>(ty,tx)), min(d_min.at<uint8_t>(ty,tx));
			bool lowContrast = (max-min) < minBWDiff;
			uint8_t thresh = min + (max - min) / 2;
			if ( lowContrast == true ) {
				Thresholded.at<uint8_t>(y,x) = 127;
			} else {
				if (image.at<uint8_t>(y,x) > thresh ) {
					Thresholded.at<uint8_t>(y,x) = 255;
				} else {
					Thresholded.at<uint8_t>(y,x) = 0;
				}
			}
		}
	}


}
