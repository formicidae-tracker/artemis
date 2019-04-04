#include "Partitions.h"



void PartitionInTwo(const cv::Rect & rect,size_t partitions, cv::Rect & a, cv::Rect & b) {
	cv::Size sizeA,sizeB;
	cv::Point tlB;
	if ( rect.height > rect.width ) {
		sizeA = cv::Size(rect.width,rect.height/partitions);
		sizeB = cv::Size(rect.width,rect.height - sizeA.height);
		tlB = cv::Point(rect.x,rect.y+sizeA.height);
	} else {
		sizeA = cv::Size(rect.width / partitions, rect.height);
		sizeB = cv::Size(rect.width - sizeA.width,rect.height);
		tlB = cv::Point(rect.x+sizeA.width,rect.y);
	}
	a = cv::Rect(rect.tl(),sizeA);
	b = cv::Rect(tlB,sizeB);
}


void PartitionRectangle(const cv::Rect & rect, size_t partitions, Partition & results) {
	if (partitions == 1 ) {
		results.push_back(rect);
		return;
	}

	if ( partitions % 2 == 1 ) {
		cv::Rect single,rest;
		PartitionInTwo(rect,partitions,single,rest);
		results.push_back(single);
		PartitionRectangle(rest, partitions-1, results);
		return;
	}

	cv::Rect a,b;
	PartitionInTwo(rect,2,a,b);
	PartitionRectangle(a,partitions/2,results);
	PartitionRectangle(b,partitions/2,results);
}


void AddMargin(const cv::Size & size, size_t margin, Partition & partitions) {
	for ( auto & p : partitions ) {
		if (p.x >= margin) {
			p.x -= margin;
			p.width += margin;
		} else {
			p.width += p.x;
			p.x = 0;
		}

		if ( p.x + p.width + margin <= size.width ) {
			p.width += margin;
		} else {
			p.width = size.width - p.x;
		}
		if (p.y >= margin ) {
			p.y -= margin;
			p.height += margin;
		} else {
			p.height += p.y;
			p.y = 0;
		}
		if (p.y + p.height + margin <= size.height ) {
			p.height += margin;
		} else {
			p.height = size.height - p.y;
		}
	}
}
