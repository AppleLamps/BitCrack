/*
 * Kangaroo: charge-balanced herd solver for the interval discrete logarithm
 * problem on secp256k1.
 *
 * This complements BitCrack rather than replacing it.  BitCrack searches a
 * keyspace against address hashes and needs only the hash160.  A kangaroo walk
 * needs the target public key, and in exchange costs about 2*sqrt(w) group
 * operations instead of w.
 */
#include <stdio.h>
#include <math.h>
#include <string>
#include <vector>
#include <random>

#include "Kangaroo.h"
#include "CmdParse.h"
#include "Logger.h"
#include "util.h"
#include "secp256k1.h"

static void usage()
{
    printf("Kangaroo: interval discrete logarithm solver (charge-balanced herd)\n\n");
    printf("Usage: kangaroo -k <pubkey> [--range a:b | --bits N] [options]\n\n");
    printf("  -k, --pubkey <hex>     Target public key, compressed or uncompressed\n");
    printf("      --range <a:b>      Hex search interval, inclusive\n");
    printf("      --bits <N>         Shorthand for the range [2^(N-1), 2^N - 1]\n");
    printf("      --charges <2|3>    2 = classic tame/wild herd, 3 = charge balanced (default 3)\n");
    printf("      --herd <N>         Walkers in the herd (default 96)\n");
    printf("      --pool <N>         Reseed pool size (default auto, 8x herd)\n");
    printf("      --spread <TWR>     Seed spread in pool draws per class (default 211)\n");
    printf("      --mix <T:W:R>      Herd composition by charge (default 1:1:1)\n");
    printf("  -t, --threads <N>      Worker threads for the batch point add (default 1)\n");
    printf("  -d, --dp-bits <N>      Distinguished point bits (default auto)\n");
    printf("      --stride-bits <N>  log2 of the mean jump distance (default auto, log2 sqrt(w))\n");
    printf("      --jumps <N>        Jump table size, power of two (default 32)\n");
    printf("      --seed <N>         PRNG seed for reproducible runs\n");
    printf("      --max-steps <N>    Abort after N group operations\n");
    printf("      --benchmark <N>    Run N random solves per arm and A/B the charge sets\n");
    printf("  -o, --out <file>       Append the found key to a file\n");
    printf("  -h, --help             This message\n\n");
    printf("Examples:\n");
    printf("  kangaroo -k 02145d2611c823a396ef6712ce0f712f09b9b4f3135e3e0aa3230fb9b6d08d1e16 --bits 40 -t 8\n");
    printf("  kangaroo --bits 32 --benchmark 40 -t 4\n");
}

static bool parseRange(const std::string &s, secp256k1::uint256 &a, secp256k1::uint256 &b)
{
    size_t colon = s.find(':');
    if(colon == std::string::npos) {
        return false;
    }
    a = secp256k1::uint256(s.substr(0, colon));
    b = secp256k1::uint256(s.substr(colon + 1));
    return true;
}

static double sqrtWidth(const secp256k1::uint256 &w, int rangeBits)
{
    secp256k1::uint256 t = w;
    if(rangeBits <= 63) {
        return sqrt((double)t.toUint64());
    }
    return ldexp(1.0, rangeBits / 2.0);
}

static void printStats(const char *label, const kangaroo::Stats &st, double sqrtW)
{
    printf("  %-18s steps=%-14s  %.4f*sqrt(w)   dp=%-9llu merges=%-7llu table=%-9llu  %.1fs  %s ops/s\n",
           label,
           util::formatThousands(st.steps).c_str(),
           (double)st.steps / sqrtW,
           (unsigned long long)st.dpHits,
           (unsigned long long)st.sameChargeMerges,
           (unsigned long long)st.tableSize,
           st.seconds,
           util::formatThousands(st.seconds > 0 ? (uint64_t)(st.steps / st.seconds) : 0).c_str());
}

