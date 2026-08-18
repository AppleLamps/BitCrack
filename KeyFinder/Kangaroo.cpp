#include "Kangaroo.h"

#include "CryptoUtil.h"
#include "Logger.h"
#include "util.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

static const int kJumpCount = 32;

struct Roo {
	secp256k1::ecpoint p;
	secp256k1::uint256 d;
	uint64_t hops = 0;
	bool wild;
};

struct DpKey {
	unsigned int v[8];

	bool operator==(const DpKey &o) const
	{
		for(int i = 0; i < 8; i++) {
			if(v[i] != o.v[i]) {
				return false;
			}
		}
		return true;
	}
};

struct DpKeyHash {
	size_t operator()(const DpKey &k) const
	{
		size_t h = k.v[0];
		h ^= (size_t)k.v[1] + 0x9e3779b9u + (h << 6) + (h >> 2);
		h ^= (size_t)k.v[2] + 0x9e3779b9u + (h << 6) + (h >> 2);
		h ^= (size_t)k.v[3] + 0x9e3779b9u + (h << 6) + (h >> 2);
		return h;
	}
};

struct DpEntry {
	secp256k1::uint256 d;
	bool wild;
};

static int bitLength(const secp256k1::uint256 &a)
{
	for(int i = 7; i >= 0; i--) {
		if(a.v[i] != 0) {
#if defined(__GNUC__)
			return i * 32 + 32 - __builtin_clz(a.v[i]);
#else
			unsigned int x = a.v[i];
			int n = i * 32 + 1;
			while(x >>= 1) {
				n++;
			}
			return n;
#endif
		}
	}
	return 0;
}

static secp256k1::uint256 shlOne(int bits)
{
	secp256k1::uint256 r;
	if(bits < 0 || bits >= 256) {
		return r;
	}
	r.v[bits / 32] = 1u << (bits % 32);
	return r;
}

static DpKey makeKey(const secp256k1::uint256 &x)
{
	DpKey k;
	for(int i = 0; i < 8; i++) {
		k.v[i] = x.v[i];
	}
	return k;
}

static bool isDistinguished(const secp256k1::uint256 &x, unsigned int mask)
{
	return (x.v[0] & mask) == 0;
}

static secp256k1::uint256 randomBelow(crypto::Rng &rng, const secp256k1::uint256 &limit)
{
	if(limit.isZero() || limit.cmp(secp256k1::uint256(1)) <= 0) {
		return secp256k1::uint256(0);
	}

	unsigned char buf[32];
	secp256k1::uint256 r;
	const int bits = bitLength(limit);
	for(int attempt = 0; attempt < 16; attempt++) {
		rng.get(buf, 32);
		unsigned int w[8];
		for(int i = 0; i < 8; i++) {
			w[i] = (unsigned int)buf[i * 4] |
			       ((unsigned int)buf[i * 4 + 1] << 8) |
			       ((unsigned int)buf[i * 4 + 2] << 16) |
			       ((unsigned int)buf[i * 4 + 3] << 24);
		}
		r = secp256k1::uint256(w);
		if(bits < 256) {
			int keep = bits;
			for(int i = 7; i >= 0; i--) {
				if(keep >= 32) {
					keep -= 32;
					continue;
				}
				if(keep <= 0) {
					r.v[i] = 0;
				} else {
					r.v[i] &= (1u << keep) - 1;
					keep = 0;
				}
			}
		}
		if(r.cmp(limit) < 0) {
			return r;
		}
	}
	return secp256k1::uint256(0);
}

static int autoDpBits(int rangeBits)
{
	int dp = rangeBits / 2 - 8;
	if(dp < 4) {
		dp = 4;
	}
	if(dp > 18) {
		dp = 18;
	}
	if(rangeBits <= 20) {
		dp = 4;
	} else if(rangeBits <= 28) {
		dp = 6;
	}
	return dp;
}

