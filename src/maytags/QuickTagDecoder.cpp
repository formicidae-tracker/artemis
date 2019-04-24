#include "QuickTagDecoder.h"

#define CODE_ONE ((Code)1)

using namespace maytags;

QuickTagDecoder::Entry::Entry(Code decoded,uint16_t id, uint8_t hamming)
	: Decoded(decoded)
	, ID(id)
	, Hamming(hamming)
	, Rotation(0) {
}

QuickTagDecoder::QuickTagDecoder(const Family::Ptr & family, size_t maxHamming )
	: d_numBits(family->NumberOfBits) {
	if ( maxHamming > 2 ) {
		throw std::invalid_argument("Only a maximum hamming  <= 2 is supported");
	}
	uint16_t id = 0;
	for ( auto const & c : family->Codes ) {
		d_dictionnary.emplace(std::make_pair(c,Entry(c,id,0)));

		if ( maxHamming >= 1 ) {
			for (size_t i = 0; i < family->NumberOfBits; ++i ) {
				Code newCode = c ^(CODE_ONE << i);
				d_dictionnary.emplace(std::make_pair(newCode,Entry(c,id,1)));
			}
		}

		if ( maxHamming >= 2 ) {
			for (size_t i = 0; i < family->NumberOfBits; ++i ) {
				for ( size_t j = 0; j < i ; ++j ) {
					Code newCode = c ^(CODE_ONE << i) ^ (CODE_ONE << j);
					d_dictionnary.emplace(std::make_pair(newCode,Entry(c,id,2)));
				}
			}
		}

		++id;
	}
}

bool QuickTagDecoder::Decode(Code c, Entry & entry) {
	for(size_t r = 0; r < 4; ++r) {
		CodeDictionnary::const_iterator search = d_dictionnary.find(c);
		if ( search != d_dictionnary.end() ) {
			entry = search->second;
			entry.Rotation = r;
			return true;
		}

		c = Rotate90(c,d_numBits);
	}
	return false;
}


Code QuickTagDecoder::Rotate90(Code w,size_t numberOfBits) {
	size_t p = numberOfBits;
	uint64_t l = 0;
	if (numberOfBits % 4 == 1) {
		p = numberOfBits - 1;
	    l = 1;
    }
    w = ((w >> l) << (p/4 + l)) | (w >> (3 * p/ 4 + l) << l) | (w & l);
    w &= (( ((Code)1) << numberOfBits) - 1);
    return w;
}
