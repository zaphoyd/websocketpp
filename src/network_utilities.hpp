#ifndef NETWORK_UTILITIES_HPP
#define NETWORK_UTILITIES_HPP

#include <stdint.h>

// http://www.viva64.com/en/k/0018/
// TODO: impliment stuff from here: 
// http://stackoverflow.com/questions/809902/64-bit-ntohl-in-c

#define TYP_INIT 0 
#define TYP_SMLE 1 
#define TYP_BIGE 2 

uint64_t htonll(uint64_t src);
uint64_t ntohll(uint64_t src);

#endif // NETWORK_UTILITIES_HPP