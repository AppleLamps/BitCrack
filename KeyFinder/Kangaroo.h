#ifndef _BITCRACK_KANGAROO_H
#define _BITCRACK_KANGAROO_H

#include "secp256k1.h"
#include <string>

enum KangarooOpt {
	KANGAROO_OPT_NONE = 0,
	KANGAROO_OPT_BATCH_ADD = 1,
	KANGAROO_OPT_JUMPS = 2,
	KANGAROO_OPT_DP = 4,
	KANGAROO_OPT_FALCON = 8,
	KANGAROO_OPT_ADAPT = 16
};

struct KangarooConfig {
	secp256k1::ecpoint pub;
	secp256k1::uint256 start;
	secp256k1::uint256 end;
	int threads = 1;
	int herdSize = 64;
	int dpBits = 0;
	int optFlags = 0;
	bool quiet = false;
	uint64_t statusIntervalMs = 1800;
	uint64_t maxMs = 0;
	uint64_t maxJumps = 0;
};

struct KangarooResult {
	bool found;
	bool timedOut;
	secp256k1::uint256 privateKey;
	uint64_t jumps;
	uint64_t distinguished;
	uint64_t elapsedMs;
	uint64_t adaptRebalances;
};

KangarooResult runKangaroo(const KangarooConfig &config);
int runKangarooBench(const KangarooConfig &base);

#endif
