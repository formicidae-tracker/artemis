#pragma once

#include <vector>
#include <memory>

#include <Eigen/StdVector>

namespace maytags {

typedef uint64_t Code;

class Family {
public :
	typedef std::shared_ptr<const Family> Ptr;

	static Ptr Create(const std::string & name);

	size_t NumberOfBits;
	std::vector<Code> Codes;

	size_t WidthAtBorder;
	size_t TotalWidth;
	bool ReversedBorder;


	typedef Eigen::Matrix<size_t,2,1> Vector2u;
	typedef std::vector<Vector2u,Eigen::aligned_allocator<Vector2u> > BitLocationList;

	BitLocationList BitLocation;

};

} //maytags
