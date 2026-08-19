#include "Kangaroo.h"

#include <random>
#include <unordered_map>
#include <stdexcept>

#include "util.h"
#include "Logger.h"

using namespace secp256k1;

namespace kangaroo {

int bitLength(const uint256 &v)
{
    for(int i = 7; i >= 0; i--) {
        if(v.v[i]) {
            uint32_t w = v.v[i];
            int b = 0;
            while(w) { w >>= 1; b++; }
            return i * 32 + b;
        }
    }
    return 0;
}

static uint256 randomBits(std::mt19937_64 &rng, int bits)
{
    unsigned int words[8] = { 0 };
    int full = bits / 32;
    int rem  = bits % 32;

    for(int i = 0; i < full && i < 8; i++) {
        words[i] = (unsigned int)(rng() & 0xFFFFFFFFULL);
    }
    if(rem && full < 8) {
        words[full] = (unsigned int)(rng() & ((1ULL << rem) - 1ULL));
    }
    return uint256(words);
}

// Uniform in [0, bound) by rejection.  bound must be nonzero.
static uint256 randomBelow(std::mt19937_64 &rng, const uint256 &bound)
{
    int bits = bitLength(bound);
    if(bits == 0) {
        return uint256((uint64_t)0);
    }
    for(;;) {
        uint256 r = randomBits(rng, bits);
        if(r.cmp(bound) < 0) {
            return r;
        }
    }
}

uint256 chargeDeltaInverse(int delta)
{
    // n is odd, so inv(2) = (n+1)/2.  Charge differences are +/-1 and +/-2.
    static const uint256 invTwo = N.add((unsigned int)1).div(2);

    switch(delta) {
        case  1: return uint256((uint64_t)1);
        case -1: return negModN(uint256((uint64_t)1));
        case  2: return invTwo;
        case -2: return negModN(invTwo);
        default: throw std::string("chargeDeltaInverse: unsupported charge difference");
    }
}

/*
 * Windowed table of generator multiples.  Seeding, respawning and the final
 * verification all multiply the generator, and the naive double-and-add costs
 * about 1.5*bits point operations each.  A 4 bit window over 64 nibbles costs
 * at most 64 additions for a full scalar and rangeBits/4 for a seed offset,
 * which is what makes respawns and pool construction cheap.
 */
class GeneratorTable {
public:
    void build()
    {
        if(_built) return;
        _tbl.resize(64 * 15);
        ecpoint base = G();
        for(int i = 0; i < 64; i++) {
            ecpoint acc = base;
            for(int j = 0; j < 15; j++) {
                _tbl[i * 15 + j] = acc;
                if(j < 14) acc = addPoints(acc, base);
            }
            for(int d = 0; d < 4; d++) base = doublePoint(base);
        }
        _built = true;
    }

