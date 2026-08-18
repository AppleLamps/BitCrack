#ifndef _KANGAROO_H
#define _KANGAROO_H

#include <stdint.h>
#include <string>
#include <vector>

#include "secp256k1.h"

/*
 * Charge-balanced herd kangaroo for the interval discrete logarithm problem.
 *
 * Seed walker j at  h^{k_j} * g^{c_j}.  Its exponent is
 *
 *      p_j(t) = k_j * x + c_j + d_j(t)
 *
 * where d_j is the accumulated jump distance.  A jump multiplies by g^s, so it
 * moves d_j and leaves k_j fixed: the charge k_j is a conserved quantity of the
 * walk, assigned once at seeding.
 *
 * When walkers i and j collide,
 *
 *      (k_i - k_j) * x = (c_j + d_j) - (c_i + d_i)   (mod n)
 *
 * which determines x if and only if k_i != k_j.  Equal charges cancel x and
 * yield nothing, so a herd split 50/50 between tame (k=0) and wild (k=+1)
 * kangaroos wastes half of every collision it produces.
 *
 * An interval of width w admits charges with |k| <= 1, because a charge-k
 * walker occupies a window of width |k|*w.  Equivalently the affine maps
 * preserving an interval are the identity and the reflection x -> (a+b)-x, so
 * the usable charge set is {0, +1, -1}.  Balancing the herd across all three
 * raises the productive share of collisions from 1/2 to 2/3.
 */
namespace kangaroo {

enum Charge {
    CHARGE_TAME      =  0,   // g^c
    CHARGE_WILD      =  1,   // h * g^c
    CHARGE_REFLECTED = -1    // h^{-1} * g^{a+b+c}
};

struct Config {
    secp256k1::ecpoint target;        // H = x*G
    secp256k1::uint256 rangeStart;    // a
    secp256k1::uint256 rangeEnd;      // b, inclusive

    int      herdSize      = 96;      // number of walkers
    int      chargeClasses = 3;       // 2 = classic tame/wild, 3 = charge balanced
    // Explicit herd composition.  Zeroes mean "derive from chargeClasses".
    // The measured overlap weights are not equal across charge pairs, so the
    // cost optimum sits slightly tame-heavy of the balanced 1:1:1 point.
    int      mixTame       = 0;
    int      mixWild       = 0;
    int      mixRefl       = 0;
    int      dpBits        = -1;      // -1 = auto
    int      jumpCount     = 32;      // size of the jump table, power of two
    int      strideBits    = -1;      // -1 = auto (mean jump ~ sqrt(w))
    int      threads       = 1;
    int      poolSize      = 0;       // 0 = auto (reseed pool for cheap respawns)
    /*
     * Seed spread per charge class, in draws from the offset pool.  One draw
     * gives a window of width w, two draws give a triangular window of width
     * 2w.  Only the tame class has a position independent of x, so it is the
     * class that bridges the two wild windows; widening the reflected class
     * trades density for overlap with the wild class.
     */
    int      spreadTame    = 2;
    int      spreadWild    = 1;
    int      spreadRefl    = 1;
    uint64_t seed          = 0;       // 0 = nondeterministic
    uint64_t maxSteps      = 0;       // 0 = unlimited
    bool     quiet         = false;
};

struct Stats {
    uint64_t steps                 = 0;   // walk group operations (the theory metric)
    uint64_t setupOps              = 0;   // seeding and reseeding cost, in point ops
    uint64_t poolSize              = 0;   // precomputed reseed pool entries
    uint64_t dpHits                = 0;   // distinguished points reached
    uint64_t sameChargeMerges      = 0;   // collisions that cancelled x
    uint64_t productiveCollisions  = 0;   // collisions that determined x
    uint64_t pairTameWild          = 0;   // solving collision by charge pair
    uint64_t pairTameRefl          = 0;
    uint64_t pairWildRefl          = 0;
    uint64_t verifyFailures        = 0;   // must stay 0
    uint64_t tableSize             = 0;
    double   seconds               = 0.0;
    int      dpBits                = 0;
    int      strideBits            = 0;
    int      rangeBits             = 0;         // ceil(log2(width))

    uint64_t totalOps() const { return steps + setupOps; }
};

// Returns true and sets keyOut when the key is found and verified against the
// target point.  Returns false if maxSteps was reached first.
bool solve(const Config &cfg, secp256k1::uint256 &keyOut, Stats &statsOut);

// Bit length of a 256 bit value; 0 for zero.
int bitLength(const secp256k1::uint256 &v);

// Multiplicative inverse of a charge difference in {-2,-1,1,2}, mod n.
secp256k1::uint256 chargeDeltaInverse(int delta);

}

#endif