static int runBenchmark(kangaroo::Config cfg, int trials)
{
    secp256k1::uint256 w = cfg.rangeEnd.sub(cfg.rangeStart).add((unsigned int)1);
    int rangeBits = kangaroo::bitLength(w);
    double sqrtW  = sqrtWidth(w, rangeBits);

    printf("\nBenchmark: %d paired trials, range 2^%d, herd %d, threads %d\n",
           trials, rangeBits, cfg.herdSize, cfg.threads);
    printf("Each trial solves the SAME random key with both herds.\n\n");

    double sum[2]  = { 0.0, 0.0 };
    double sum2[2] = { 0.0, 0.0 };
    double merges[2] = { 0.0, 0.0 };
    double ptw[2] = {0,0}, ptr[2] = {0,0}, pwr[2] = {0,0};
    uint64_t vfail = 0;
    int    ok[2]   = { 0, 0 };
    std::mt19937_64 rng(cfg.seed ? cfg.seed : (uint64_t)util::getSystemTime());

    for(int t = 0; t < trials; t++) {
        // Same key and same herd seed for both arms, so the only difference
        // between them is the charge composition.
        secp256k1::uint256 span = w;
        secp256k1::uint256 x;
        for(;;) {
            unsigned int words[8] = { 0 };
            int bits = rangeBits;
            for(int i = 0; i < 8 && bits > 0; i++) {
                unsigned int m = bits >= 32 ? 0xFFFFFFFFu : ((1u << bits) - 1u);
                words[i] = (unsigned int)(rng() & 0xFFFFFFFFULL) & m;
                bits -= 32;
            }
            x = secp256k1::uint256(words);
            if(x.cmp(span) < 0) break;
        }
        x = secp256k1::addModN(cfg.rangeStart, x);

        kangaroo::Config c = cfg;
        c.target  = secp256k1::multiplyPoint(x, secp256k1::G());
        c.seed    = rng();
        c.quiet   = true;

        for(int arm = 0; arm < 2; arm++) {
            kangaroo::Config ac = c;
            ac.chargeClasses = (arm == 0) ? 2 : 3;
            secp256k1::uint256 key;
            kangaroo::Stats st;
            if(kangaroo::solve(ac, key, st) && key == x) {
                sum[arm]    += (double)st.steps;
                sum2[arm]   += (double)st.steps * (double)st.steps;
                merges[arm] += (double)st.sameChargeMerges;
                ptw[arm] += (double)st.pairTameWild;
                ptr[arm] += (double)st.pairTameRefl;
                pwr[arm] += (double)st.pairWildRefl;
                vfail    += st.verifyFailures;
                ok[arm]++;
            }
        }

        if((t + 1) % 5 == 0 || t + 1 == trials) {
            double m0 = ok[0] ? sum[0] / ok[0] / sqrtW : 0.0;
            double m1 = ok[1] ? sum[1] / ok[1] / sqrtW : 0.0;
            printf("\r  %3d/%d   2-charge %.4f*sqrt(w)   3-charge %.4f*sqrt(w)   speedup %.4f   ",
                   t + 1, trials, m0, m1, m1 > 0 ? m0 / m1 : 0.0);
            fflush(stdout);
        }
    }
    printf("\n\n");

    const char *names[2] = { "2-charge {0,+1}   ", "3-charge {0,+1,-1}" };
    for(int arm = 0; arm < 2; arm++) {
        if(!ok[arm]) continue;
        double mean = sum[arm] / ok[arm];
        double var  = sum2[arm] / ok[arm] - mean * mean;
        double se   = sqrt(var / ok[arm]);
        printf("  %s  %.4f +/- %.4f sqrt(w)   merges/solve %.2f   n=%d\n",
               names[arm], mean / sqrtW, se / sqrtW, merges[arm] / ok[arm], ok[arm]);
    }
    printf("\n  solving collision by charge pair:\n");
    for(int arm = 0; arm < 2; arm++) {
        if(!ok[arm]) continue;
        printf("    %s  tame/wild %.3f   tame/refl %.3f   wild/refl %.3f\n",
               names[arm], ptw[arm] / ok[arm], ptr[arm] / ok[arm], pwr[arm] / ok[arm]);
    }
    printf("  verification failures: %llu (must be 0)\n", (unsigned long long)vfail);
    if(ok[0] && ok[1]) {
        double m0 = sum[0]/ok[0], m1 = sum[1]/ok[1];
        double se0 = sqrt((sum2[0]/ok[0] - m0*m0)/ok[0]) / m0;
        double se1 = sqrt((sum2[1]/ok[1] - m1*m1)/ok[1]) / m1;
        double r = m0 / m1;
        printf("\n  speedup = %.4f +/- %.4f      predicted ceiling sqrt(4/3) = %.4f\n",
               r, r * sqrt(se0*se0 + se1*se1), sqrt(4.0 / 3.0));
    }
    return 0;
}

