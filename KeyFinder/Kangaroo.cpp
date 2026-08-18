#include "Kangaroo.h"

#include "CryptoUtil.h"
#include "Logger.h"
#include "util.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

static const int kJumpCount = 32;
static const int kFalconHeat = 8;

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

static bool isDistinguished(const secp256k1::uint256 &x, unsigned int mask, bool mixed)
{
	unsigned int w = mixed ? (x.v[0] ^ x.v[1]) : x.v[0];
	return (w & mask) == 0;
}

static unsigned int jumpIndexOf(const secp256k1::uint256 &x, bool mixed)
{
	if(!mixed) {
		return x.v[0] % (unsigned int)kJumpCount;
	}
	unsigned int h = x.v[0] ^ x.v[1] ^ (x.v[0] >> 13) ^ (x.v[2] * 0x9e3779b9u);
	h ^= x.v[3] + 0x85ebca6bu;
	return h & (unsigned int)(kJumpCount - 1);
}

static unsigned int falconHeat(const secp256k1::uint256 &x)
{
	// 0 = cold (soar / large jumps), 7 = hot (stoop / small jumps).
	// This is a function of the visible curve point, not of the secret
	// scalar: a live heatmap of x cannot be updated from wild landings,
	// because those landings are EC points, not scalars.
	unsigned int h = x.v[0] ^ (x.v[0] >> 11) ^ x.v[1] ^ (x.v[2] * 0x9e3779b9u);
	h ^= x.v[3] + 0x85ebca6bu;
	return (h >> 3) & (unsigned int)(kFalconHeat - 1);
}

static unsigned int falconSlot(const secp256k1::uint256 &x)
{
	return falconHeat(x) * (unsigned int)kJumpCount + jumpIndexOf(x, true);
}

static secp256k1::uint256 shrBits(const secp256k1::uint256 &a, int bits)
{
	secp256k1::uint256 r;
	if(bits <= 0) {
		return a;
	}
	if(bits >= 256) {
		return r;
	}
	const int limb = bits / 32;
	const int rem = bits % 32;
	for(int i = 0; i < 8; i++) {
		int src = i + limb;
		if(src >= 8) {
			r.v[i] = 0;
		} else {
			r.v[i] = a.v[src] >> rem;
			if(rem != 0 && src + 1 < 8) {
				r.v[i] |= a.v[src + 1] << (32 - rem);
			}
		}
	}
	return r;
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

static int autoDpBitsHerd(int rangeBits, int herdSize)
{
	int dp = autoDpBits(rangeBits) - 2;
	int herdLog = 1;
	while(herdLog < 30 && (1 << herdLog) < herdSize) {
		herdLog++;
	}
	if(herdLog >= 8) {
		dp -= 1;
	}
	if(dp < 2) {
		dp = 2;
	}
	if(dp > 20) {
		dp = 20;
	}
	return dp;
}

static void buildJumpDists(int rangeBits, int herdSize, bool retuned, secp256k1::uint256 jumpDist[kJumpCount])
{
	if(retuned) {
		int herdLog = 1;
		while(herdLog < 30 && (1 << herdLog) < std::max(2, herdSize)) {
			herdLog++;
		}
		int meanBits = std::max(1, rangeBits / 2 - 1 - herdLog / 2);
		int unitBits = std::max(0, meanBits - 4);
		secp256k1::uint256 unit = shlOne(unitBits);
		if(unit.isZero()) {
			unit = secp256k1::uint256(1);
		}
		for(int i = 0; i < kJumpCount; i++) {
			jumpDist[i] = unit * (unsigned int)(i + 1);
			if(i > 0) {
				jumpDist[i] = jumpDist[i] + (unsigned int)i;
			}
		}
	} else {
		const int maxJumpBit = std::max(1, rangeBits / 2);
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
		}
	}

	secp256k1::uint256 cap = shlOne(std::max(1, rangeBits - 1));
	for(int i = 0; i < kJumpCount; i++) {
		if(jumpDist[i].cmp(cap) > 0) {
			jumpDist[i] = cap;
		}
		if(jumpDist[i].isZero()) {
			jumpDist[i] = secp256k1::uint256(1);
		}
	}
}

