#pragma once

#include "Family.h"

#include <map>

namespace maytags {

class QuickTagDecoder {
public:
	class Entry {
	public:
		Code     Decoded;
		uint16_t ID;
		uint8_t  Hamming;
		uint8_t  Rotation;
		Entry(Code decoded,uint16_t id, uint8_t hamming);
	};

	QuickTagDecoder(const Family::Ptr & family, size_t maxHamming );

	bool Decode(Code c, Entry & entry);
private :
	static Code Rotate90(Code c,size_t numberOfBits);

	typedef std::map<Code,Entry> CodeDictionnary;
	CodeDictionnary d_dictionnary;
	size_t          d_numBits;
};


} //namespace maytags