int main(int argc, char **argv)
{
    kangaroo::Config cfg;
    std::string pubKeyStr;
    std::string outFile;
    int benchmark = 0;
    bool haveRange = false;

    CmdParse parser;
    parser.add("-k", "--pubkey", true);
    parser.add("",   "--range", true);
    parser.add("",   "--bits", true);
    parser.add("",   "--charges", true);
    parser.add("",   "--herd", true);
    parser.add("",   "--pool", true);
    parser.add("",   "--spread", true);
    parser.add("",   "--mix", true);
    parser.add("-t", "--threads", true);
    parser.add("-d", "--dp-bits", true);
    parser.add("",   "--stride-bits", true);
    parser.add("",   "--jumps", true);
    parser.add("",   "--seed", true);
    parser.add("",   "--max-steps", true);
    parser.add("",   "--benchmark", true);
    parser.add("-o", "--out", true);
    parser.add("-h", "--help", false);

    if(argc == 1) {
        usage();
        return 0;
    }

    try {
        parser.parse(argc, argv);
    } catch(std::string err) {
        printf("Error: %s\n", err.c_str());
        return 1;
    }

    try {
        for(OptArg &a : parser.getArgs()) {
            if(a.equals("-h", "--help")) { usage(); return 0; }
            else if(a.equals("-k", "--pubkey"))    pubKeyStr = a.arg;
            else if(a.equals("", "--range"))       { if(!parseRange(a.arg, cfg.rangeStart, cfg.rangeEnd)) throw std::string("range must be a:b"); haveRange = true; }
            else if(a.equals("", "--bits")) {
                int n = (int)util::parseUInt32(a.arg);
                if(n < 2 || n > 256) throw std::string("--bits must be between 2 and 256");
                secp256k1::uint256 lo((uint64_t)0);
                unsigned int wlo[8] = { 0 }, whi[8] = { 0 };
                wlo[(n - 1) / 32] = 1u << ((n - 1) % 32);
                for(int i = 0; i < n; i++) whi[i / 32] |= 1u << (i % 32);
                cfg.rangeStart = secp256k1::uint256(wlo);
                cfg.rangeEnd   = secp256k1::uint256(whi);
                haveRange = true;
            }
            else if(a.equals("", "--charges"))     cfg.chargeClasses = (int)util::parseUInt32(a.arg);
            else if(a.equals("", "--herd"))        cfg.herdSize      = (int)util::parseUInt32(a.arg);
            else if(a.equals("", "--pool"))        cfg.poolSize      = (int)util::parseUInt32(a.arg);
            else if(a.equals("", "--mix")) {
                // T:W:R relative herd composition
                std::string v = a.arg;
                size_t c1 = v.find(':'), c2 = v.rfind(':');
                if(c1 == std::string::npos || c1 == c2) throw std::string("--mix takes T:W:R, e.g. 3:2:2");
                cfg.mixTame = (int)util::parseUInt32(v.substr(0, c1));
                cfg.mixWild = (int)util::parseUInt32(v.substr(c1 + 1, c2 - c1 - 1));
                cfg.mixRefl = (int)util::parseUInt32(v.substr(c2 + 1));
            }
            else if(a.equals("", "--spread")) {
                // three digits: tame,wild,reflected seed spread in pool draws
                std::string v = a.arg;
                if(v.size() != 3) throw std::string("--spread takes three digits, e.g. 212");
                cfg.spreadTame = v[0] - '0';
                cfg.spreadWild = v[1] - '0';
                cfg.spreadRefl = v[2] - '0';
            }
            else if(a.equals("-t", "--threads"))   cfg.threads       = (int)util::parseUInt32(a.arg);
            else if(a.equals("-d", "--dp-bits"))   cfg.dpBits        = (int)util::parseUInt32(a.arg);
            else if(a.equals("", "--stride-bits")) cfg.strideBits    = (int)util::parseUInt32(a.arg);
            else if(a.equals("", "--jumps"))       cfg.jumpCount     = (int)util::parseUInt32(a.arg);
            else if(a.equals("", "--seed"))        cfg.seed          = util::parseUInt64(a.arg);
            else if(a.equals("", "--max-steps"))   cfg.maxSteps      = util::parseUInt64(a.arg);
            else if(a.equals("", "--benchmark"))   benchmark         = (int)util::parseUInt32(a.arg);
            else if(a.equals("-o", "--out"))       outFile           = a.arg;
        }
    } catch(std::string err) {
        printf("Error: %s\n", err.c_str());
        return 1;
    }

    if(!haveRange) {
        printf("Error: specify --range a:b or --bits N\n");
        return 1;
    }

    if(benchmark > 0) {
        try {
            return runBenchmark(cfg, benchmark);
        } catch(std::string err) {
            printf("Error: %s\n", err.c_str());
            return 1;
        }
    }

    if(pubKeyStr.empty()) {
        printf("Error: --pubkey is required unless running --benchmark\n");
        return 1;
    }

    try {
        cfg.target = secp256k1::parsePublicKey(pubKeyStr);
    } catch(std::string err) {
        printf("Error: could not parse public key: %s\n", err.c_str());
        return 1;
    }

    secp256k1::uint256 w = cfg.rangeEnd.sub(cfg.rangeStart).add((unsigned int)1);
    int rangeBits = kangaroo::bitLength(w);
    double sqrtW  = sqrtWidth(w, rangeBits);

    printf("Target      : %s\n", cfg.target.toString(true).c_str());
    printf("Range       : %s : %s  (2^%d)\n",
           cfg.rangeStart.toString().c_str(), cfg.rangeEnd.toString().c_str(), rangeBits);
    printf("Herd        : %d walkers, %d charge classes%s\n",
           cfg.herdSize, cfg.chargeClasses,
           cfg.chargeClasses == 3 ? " {0,+1,-1}" : " {0,+1}");
    printf("Expected    : ~%s group operations\n\n",
           util::formatThousands((uint64_t)(2.1 * sqrtW)).c_str());

    secp256k1::uint256 key;
    kangaroo::Stats st;
    bool found = false;
    try {
        found = kangaroo::solve(cfg, key, st);
    } catch(std::string err) {
        printf("Error: %s\n", err.c_str());
        return 1;
    }

    if(!found) {
        printf("No key found within the step limit.\n");
        printStats("stats", st, sqrtW);
        return 1;
    }

    printf("FOUND  private key : %s\n\n", key.toString().c_str());
    printStats("stats", st, sqrtW);

    if(!outFile.empty()) {
        util::appendToFile(outFile, key.toString() + " " + cfg.target.toString(true) + "\n");
        printf("Written to %s\n", outFile.c_str());
    }
    return 0;
}