static void buildFalconJumps(int rangeBits, int herdSize, secp256k1::uint256 *jumpDist, int nJumps)
{
	secp256k1::uint256 base[kJumpCount];
	buildJumpDists(rangeBits, herdSize, true, base);
	for(int h = 0; h < kFalconHeat; h++) {
		for(int i = 0; i < kJumpCount; i++) {
			secp256k1::uint256 d = shrBits(base[i], h);
			if(d.isZero()) {
				d = secp256k1::uint256(1);
			}
			jumpDist[h * kJumpCount + i] = d;
		}
	}
	(void)nJumps;
}

static std::string optLabel(int flags)
{
	if(flags == 0) {
		return "baseline";
	}
	std::string s;
	if(flags & KANGAROO_OPT_BATCH_ADD) {
		s += "batch";
	}
	if(flags & KANGAROO_OPT_JUMPS) {
		if(!s.empty()) {
			s += "+";
		}
		s += "jumps";
	}
	if(flags & KANGAROO_OPT_DP) {
		if(!s.empty()) {
			s += "+";
		}
		s += "dp";
	}
	if(flags & KANGAROO_OPT_FALCON) {
		if(!s.empty()) {
			s += "+";
		}
		s += "falcon";
	}
	return s;
}

KangarooResult runKangaroo(const KangarooConfig &config)
{
	KangarooResult out;
	out.found = false;
	out.timedOut = false;
	out.jumps = 0;
	out.distinguished = 0;
	out.elapsedMs = 0;

	if(config.start.isZero() || config.start.cmp(config.end) > 0) {
		throw std::string("Invalid kangaroo keyspace");
	}
	if(!secp256k1::pointExists(config.pub)) {
		throw std::string("Public key is not on secp256k1");
	}

	const bool useBatch = (config.optFlags & KANGAROO_OPT_BATCH_ADD) != 0;
	const bool useJumps = (config.optFlags & KANGAROO_OPT_JUMPS) != 0;
	const bool useDp = (config.optFlags & KANGAROO_OPT_DP) != 0;
	const bool useFalcon = (config.optFlags & KANGAROO_OPT_FALCON) != 0;

	const secp256k1::uint256 width = config.end - config.start + 1;
	const int rangeBits = std::max(1, bitLength(width));
	int dpBits = config.dpBits;
	if(dpBits <= 0) {
		dpBits = useDp ? autoDpBitsHerd(rangeBits, std::max(2, config.herdSize)) : autoDpBits(rangeBits);
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

	const int nJumps = useFalcon ? (kJumpCount * kFalconHeat) : kJumpCount;
	std::vector<secp256k1::uint256> jumpDist((size_t)nJumps);
	std::vector<secp256k1::ecpoint> jumpPoint((size_t)nJumps);
	secp256k1::ecpoint g = secp256k1::G();

	if(useFalcon) {
		buildFalconJumps(rangeBits, herdSize, jumpDist.data(), nJumps);
	} else {
		buildJumpDists(rangeBits, herdSize, useJumps, jumpDist.data());
	}
	std::vector<secp256k1::uint256> jumpKeys((size_t)nJumps);
	std::vector<secp256k1::ecpoint> jumpPts;
	for(int i = 0; i < nJumps; i++) {
		jumpKeys[(size_t)i] = jumpDist[(size_t)i];
	}
	secp256k1::generateKeyPairsBulk(g, jumpKeys, jumpPts);
	for(int i = 0; i < nJumps; i++) {
		jumpPoint[(size_t)i] = jumpPts[(size_t)i];
	}

	uint64_t maxHops;
	if(useDp) {
		int cap = std::min(dpBits, 40);
		uint64_t span = 1ull << cap;
		maxHops = std::max(4096ull, span * 4ull);
	} else {
		maxHops = 1ull << std::min(40, rangeBits / 2 + dpBits + 8);
	}

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

	if(!config.quiet) {
		secp256k1::ecpoint pubLog = config.pub;
		secp256k1::uint256 startLog = config.start;
		secp256k1::uint256 endLog = config.end;
		Logger::log(LogLevel::Info, "Kangaroo ECDLP (" + optLabel(config.optFlags) + ")");
		Logger::log(LogLevel::Info, "Public key:  " + pubLog.toString(true));
		Logger::log(LogLevel::Info, "Range:       " + startLog.toString() + " : " + endLog.toString());
		Logger::log(LogLevel::Info, "Range bits:  " + util::format((uint64_t)rangeBits));
		Logger::log(LogLevel::Info, "Herd:        " + util::format((uint64_t)nTame) + " tame / " +
		            util::format((uint64_t)nWild) + " wild");
		Logger::log(LogLevel::Info, "DP bits:     " + util::format((uint64_t)dpBits));
		Logger::log(LogLevel::Info, "Threads:     " + util::format((uint64_t)threads));
	}

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

	auto considerDp = [&](Roo &roo, bool force) {
		if(!force && !isDistinguished(roo.p.x, dpMask, useDp)) {
			return;
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
	};

	if(useDp) {
		for(int i = 0; i < herdSize; i++) {
			considerDp(herd[(size_t)i], true);
		}
	}

	std::vector<secp256k1::ecpoint> batchP;
	std::vector<secp256k1::ecpoint> batchQ;
	std::vector<unsigned int> batchJ;
	if(useBatch) {
		batchP.resize((size_t)herdSize);
		batchQ.resize((size_t)herdSize);
		batchJ.resize((size_t)herdSize);
	}

	while(!found.load(std::memory_order_relaxed)) {
		for(int i = 0; i < herdSize; i++) {
			Roo &roo = herd[(size_t)i];
			if(secp256k1::isPointAtInfinity(roo.p) || roo.hops > maxHops) {
				reseed(roo);
				if(useDp) {
					considerDp(roo, true);
				}
			}
		}

		if(useBatch) {
			for(int i = 0; i < herdSize; i++) {
			const unsigned int j = useFalcon ? falconSlot(herd[(size_t)i].p.x)
			                                 : jumpIndexOf(herd[(size_t)i].p.x, useJumps);
			batchJ[(size_t)i] = j;
			batchP[(size_t)i] = herd[(size_t)i].p;
			batchQ[(size_t)i] = jumpPoint[(size_t)j];
			}
			secp256k1::addPointsIndependent(batchP, batchQ, threads);
			for(int i = 0; i < herdSize; i++) {
				unsigned int j = batchJ[(size_t)i];
				herd[(size_t)i].p = batchP[(size_t)i];
				herd[(size_t)i].d = herd[(size_t)i].d + jumpDist[(size_t)j];
				herd[(size_t)i].hops++;
				considerDp(herd[(size_t)i], false);
			}
		} else {
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
				const unsigned int j = useFalcon ? falconSlot(roo.p.x)
				                                 : jumpIndexOf(roo.p.x, useJumps);
				roo.p = secp256k1::addPoints(roo.p, jumpPoint[(size_t)j]);
				roo.d = roo.d + jumpDist[(size_t)j];
				roo.hops++;
				considerDp(roo, false);
			}
		}

		jumps.fetch_add((uint64_t)herdSize, std::memory_order_relaxed);

		uint64_t now = util::getSystemTime();
		if(config.maxMs > 0 && now - t0 >= config.maxMs) {
			out.timedOut = true;
			break;
		}
		if(config.maxJumps > 0 && jumps.load(std::memory_order_relaxed) >= config.maxJumps) {
			out.timedOut = true;
			break;
		}

		if(!config.quiet && now - lastStatus >= statusEvery) {
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
	out.elapsedMs = util::getSystemTime() - t0;
	return out;
}

static KangarooConfig benchCfg(const KangarooConfig &base, const secp256k1::uint256 &start,
                               const secp256k1::uint256 &end, const secp256k1::uint256 &key, int flags,
                               uint64_t maxMs)
{
	KangarooConfig c = base;
	c.start = start;
	c.end = end;
	c.pub = secp256k1::multiplyPoint(key, secp256k1::G());
	c.optFlags = flags;
	c.quiet = true;
	c.statusIntervalMs = 60000;
	c.maxMs = maxMs;
	c.maxJumps = 0;
	c.dpBits = 0;
	return c;
}

static void logBench(const std::string &msg)
{
	Logger::log(LogLevel::Info, msg);
}

int runKangarooBench(const KangarooConfig &baseIn)
{
	KangarooConfig base = baseIn;
	if(base.threads < 1) {
		base.threads = 4;
	}
	if(base.herdSize < 2 || base.herdSize > 1024) {
		base.herdSize = 256;
	}

	const int variants[] = {
		KANGAROO_OPT_NONE,
		KANGAROO_OPT_BATCH_ADD,
		KANGAROO_OPT_JUMPS,
		KANGAROO_OPT_DP,
		KANGAROO_OPT_BATCH_ADD | KANGAROO_OPT_FALCON
	};
	const int nVar = 5;

	logBench("Kangaroo variant bench");
	logBench("Herd: " + util::format((uint64_t)base.herdSize) +
	         "  Threads: " + util::format((uint64_t)base.threads));

	struct NamedKey {
		const char *name;
		secp256k1::uint256 start;
		secp256k1::uint256 end;
		secp256k1::uint256 key;
	};

	NamedKey check[] = {
		{"k=1 in 1:8", secp256k1::uint256(1), secp256k1::uint256(8), secp256k1::uint256(1)},
		{"k=0x1000 in 1:2000", secp256k1::uint256(1), secp256k1::uint256(0x2000), secp256k1::uint256(0x1000)},
		{"k=0xABCDEF in AB0000:AC0000", secp256k1::uint256("AB0000"), secp256k1::uint256("AC0000"), secp256k1::uint256("ABCDEF")}
	};

	logBench("--- correctness ---");
	bool ok = true;
	for(int v = 0; v < nVar; v++) {
		for(int t = 0; t < 3; t++) {
			KangarooConfig c = benchCfg(base, check[t].start, check[t].end, check[t].key, variants[v], 30000);
			if(t == 0) {
				c.herdSize = 4;
			}
			KangarooResult r = runKangaroo(c);
			std::string line = optLabel(variants[v]) + "  " + check[t].name + "  ";
			if(!r.found || r.privateKey.cmp(check[t].key) != 0) {
				line += "FAIL";
				ok = false;
			} else {
				line += "ok  jumps=" + util::formatThousands(r.jumps) +
				        "  " + util::format((uint64_t)r.elapsedMs) + " ms";
			}
			logBench(line);
		}
	}
	if(!ok) {
		logBench("Correctness failed; skipping remaining benches");
		return 1;
	}

	logBench("--- throughput (2.0s, range 2^36) ---");
	secp256k1::uint256 tStart(1);
	secp256k1::uint256 tEnd((uint64_t)1 << 36);
	secp256k1::uint256 tKey((uint64_t)0x123456789ULL);
	double jps[5] = {0, 0, 0, 0, 0};
	for(int v = 0; v < nVar; v++) {
		KangarooConfig c = benchCfg(base, tStart, tEnd, tKey, variants[v], 2000);
		KangarooResult r = runKangaroo(c);
		double secs = (double)r.elapsedMs / 1000.0;
		jps[v] = secs > 0 ? (double)r.jumps / secs : 0;
		logBench(optLabel(variants[v]) + "  " + util::formatThousands(r.jumps) + " jumps  " +
		         util::format("%.0f", jps[v]) + " j/s  dp=" + util::formatThousands(r.distinguished));
	}

	struct SolveKey {
		secp256k1::uint256 key;
	};
	secp256k1::uint256 sStart(0x100000);
	secp256k1::uint256 sEnd(0x4FFFFF);
	SolveKey solves[] = {
		{secp256k1::uint256(0x123456)},
		{secp256k1::uint256(0x2F00AA)},
		{secp256k1::uint256(0x3C0FFE)},
		{secp256k1::uint256(0x1EDCBA)}
	};
	const int nSolve = 4;

	logBench("--- solve (range 2^22, 4 keys, 45s cap) ---");
	double meanJumps[5] = {0, 0, 0, 0, 0};
	double meanMs[5] = {0, 0, 0, 0, 0};
	int foundN[5] = {0, 0, 0, 0, 0};
	for(int v = 0; v < nVar; v++) {
		uint64_t sumJ = 0;
		uint64_t sumMs = 0;
		for(int t = 0; t < nSolve; t++) {
			KangarooConfig c = benchCfg(base, sStart, sEnd, solves[t].key, variants[v], 45000);
			KangarooResult r = runKangaroo(c);
			std::string line = optLabel(variants[v]) + "  key=" + solves[t].key.toString() + "  ";
			if(!r.found) {
				line += r.timedOut ? "TIMEOUT" : "FAIL";
			} else {
				foundN[v]++;
				sumJ += r.jumps;
				sumMs += r.elapsedMs;
				line += "ok  jumps=" + util::formatThousands(r.jumps) +
				        "  " + util::format((uint64_t)r.elapsedMs) + " ms";
			}
			logBench(line);
		}
		if(foundN[v] > 0) {
			meanJumps[v] = (double)sumJ / (double)foundN[v];
			meanMs[v] = (double)sumMs / (double)foundN[v];
		}
		logBench(optLabel(variants[v]) + "  mean jumps=" + util::format("%.0f", meanJumps[v]) +
		         "  mean ms=" + util::format("%.0f", meanMs[v]) +
		         "  found=" + util::format(foundN[v]) + "/" + util::format(nSolve));
	}

	logBench("--- solve (range 2^26, 2 keys, 20s cap) ---");
	secp256k1::uint256 mStart((uint64_t)0x2000000);
	secp256k1::uint256 mEnd((uint64_t)0x3FFFFFF);
	secp256k1::uint256 mKeys[2] = {secp256k1::uint256((uint64_t)0x2ABCDEF), secp256k1::uint256((uint64_t)0x35E0001)};
	double meanJumps26[5] = {0, 0, 0, 0, 0};
	double meanMs26[5] = {0, 0, 0, 0, 0};
	int found26[5] = {0, 0, 0, 0, 0};
	for(int v = 0; v < nVar; v++) {
		uint64_t sumJ = 0;
		uint64_t sumMs = 0;
		for(int t = 0; t < 2; t++) {
			KangarooConfig c = benchCfg(base, mStart, mEnd, mKeys[t], variants[v], 20000);
			KangarooResult r = runKangaroo(c);
			std::string line = optLabel(variants[v]) + "  key=" + mKeys[t].toString() + "  ";
			if(!r.found) {
				line += r.timedOut ? "TIMEOUT" : "FAIL";
			} else {
				found26[v]++;
				sumJ += r.jumps;
				sumMs += r.elapsedMs;
				line += "ok  jumps=" + util::formatThousands(r.jumps) +
				        "  " + util::format((uint64_t)r.elapsedMs) + " ms";
			}
			logBench(line);
		}
		if(found26[v] > 0) {
			meanJumps26[v] = (double)sumJ / (double)found26[v];
			meanMs26[v] = (double)sumMs / (double)found26[v];
		}
		logBench(optLabel(variants[v]) + "  mean jumps=" + util::format("%.0f", meanJumps26[v]) +
		         "  mean ms=" + util::format("%.0f", meanMs26[v]) +
		         "  found=" + util::format(found26[v]) + "/2");
	}

	int winJps = 0;
	int winJumps = 0;
	int winMs = 0;
	for(int v = 1; v < nVar; v++) {
		if(jps[v] > jps[winJps]) {
			winJps = v;
		}
		if(foundN[v] >= foundN[winJumps] && meanJumps[v] > 0 &&
		   (meanJumps[winJumps] == 0 || meanJumps[v] < meanJumps[winJumps])) {
			winJumps = v;
		}
		if(foundN[v] >= foundN[winMs] && meanMs[v] > 0 &&
		   (meanMs[winMs] == 0 || meanMs[v] < meanMs[winMs])) {
			winMs = v;
		}
	}

	int combo = 0;
	if(jps[1] > jps[0] * 1.05) {
		combo |= KANGAROO_OPT_BATCH_ADD;
	}
	if(found26[2] > 0) {
		if(meanJumps26[2] > 0 && meanJumps26[2] < meanJumps26[0] * 0.95) {
			combo |= KANGAROO_OPT_JUMPS;
		}
	} else if(foundN[2] > 0 && meanJumps[2] > 0 && meanJumps[2] < meanJumps[0] * 0.95) {
		combo |= KANGAROO_OPT_JUMPS;
	}
	if(foundN[3] > 0 && meanMs[3] > 0 && meanMs[3] < meanMs[0] * 0.95) {
		combo |= KANGAROO_OPT_DP;
	}
	if(found26[4] > 0 && meanJumps26[4] > 0 && meanJumps26[1] > 0 &&
	   meanJumps26[4] < meanJumps26[1] * 0.95) {
		combo |= KANGAROO_OPT_FALCON;
	} else if(foundN[4] > 0 && meanJumps[4] > 0 && meanJumps[1] > 0 &&
	          meanJumps[4] < meanJumps[1] * 0.95) {
		combo |= KANGAROO_OPT_FALCON;
	}

	logBench("--- summary ---");
	logBench("Best throughput: " + optLabel(variants[winJps]) + " (" + util::format("%.0f", jps[winJps]) + " j/s)");
	logBench("Fewest jumps:    " + optLabel(variants[winJumps]) + " (mean " +
	         util::format("%.0f", meanJumps[winJumps]) + ")");
	logBench("Fastest solve:   " + optLabel(variants[winMs]) + " (mean " +
	         util::format("%.0f", meanMs[winMs]) + " ms)");

	if(combo != 0 && combo != KANGAROO_OPT_BATCH_ADD && combo != KANGAROO_OPT_JUMPS && combo != KANGAROO_OPT_DP) {
		logBench("--- combined winners (" + optLabel(combo) + ") ---");
		KangarooConfig ct = benchCfg(base, tStart, tEnd, tKey, combo, 2000);
		KangarooResult rt = runKangaroo(ct);
		double secs = (double)rt.elapsedMs / 1000.0;
		double cjps = secs > 0 ? (double)rt.jumps / secs : 0;
		logBench(optLabel(combo) + "  throughput " + util::format("%.0f", cjps) + " j/s");

		uint64_t sumJ = 0;
		uint64_t sumMs = 0;
		int foundC = 0;
		for(int t = 0; t < nSolve; t++) {
			KangarooConfig c = benchCfg(base, sStart, sEnd, solves[t].key, combo, 45000);
			KangarooResult r = runKangaroo(c);
			if(r.found) {
				foundC++;
				sumJ += r.jumps;
				sumMs += r.elapsedMs;
			}
			logBench(optLabel(combo) + "  key=" + solves[t].key.toString() + "  " +
			         (r.found ? ("ok  jumps=" + util::formatThousands(r.jumps) +
			                     "  " + util::format((uint64_t)r.elapsedMs) + " ms")
			                  : (r.timedOut ? "TIMEOUT" : "FAIL")));
		}
		if(foundC > 0) {
			logBench(optLabel(combo) + "  mean jumps=" + util::format("%.0f", (double)sumJ / foundC) +
			         "  mean ms=" + util::format("%.0f", (double)sumMs / foundC) +
			         "  found=" + util::format(foundC) + "/" + util::format(nSolve));
		}
	} else if(combo == 0) {
		logBench("No variant beat baseline by 5%; keep baseline.");
	} else {
		logBench("Recommended --kangaroo-opt " + util::format(combo) + " (" + optLabel(combo) + ")");
	}

	return ok ? 0 : 1;
}
