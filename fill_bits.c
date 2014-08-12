#include "fill_bits.h"

uint64_t fill_bits(uint8_t n)
{
	int i;
	uint64_t u;

	for(i=0, u=0; i<n; i++){
		u<<=1;
		u|=(uint64_t)1;
	}

	return u;
}