    ecpoint mul(const uint256 &k) const
    {
        ecpoint acc = pointAtInfinity();
        for(int i = 63; i >= 0; i--) {
            unsigned int nib = (k.v[i / 8] >> ((i % 8) * 4)) & 0xF;
            if(nib) {
                acc = isPointAtInfinity(acc) ? _tbl[i * 15 + nib - 1]
                                             : addPoints(acc, _tbl[i * 15 + nib - 1]);
            }
        }
        return acc;
    }

private:
    std::vector<ecpoint> _tbl;
    bool _built = false;
};

static GeneratorTable g_gtable;

static inline uint64_t mix64(uint64_t z)
{
    z += 0x9E3779B97F4A7C15ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

/*
 * The jump index reads words 1..6 of the x coordinate and the distinguished
 * point test reads the low bits of word 0.  Keeping the two disjoint matters:
 * if the jump index were drawn from the same bits as the DP test, every
 * distinguished point would take the same jump and the walk would degenerate.
 */
static inline int jumpIndex(const ecpoint &p, int mask)
{
    uint64_t a = ((uint64_t)p.x.v[2] << 32) | (uint64_t)p.x.v[1];
    uint64_t b = ((uint64_t)p.x.v[6] << 32) | (uint64_t)p.x.v[5];
    return (int)((mix64(a) ^ mix64(b ^ 0xD1B54A32D192ED03ULL)) & (uint64_t)mask);
}

static inline bool isDistinguished(const ecpoint &p, uint32_t dpMask)
{
    return (p.x.v[0] & dpMask) == 0;
}

/*
 * Identifier of the orbit {P,-P}, used only to recognise a repeat.  x already
 * identifies the orbit, so this is just a 64 bit digest of it; unlike the jump
 * index it may share bits with anything.
 */
static inline uint64_t orbitHash(const ecpoint &p)
{
    uint64_t h = 0;
    for(int i = 0; i < 8; i += 2) {
        h ^= mix64(((uint64_t)p.x.v[i + 1] << 32) | (uint64_t)p.x.v[i]);
    }
    return h;
}

struct XKey {
    unsigned int v[8];

    bool operator==(const XKey &o) const
    {
        for(int i = 0; i < 8; i++) {
            if(v[i] != o.v[i]) return false;
        }
        return true;
    }
};

struct XKeyHash {
    size_t operator()(const XKey &k) const
    {
        uint64_t h = 0;
        for(int i = 0; i < 8; i += 2) {
            h ^= mix64(((uint64_t)k.v[i + 1] << 32) | (uint64_t)k.v[i]);
        }
        return (size_t)h;
    }
};

struct TableEntry {
    uint256 off;
    int     charge;
};

// Cycle history is a fixed size ring so a walker stays flat and cheap to touch.
static const int MAX_CYCLE_HISTORY = 8;

struct Walker {
    ecpoint  pos;
    uint256  off;      // c + d, reduced mod n
    uint256  disp;     // displacement since seeding, reduced mod n
    int      charge;
    int      forceIdx;   // >= 0: cycle escape jump scheduled for the next step
    int      histLen;    // orbit hashes currently held
    uint64_t hist[MAX_CYCLE_HISTORY];
};

static uint256 centeredMagnitude(const uint256 &d)
{
    uint256 other = N.sub(d);
    return d.cmp(other) <= 0 ? d : other;
}

bool solve(const Config &cfg, uint256 &keyOut, Stats &statsOut)
{
    Stats st;
    util::Timer timer;
    timer.start();

    if(cfg.rangeEnd.cmp(cfg.rangeStart) < 0) {
        throw std::string("range end is below range start");
    }
    if(cfg.herdSize < 2) {
        throw std::string("herd size must be at least 2");
    }
    if(cfg.jumpCount < 2 || (cfg.jumpCount & (cfg.jumpCount - 1))) {
        throw std::string("jump count must be a power of two");
    }
    if(cfg.chargeClasses != 2 && cfg.chargeClasses != 3) {
        throw std::string("charge classes must be 2 or 3");
    }

    // Folding already produces both signs of the wild charge, so a reflected
    // seed class would only duplicate what the fold does for free.
    const bool fold = cfg.fold || cfg.gs;
    const bool gs = cfg.gs;
    if(gs && (cfg.gsWildShift < 0 || cfg.gsWildShift > 255)) {
        throw std::string("GS wild shift must be between 0 and 255");
    }
    int cycleHistory = cfg.cycleHistory;
    if(cycleHistory < 2) cycleHistory = 2;
    if(cycleHistory > MAX_CYCLE_HISTORY) cycleHistory = MAX_CYCLE_HISTORY;

    const uint256 a = cfg.rangeStart;
    const uint256 b = cfg.rangeEnd;
    const uint256 w = b.sub(a).add((unsigned int)1);   // interval width
    const int rangeBits = bitLength(w);
    st.rangeBits = rangeBits;
    uint256 wildWidth = w;
    if(gs) {
        for(int i = 0; i < cfg.gsWildShift; i++) wildWidth = wildWidth.div(2);
        if(wildWidth.isZero()) {
            throw std::string("GS wild shift leaves an empty seed window");
        }
    }

    // Mean jump ~ 2^strideBits.  Cost is flat in the stride over a wide band,
    // so sqrt(w) is a safe default.
    int strideBits = cfg.strideBits >= 0 ? cfg.strideBits : rangeBits / 2;
    if(strideBits < 1) strideBits = 1;
    st.strideBits = strideBits;

    // Auto DP: aim for roughly 2^18 stored points at the expected ~2 sqrt(w)
    // group operations, with a floor so tiny ranges still terminate.
    int dpBits = cfg.dpBits;
    if(dpBits < 0) {
        dpBits = rangeBits / 2 - 17;
        if(dpBits < 0)  dpBits = 0;
        if(dpBits > 24) dpBits = 24;
    }
    st.dpBits = dpBits;
    const uint32_t dpMask = dpBits >= 32 ? 0xFFFFFFFFu : ((1u << dpBits) - 1u);

    g_gtable.build();

    std::mt19937_64 rng(cfg.seed ? cfg.seed : (uint64_t)util::getSystemTime());

    // Jump table.  Distances are uniform in [1, 2^(strideBits+1)) so the mean
    // is about 2^strideBits.
    std::vector<uint256> jumpDist(cfg.jumpCount);
    std::vector<ecpoint> jumpPoint(cfg.jumpCount);
    for(int i = 0; i < cfg.jumpCount; i++) {
        uint256 d;
        do {
            d = randomBits(rng, strideBits + 1);
        } while(d.isZero());
        jumpDist[i]  = d;
        jumpPoint[i] = g_gtable.mul(d);
    }

    const ecpoint H = cfg.target;

    /*
     * Folding needs the interval centred on zero.  Negation acts on exponents as
     * e -> -e, so it folds the walk onto half as many orbits only if the set the
     * walk occupies is symmetric about zero; on the raw interval [a,b] it maps
     * the window somewhere near n instead and buys nothing.  Solve the shifted
     * instance H' = H - mid*G, whose logarithm x' = x - mid lies in
     * [-w/2, w/2], and add mid back at the end.
     */
    const uint256 half = w.div(2);
    const uint256 mid  = a.add(half);
    const ecpoint Heff = fold ? addPoints(H, ecpoint(g_gtable.mul(mid).x,
                                                     negModP(g_gtable.mul(mid).y)))
                              : H;
    const ecpoint negHeff = ecpoint(Heff.x, negModP(Heff.y));
    if(fold) st.setupOps += 128;

    // Reflection constant: a charge -1 walker sits at (2a + w - 1) - x + u,
    // which lands in the same window as the tame and wild walkers.  Folded runs
    // are already centred, so there the reflected class is just -x' + u.
    const uint256 reflectBase = a.add(a).add(w).sub(uint256((uint64_t)1));

    /*
     * Reseed pool.  A same-charge merge kills a walker, so respawns have to be
     * cheap or the herd pays a scalar multiplication every time one happens.
     * Precompute a pool of random (u, u*G) pairs once; a respawn then costs one
     * or two point additions instead of ~1.5*rangeBits of them.
     */
    int poolSize = cfg.poolSize > 0 ? cfg.poolSize : (cfg.herdSize * 8 < 512 ? 512 : cfg.herdSize * 8);
    std::vector<uint256> poolU(poolSize);
    std::vector<ecpoint> poolP(poolSize);
    std::vector<uint256> wildPoolU;
    std::vector<ecpoint> wildPoolP;
    for(int i = 0; i < poolSize; i++) {
        poolU[i] = randomBelow(rng, w);
        poolP[i] = g_gtable.mul(poolU[i]);
    }
    if(gs) {
        wildPoolU.resize(poolSize);
        wildPoolP.resize(poolSize);
        for(int i = 0; i < poolSize; i++) {
            wildPoolU[i] = randomBelow(rng, wildWidth);
            wildPoolP[i] = g_gtable.mul(wildPoolU[i]);
        }
    }
    // A double-and-add over a k bit scalar costs about 1.5k point operations.
    st.poolOps  = (uint64_t)poolSize * (uint64_t)(rangeBits / 4 + 1);
    if(gs) {
        st.poolOps += (uint64_t)poolSize *
                      (uint64_t)(bitLength(wildWidth) / 4 + 1);
    }
    st.setupOps = st.poolOps;
    st.poolSize = (uint64_t)poolSize;

    const ecpoint baseG        = g_gtable.mul(a);
    const ecpoint reflectAnchor = addPoints(negHeff, g_gtable.mul(reflectBase));
    st.setupOps += 128;

    /*
     * Per class seed anchor.  Each class draws `draws` offsets from the pool,
     * each uniform on [0,w), so a centred class needs its window pulled back by
     * draws*w/2; the shift is folded into the anchor point once at setup.
     */
    const int classDraws[3] = {
        cfg.spreadTame < 1 ? 1 : cfg.spreadTame,
        cfg.spreadWild < 1 ? 1 : cfg.spreadWild,
        cfg.spreadRefl < 1 ? 1 : cfg.spreadRefl
    };
    uint256 classBase[3];
    uint256 classWidth[3];
    ecpoint classAnchor[3];
    for(int c = 0; c < 3; c++) {
        const uint256 seedWidth = gs && c != 0 ? wildWidth : w;
        const uint256 seedHalf  = seedWidth.div(2);
        classWidth[c] = seedWidth.mul((uint32_t)classDraws[c]);
        if(fold) {
            uint256 shift((uint64_t)0);
            for(int d = 0; d < classDraws[c]; d++) shift = subModN(shift, seedHalf);
            classBase[c]   = shift;
            ecpoint shiftP = g_gtable.mul(shift);
            classAnchor[c] = c == 0 ? shiftP
                           : addPoints(c == 1 ? Heff : negHeff, shiftP);
            st.setupOps += 64;
        } else if(c == 0) {
            classBase[c]   = a;
            classAnchor[c] = baseG;
        } else if(c == 1) {
            classBase[c]   = uint256((uint64_t)0);
            classAnchor[c] = Heff;
        } else {
            classBase[c]   = reflectBase;
            classAnchor[c] = reflectAnchor;
        }
    }

    // Seeding.  Tame offsets are a sum of two uniforms, which spreads the only
    // class whose position we control across the window the wild classes reach.
    std::vector<Walker> herd(cfg.herdSize);

    auto seedWalker = [&](Walker &k, int charge) {
        k.charge = charge;

        int c = charge == CHARGE_TAME ? 0 : (charge == CHARGE_WILD ? 1 : 2);
        int     draws  = classDraws[c];
        uint256 base   = classBase[c];
        ecpoint anchor = classAnchor[c];
        const std::vector<uint256> &seedU = gs && c != 0 ? wildPoolU : poolU;
        const std::vector<ecpoint> &seedP = gs && c != 0 ? wildPoolP : poolP;

        uint256 off = base;
        ecpoint pos = anchor;
        for(int d = 0; d < draws; d++) {
            int i = (int)(rng() % (uint64_t)poolSize);
            off = addModN(off, seedU[i]);
            pos = addPoints(pos, seedP[i]);
        }
        st.setupOps += (uint64_t)draws;
        k.off      = off;
        k.disp     = uint256((uint64_t)0);
        k.pos      = pos;
        k.forceIdx = -1;
        k.histLen  = 0;
    };

    /*
     * Move a walker onto the even-y representative of its orbit, carrying the
     * bookkeeping with it.  Every point that can reach the table has to pass
     * through here: the table is keyed on x, so an odd-y entry and an even-y
     * entry with the same x read as a collision whose exponents differ by a
     * sign, which recovers a wrong key.
     */
    auto canonicalise = [&](Walker &k) {
        if(k.pos.y.v[0] & 1) {
            k.pos    = ecpoint(k.pos.x, negModP(k.pos.y));
            k.charge = -k.charge;
            k.off    = negModN(k.off);
            k.disp   = negModN(k.disp);
            st.foldNegations++;
        }
    };

    // Build the charge pattern.  Interleaving rather than blocking keeps class
    // members from being adjacent in the round robin.
    int mt = cfg.mixTame, mw = cfg.mixWild, mr = cfg.mixRefl;
    if(mt + mw + mr == 0) {
        mt = 1;
        mw = 1;
        mr = (cfg.chargeClasses == 3) ? 1 : 0;
    }
    if(cfg.chargeClasses == 2) {
        mr = 0;
    }
    if(mt == 0 || mw == 0) {
        throw std::string("herd needs at least one tame and one wild class member");
    }

    std::vector<int> pattern;
    for(int i = 0; i < mt; i++) pattern.push_back(CHARGE_TAME);
    for(int i = 0; i < mw; i++) pattern.push_back(CHARGE_WILD);
    for(int i = 0; i < mr; i++) pattern.push_back(CHARGE_REFLECTED);

    for(int j = 0; j < cfg.herdSize; j++) {
        seedWalker(herd[j], pattern[j % (int)pattern.size()]);
    }

    std::unordered_map<XKey, TableEntry, XKeyHash> table;
    table.reserve(1u << 16);

    std::vector<ecpoint> points(cfg.herdSize);
    std::vector<ecpoint> addends(cfg.herdSize);
    std::vector<int>     idx(cfg.herdSize);

    bool found = false;
    uint256 recovered;

    while(!found) {
        if(cfg.maxSteps && st.steps >= cfg.maxSteps) {
            break;
        }

        for(int j = 0; j < cfg.herdSize; j++) {
            // The jump index reads x only, so mirrored walkers agree on it.
            idx[j] = herd[j].forceIdx >= 0 ? herd[j].forceIdx
                                           : jumpIndex(herd[j].pos, cfg.jumpCount - 1);
            herd[j].forceIdx = -1;
            points[j]  = herd[j].pos;
            addends[j] = jumpPoint[idx[j]];
        }

        // One modular inversion per worker chunk instead of one per addition.
        addPointsIndependent(points, addends, cfg.threads);

        for(int j = 0; j < cfg.herdSize; j++) {
            herd[j].pos = points[j];
            herd[j].off = addModN(herd[j].off, jumpDist[idx[j]]);
            herd[j].disp = addModN(herd[j].disp, jumpDist[idx[j]]);
        }
        st.steps += (uint64_t)cfg.herdSize;

        if(fold) {
            for(int j = 0; j < cfg.herdSize; j++) {
                Walker &k = herd[j];

                /*
                 * Canonicalise before the distinguished point test, not after
                 * the jump index: x already identifies the orbit, but the
                 * exponent stored in the table has to belong to the same
                 * representative that other walkers will store, or the two
                 * readings differ by a sign and the collision is lost.
                 */
                canonicalise(k);

                if(gs) {
                    int c = k.charge == CHARGE_TAME ? 0 :
                            (k.charge == CHARGE_WILD ? 1 : 2);
                    if(centeredMagnitude(k.disp).cmp(classWidth[c]) > 0) {
                        st.gsRestarts++;
                        seedWalker(k, k.charge);
                        canonicalise(k);
                        continue;
                    }
                }

                uint64_t h = orbitHash(k.pos);

                // A jump distance is never zero, so a repeated orbit means the
                // canonical choice sent the walker back: a fruitless cycle.
                bool cycle = false;
                for(int i = 0; i < k.histLen; i++) {
                    if(k.hist[i] == h) { cycle = true; break; }
                }

                if(cycle) {
                    st.cycleEvents++;
                    /*
                     * Escape on a jump derived from the smallest orbit hash in
                     * the cycle.  That representative is a property of the cycle
                     * and not of the walker, so two walkers caught in the same
                     * cycle leave it in the same direction and stay merged,
                     * which is the whole point of folding.
                     */
                    uint64_t anchor = h;
                    for(int i = 0; i < k.histLen; i++) {
                        if(k.hist[i] < anchor) anchor = k.hist[i];
                    }
                    int esc = (int)(mix64(anchor ^ 0x2545F4914F6CDD1DULL)
                                    & (uint64_t)(cfg.jumpCount - 1));
                    // The escape must differ from the jump the walk would take
                    // anyway, or the cycle repeats for ever.
                    if(esc == jumpIndex(k.pos, cfg.jumpCount - 1)) {
                        esc = (esc + 1) & (cfg.jumpCount - 1);
                    }
                    k.forceIdx = esc;
                    k.histLen  = 0;
                } else if(k.histLen < cycleHistory) {
                    k.hist[k.histLen++] = h;
                } else {
                    for(int i = 1; i < cycleHistory; i++) k.hist[i - 1] = k.hist[i];
                    k.hist[cycleHistory - 1] = h;
                }
            }
        }

        for(int j = 0; j < cfg.herdSize && !found; j++) {
            if(!isDistinguished(herd[j].pos, dpMask)) {
                continue;
            }
            st.dpHits++;

            XKey key;
            herd[j].pos.x.exportWords(key.v, 8);

            auto it = table.find(key);
            if(it == table.end()) {
                TableEntry e;
                e.off    = herd[j].off;
                e.charge = herd[j].charge;
                table.emplace(key, e);
                continue;
            }

            int delta = it->second.charge - herd[j].charge;
            if(delta == 0) {
                // Equal charges cancel x.  The two walkers are now on the same
                // trajectory forever, so one of them is dead weight: reseed it.
                st.sameChargeMerges++;
                seedWalker(herd[j], herd[j].charge);
                continue;
            }

            // (k1 - k2) * x = off2 - off1
            uint256 num = subModN(herd[j].off, it->second.off);
            uint256 x   = multiplyModN(num, chargeDeltaInverse(delta));

            st.productiveCollisions++;
            if(it->second.charge * herd[j].charge == 0) {
                if(it->second.charge == CHARGE_WILD || herd[j].charge == CHARGE_WILD) st.pairTameWild++;
                else st.pairTameRefl++;
            } else {
                st.pairWildRefl++;
            }

            if(g_gtable.mul(x) == Heff) {
                // Undo the centring shift, then check the answer against the
                // public key the caller actually asked about.
                uint256 cand = fold ? addModN(x, mid) : x;
                if(fold && !(g_gtable.mul(cand) == H)) {
                    throw std::string("internal error: shifted solution does not match the target");
                }
                recovered = cand;
                found = true;
            } else {
                st.verifyFailures++;
                if(!cfg.quiet) {
                    Logger::log(LogLevel::Warning, "collision failed verification, continuing");
                }
            }
        }
    }

    st.tableSize = (uint64_t)table.size();
    st.seconds   = (double)timer.getTime() / 1000.0;

    statsOut = st;
    if(found) {
        keyOut = recovered;
    }
    return found;
}

}
