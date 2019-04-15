#include "ComponentConnecter.h"
#include <map>

#include "ColorLabeller.h"

using namespace maytags;

void ComponentConnecter::ConnectComponent(const cv::Mat & image) {
	size_t imageSize = image.cols * image.rows;
	if ( d_parent.size() != imageSize ) {
		d_parent.resize(imageSize);
	}

	d_size.assign(imageSize,0);


	//initializes the first line
	{
		uint8_t vPrev = image.at<uint8_t>(0,0);
		for(size_t x = 1; x < image.cols; ++x) {
			uint8_t v = image.at<uint8_t>(0,x);
			if ( v == 127 ) {
				vPrev = v;
				continue;
			}
			if ( v == vPrev ) {
				Connect(x,x-1);
			}
			vPrev = v;
		}

	}

#define ID(x,y) REPRESENTATIVE_ID(x,y,image.cols)

	for ( size_t y = 1; y < image.rows; ++y ) {
		uint8_t v_m1_m1;
		uint8_t v_0_m1(image.at<uint8_t>(y-1,0));
		uint8_t v_1_m1(image.at<uint8_t>(y-1,1));
		uint8_t v_m1_0;
		uint8_t v(image.at<uint8_t>(y,0));

		for ( size_t x = 1; x < image.cols; ++x ) {
			v_m1_m1 = v_0_m1;
			v_0_m1 = v_1_m1;
			v_1_m1 = image.at<uint8_t>(y-1,x+1);
			v_m1_0 = v;
			v = image.at<uint8_t>(y,x);

			if ( v == 127 ) {
				continue;
			}

			if ( v_m1_0 == v ) {
				Connect(ID(x,y),ID(x-1,y));
			}

			if ( x == 1 || !((v_m1_0 == v_m1_m1 ) && (v_m1_m1 == v_0_m1))) {
				if( v_0_m1 == v ) {
					Connect(ID(x,y),ID(x,y-1));
				}
			}


			if ( v == 0 ) {
				continue;
			}

			if ( x == 1 || !( (v_m1_0 == v_m1_m1) || (v_0_m1 == v_m1_m1)) ) {
				if (v_m1_m1 == v ) {
					Connect(ID(x,y),ID(x-1,y-1));
				}
			}

			if ( (v_0_m1 != v_1_m1) && (v_1_m1 == v) ) {
				Connect(ID(x,y),ID(x+1,y-1));
			}
		}
	}

#undef ID

}

uint32_t ComponentConnecter::GetRepresentative(uint32_t ID) {
#ifndef NDEBUG
	if ( ID >= d_parent.size() ) {
		throw std::out_of_range("index out of range");
	}
#endif

	if (d_size[ID] == 0 ) {
		d_size[ID] = 1;
		d_parent[ID] = ID;
		return ID;
	}

	uint32_t root = ID;
	while ( d_parent[root] != root ) {
#ifndef NDEBUG
		if (root >= d_parent.size() ) {
			throw std::out_of_range("tree walking: index out of range");
		}
#endif
		root = d_parent[root];
	}

	while ( d_parent[ID] != root ) {
		uint32_t tmp = d_parent[ID];
		d_parent[ID] = root;
		ID = tmp;
	}

	return root;
}


uint32_t ComponentConnecter::Connect(uint32_t a, uint32_t b) {
	uint32_t aRoot = GetRepresentative(a);
	uint32_t bRoot = GetRepresentative(b);

	if ( aRoot == bRoot ) {
		return aRoot;
	}
	uint32_t aSize = d_size[aRoot];
	uint32_t bSize = d_size[bRoot];

	if (aSize > bSize ) {
		d_parent[bRoot] = aRoot;
		d_size[aRoot] += bSize;
		return aRoot;
	}
	d_parent[aRoot] = bRoot;
	d_size[bRoot] += aSize;

	return bRoot;

}

uint32_t ComponentConnecter::GetSize(uint32_t ID) {
#ifndef NDEBUG
	if ( ID >= d_size.size() ) {
		throw std::out_of_range("ID is out of range");
	}
#endif
	return d_size[ID];
}

void ComponentConnecter::PrintDebug(const cv::Size & s ,cv::Mat & res) {
	if ( s.width * s.height != d_parent.size() ) {
		throw std::invalid_argument("size mismatch");
	}

	res = cv::Mat(s.height,s.width,CV_8UC3);

	ColorLabeller labeller;


	for (size_t y = 0; y < res.rows; ++y ) {
		for (size_t x = 0; x < res.cols; ++x ) {
			uint32_t rep = GetRepresentative(y*res.cols+x);
			if (GetSize(rep) < 5 ) {
				res.at<cv::Vec3b>(y,x) = cv::Vec3b(0,0,0);
				continue;
			}
			res.at<cv::Vec3b>(y,x) = labeller.Color(rep);
		}
	}


}
