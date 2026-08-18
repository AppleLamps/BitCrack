#ifndef _BITCRACK_KANGAROO_H
#define _BITCRACK_KANGAROO_H

#include "secp256k1.h"
#include <string>

struct KangarooConfig {
	secp256k1::ecpoint pub;
	secp256k1::uint256 start;
	secp256k1::uint256 end;
	int threads;
	int herdSize;
	int dpBits;
	uint64_t statusIntervalMs;
};

struct KangarooResult {
	bool found;
	secp256k1::uint256 privateKey;
	uint64_t jumps;
	uint64_t distinguished;
};

KangarooResult runKangaroo(const KangarooConfig &config);

#endif
