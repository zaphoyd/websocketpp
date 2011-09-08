#include "network_utilities.hpp"

uint64_t htonll(uint64_t src) { 
	static int typ = TYP_INIT; 
	unsigned char c; 
	union { 
		uint64_t ull; 
		unsigned char c[8]; 
	} x; 
	if (typ == TYP_INIT) { 
		x.ull = 0x01; 
		typ = (x.c[7] == 0x01ULL) ? TYP_BIGE : TYP_SMLE; 
	} 
	if (typ == TYP_BIGE) 
		return src; 
	x.ull = src; 
	c = x.c[0]; x.c[0] = x.c[7]; x.c[7] = c; 
	c = x.c[1]; x.c[1] = x.c[6]; x.c[6] = c; 
	c = x.c[2]; x.c[2] = x.c[5]; x.c[5] = c; 
	c = x.c[3]; x.c[3] = x.c[4]; x.c[4] = c; 
	return x.ull; 
}

uint64_t ntohll(uint64_t src) { 
	return htonll(src);
}