#include "ComponentConnecter.h"
#include <map>

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

#define ID(x,y) ((y) * image.cols + (x))

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

size_t ComponentConnecter::GetRepresentative(size_t ID) {
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

	size_t root = ID;
	while ( d_parent[root] != root ) {
#ifndef NDEBUG
		if (root >= d_parent.size() ) {
			throw std::out_of_range("tree walking: index out of range");
		}
#endif
		root = d_parent[root];
	}

	while ( d_parent[ID] != root ) {
		size_t tmp = d_parent[ID];
		d_parent[ID] = root;
		ID = tmp;
	}

	return root;
}


size_t ComponentConnecter::Connect(size_t a, size_t b) {
	size_t aRoot = GetRepresentative(a);
	size_t bRoot = GetRepresentative(b);

	if ( aRoot == bRoot ) {
		return aRoot;
	}
	size_t aSize = d_size[aRoot];
	size_t bSize = d_size[bRoot];

	if (aSize > bSize ) {
		d_parent[bRoot] = aRoot;
		d_size[aRoot] += bSize;
		return aRoot;
	}
	d_parent[aRoot] = bRoot;
	d_size[bRoot] += aSize;

	return bRoot;

}

size_t ComponentConnecter::GetSize(size_t ID) {
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

	std::vector<cv::Vec3b> colors = {
		cv::Vec3b(219,112,147),
		cv::Vec3b(200,0,0),
		cv::Vec3b(230,100,60),
		cv::Vec3b(242,155,120),
		cv::Vec3b(230,224,176),
		cv::Vec3b(170,178,32),
		cv::Vec3b(50,205,154),
		cv::Vec3b(87,139,46),
		cv::Vec3b(190,230,245),
		cv::Vec3b(135,184,222),
		cv::Vec3b(0,225,255),
		cv::Vec3b(0,165,255),
		cv::Vec3b(0,69,255),
		cv::Vec3b(34,34,178),
		cv::Vec3b(193,182,255),
		cv::Vec3b(147,20,255),
	};
	size_t nextColor = 0;
	std::map<size_t,size_t> colorMapping;

	for (size_t y = 0; y < res.rows; ++y ) {
		for (size_t x = 0; x < res.cols; ++x ) {
			size_t rep = GetRepresentative(y*res.cols+x);
			if (GetSize(rep) < 5 ) {
				res.at<cv::Vec3b>(y,x) = cv::Vec3b(00,0,0);
				continue;
			}
			if ( colorMapping.count(rep) == 0 ) {
				colorMapping[rep] = nextColor;
				nextColor = (nextColor + 1) % colors.size();
			}
			cv::Vec3b color = colors[colorMapping[rep]];
			res.at<cv::Vec3b>(y,x) = color;
		}
	}


}