KangarooResult runKangaroo(const KangarooConfig &config)
{
	KangarooResult out;
	out.found = false;
	out.jumps = 0;
	out.distinguished = 0;

	if(config.start.isZero() || config.start.cmp(config.end) > 0) {
		throw std::string("Invalid kangaroo keyspace");
	}
	if(!secp256k1::pointExists(config.pub)) {
		throw std::string("Public key is not on secp256k1");
	}

	const secp256k1::uint256 width = config.end - config.start + 1;
	const int rangeBits = std::max(1, bitLength(width));
	int dpBits = config.dpBits;
	if(dpBits <= 0) {
		dpBits = autoDpBits(rangeBits);
	}
	if(dpBits < 1) {
		dpBits = 1;
	}
	if(dpBits > 31) {
		dpBits = 31;
	}
	const unsigned int dpMask = (1u << dpBits) - 1u;

	int threads = config.threads;
	if(threads < 1) {
		threads = 1;
	}

	int herdSize = config.herdSize;
	if(herdSize < 2) {
		herdSize = 2;
	}
	if((herdSize & 1) != 0) {
		herdSize++;
	}

	if(rangeBits <= 32) {
		secp256k1::uint256 w256 = width;
		uint64_t w = w256.toUint64();
		if(w < (uint64_t)herdSize) {
			herdSize = (int)std::max((uint64_t)2, (w / 2) * 2);
			if(herdSize < 2) {
				herdSize = 2;
			}
		}
	}

	const int nTame = herdSize / 2;
	const int nWild = herdSize - nTame;
	const int maxJumpBit = std::max(1, rangeBits / 2);

	secp256k1::uint256 jumpDist[kJumpCount];
	secp256k1::ecpoint jumpPoint[kJumpCount];
	secp256k1::ecpoint g = secp256k1::G();

	std::vector<secp256k1::uint256> jumpKeys(kJumpCount);
	std::vector<secp256k1::ecpoint> jumpPts;
	for(int i = 0; i < kJumpCount; i++) {
		int b = 1 + (i * (maxJumpBit - 1)) / (kJumpCount - 1);
		if(b < 1) {
			b = 1;
		}
		if(b > 255) {
			b = 255;
		}
		jumpDist[i] = shlOne(b - 1);
		if((i & 1) != 0) {
			jumpDist[i] = jumpDist[i] + (unsigned int)(i + 1);
		}
		jumpKeys[i] = jumpDist[i];
	}
	secp256k1::generateKeyPairsBulk(g, jumpKeys, jumpPts);
	for(int i = 0; i < kJumpCount; i++) {
		jumpPoint[i] = jumpPts[i];
	}

	uint64_t maxHops = 1ull << std::min(40, rangeBits / 2 + dpBits + 8);

	crypto::Rng rng;
	std::mutex rngMutex;
	std::vector<Roo> herd((size_t)herdSize);

	std::vector<secp256k1::uint256> tameKeys((size_t)nTame);
	std::vector<secp256k1::ecpoint> tamePts;
	for(int i = 0; i < nTame; i++) {
		secp256k1::uint256 offset = width.div((unsigned int)(nTame + 1)) * (unsigned int)(i + 1);
		tameKeys[(size_t)i] = config.start + offset;
		if(tameKeys[(size_t)i].cmp(config.end) > 0) {
			tameKeys[(size_t)i] = config.end;
		}
	}
	secp256k1::generateKeyPairsBulk(g, tameKeys, tamePts);
	for(int i = 0; i < nTame; i++) {
		herd[(size_t)i].p = tamePts[(size_t)i];
		herd[(size_t)i].d = tameKeys[(size_t)i];
		herd[(size_t)i].hops = 0;
		herd[(size_t)i].wild = false;
	}

	std::vector<secp256k1::uint256> wildOff((size_t)nWild);
	std::vector<secp256k1::ecpoint> wildPts;
	for(int i = 0; i < nWild; i++) {
		wildOff[(size_t)i] = randomBelow(rng, width);
	}
	secp256k1::generateKeyPairsBulk(g, wildOff, wildPts);
	secp256k1::addPointsBulk(wildPts, config.pub, threads);
	for(int i = 0; i < nWild; i++) {
		herd[(size_t)nTame + i].p = wildPts[(size_t)i];
		herd[(size_t)nTame + i].d = wildOff[(size_t)i];
		herd[(size_t)nTame + i].hops = 0;
		herd[(size_t)nTame + i].wild = true;
	}

	std::unordered_map<DpKey, DpEntry, DpKeyHash> table;
	table.reserve(1u << std::min(dpBits + 2, 20));
	std::mutex tableMutex;
	std::atomic<bool> found(false);
	std::atomic<uint64_t> jumps(0);
	std::atomic<uint64_t> dps(0);
	secp256k1::uint256 priv;

	secp256k1::ecpoint pubLog = config.pub;
	secp256k1::uint256 startLog = config.start;
	secp256k1::uint256 endLog = config.end;
	Logger::log(LogLevel::Info, "Kangaroo ECDLP");
	Logger::log(LogLevel::Info, "Public key:  " + pubLog.toString(true));
	Logger::log(LogLevel::Info, "Range:       " + startLog.toString() + " : " + endLog.toString());
	Logger::log(LogLevel::Info, "Range bits:  " + util::format((uint64_t)rangeBits));
	Logger::log(LogLevel::Info, "Herd:        " + util::format((uint64_t)nTame) + " tame / " +
	            util::format((uint64_t)nWild) + " wild");
	Logger::log(LogLevel::Info, "DP bits:     " + util::format((uint64_t)dpBits));
	Logger::log(LogLevel::Info, "Threads:     " + util::format((uint64_t)threads));

	uint64_t statusEvery = config.statusIntervalMs;
	if(statusEvery == 0) {
		statusEvery = 1800;
	}
	uint64_t t0 = util::getSystemTime();
	uint64_t lastStatus = t0;
	uint64_t lastJumps = 0;

	auto reseed = [&](Roo &roo) {
		std::lock_guard<std::mutex> lock(rngMutex);
		if(roo.wild) {
			secp256k1::uint256 off = randomBelow(rng, width);
			roo.d = off;
			roo.p = secp256k1::addPoints(config.pub, secp256k1::multiplyPoint(off, g));
		} else {
			secp256k1::uint256 s = config.start + randomBelow(rng, width);
			if(s.cmp(config.end) > 0) {
				s = config.end;
			}
			roo.d = s;
			roo.p = secp256k1::multiplyPoint(s, g);
		}
		roo.hops = 0;
	};

	while(!found.load(std::memory_order_relaxed)) {
#ifdef _OPENMP
		#pragma omp parallel for schedule(static) num_threads(threads)
		for(int i = 0; i < herdSize; i++) {
#else
		for(int i = 0; i < herdSize; i++) {
#endif
			if(found.load(std::memory_order_relaxed)) {
				continue;
			}

			Roo &roo = herd[(size_t)i];
			if(secp256k1::isPointAtInfinity(roo.p) || roo.hops > maxHops) {
				reseed(roo);
			}

			const unsigned int j = roo.p.x.v[0] % (unsigned int)kJumpCount;
			roo.p = secp256k1::addPoints(roo.p, jumpPoint[j]);
			roo.d = roo.d + jumpDist[j];
			roo.hops++;

			if(!isDistinguished(roo.p.x, dpMask)) {
				continue;
			}

			DpKey key = makeKey(roo.p.x);
			DpEntry incoming;
			incoming.d = roo.d;
			incoming.wild = roo.wild;

			std::lock_guard<std::mutex> lock(tableMutex);
			dps.fetch_add(1, std::memory_order_relaxed);

			auto it = table.find(key);
			if(it == table.end()) {
				table.emplace(key, incoming);
			} else if(it->second.wild != incoming.wild) {
				const DpEntry &tame = incoming.wild ? it->second : incoming;
				const DpEntry &wild = incoming.wild ? incoming : it->second;
				secp256k1::uint256 k = secp256k1::subModN(tame.d, wild.d);
				if(k.cmp(config.start) >= 0 && k.cmp(config.end) <= 0) {
					secp256k1::ecpoint check = secp256k1::multiplyPoint(k, g);
					if(check == config.pub) {
						priv = k;
						found.store(true, std::memory_order_relaxed);
					}
				}
			}
		}

		jumps.fetch_add((uint64_t)herdSize, std::memory_order_relaxed);

		uint64_t now = util::getSystemTime();
		if(now - lastStatus >= statusEvery) {
			uint64_t j = jumps.load(std::memory_order_relaxed);
			double dt = (double)(now - lastStatus) / 1000.0;
			double rate = dt > 0 ? (double)(j - lastJumps) / dt : 0;
			std::string msg = util::formatThousands(j) + " jumps";
			msg += " (" + util::format((uint64_t)rate) + " j/s)";
			msg += "  DP: " + util::formatThousands(dps.load(std::memory_order_relaxed));
			Logger::log(LogLevel::Info, msg);
			lastStatus = now;
			lastJumps = j;
		}
	}

	out.found = found.load();
	out.privateKey = priv;
	out.jumps = jumps.load();
	out.distinguished = dps.load();
	return out;
}
