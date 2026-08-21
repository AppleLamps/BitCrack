#include "CpuKeySearchDevice.h"

#include "AddressUtil.h"
#include "CryptoUtil.h"
#include "Logger.h"
#include "util.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <thread>

static inline void uint256ToLimbs(const secp256k1::uint256 &a, uint64_t out[4])
{
    out[0] = (uint64_t)a.v[0] | ((uint64_t)a.v[1] << 32);
    out[1] = (uint64_t)a.v[2] | ((uint64_t)a.v[3] << 32);
    out[2] = (uint64_t)a.v[4] | ((uint64_t)a.v[5] << 32);
    out[3] = (uint64_t)a.v[6] | ((uint64_t)a.v[7] << 32);
}

static inline void limbsToUint256(const uint64_t in[4], secp256k1::uint256 &a)
{
    a.v[0] = (unsigned int)in[0];
    a.v[1] = (unsigned int)(in[0] >> 32);
    a.v[2] = (unsigned int)in[1];
    a.v[3] = (unsigned int)(in[1] >> 32);
    a.v[4] = (unsigned int)in[2];
    a.v[5] = (unsigned int)(in[2] >> 32);
    a.v[6] = (unsigned int)in[3];
    a.v[7] = (unsigned int)(in[3] >> 32);
}

static inline secp256k1::ecpoint limbsToPoint(const uint64_t x[4], const uint64_t y[4])
{
    secp256k1::uint256 ux, uy;
    limbsToUint256(x, ux);
    limbsToUint256(y, uy);
    return secp256k1::ecpoint(ux, uy);
}

static inline void limbsToBeWords(const uint64_t n[4], unsigned int words[8])
{
    words[0] = (unsigned int)(n[3] >> 32);
    words[1] = (unsigned int)n[3];
    words[2] = (unsigned int)(n[2] >> 32);
    words[3] = (unsigned int)n[2];
    words[4] = (unsigned int)(n[1] >> 32);
    words[5] = (unsigned int)n[1];
    words[6] = (unsigned int)(n[0] >> 32);
    words[7] = (unsigned int)n[0];
}

static inline unsigned int endian32(unsigned int x)
{
#if defined(__GNUC__)
    return __builtin_bswap32(x);
#else
    return (x << 24) | ((x << 8) & 0x00ff0000) | ((x >> 8) & 0x0000ff00) | (x >> 24);
#endif
}

static void fillCompressedShaMsg(const uint64_t x[4], uint64_t yOdd, unsigned int msg[16])
{
    const unsigned int x0 = (unsigned int)x[0];
    const unsigned int x1 = (unsigned int)(x[0] >> 32);
    const unsigned int x2 = (unsigned int)x[1];
    const unsigned int x3 = (unsigned int)(x[1] >> 32);
    const unsigned int x4 = (unsigned int)x[2];
    const unsigned int x5 = (unsigned int)(x[2] >> 32);
    const unsigned int x6 = (unsigned int)x[3];
    const unsigned int x7 = (unsigned int)(x[3] >> 32);

    msg[15] = 33 * 8;
    msg[8] = (x0 << 24) | 0x00800000;
    msg[7] = (x0 >> 8) | (x1 << 24);
    msg[6] = (x1 >> 8) | (x2 << 24);
    msg[5] = (x2 >> 8) | (x3 << 24);
    msg[4] = (x3 >> 8) | (x4 << 24);
    msg[3] = (x4 >> 8) | (x5 << 24);
    msg[2] = (x5 >> 8) | (x6 << 24);
    msg[1] = (x6 >> 8) | (x7 << 24);
    msg[0] = (x7 >> 8) | (yOdd ? 0x03000000u : 0x02000000u);
    msg[9] = 0;
    msg[10] = 0;
    msg[11] = 0;
    msg[12] = 0;
    msg[13] = 0;
    msg[14] = 0;
}

static void shaDigestToRipemdMsg(const unsigned int sha256Digest[8], unsigned int msg[16])
{
    msg[0] = endian32(sha256Digest[0]);
    msg[1] = endian32(sha256Digest[1]);
    msg[2] = endian32(sha256Digest[2]);
    msg[3] = endian32(sha256Digest[3]);
    msg[4] = endian32(sha256Digest[4]);
    msg[5] = endian32(sha256Digest[5]);
    msg[6] = endian32(sha256Digest[6]);
    msg[7] = endian32(sha256Digest[7]);
    msg[8] = 0x00000080;
    msg[9] = 0;
    msg[10] = 0;
    msg[11] = 0;
    msg[12] = 0;
    msg[13] = 0;
    msg[14] = 256;
    msg[15] = 0;
}

static void hashPublicKeyCompressedLimbs(const uint64_t x[4], uint64_t yOdd, unsigned int digest[5])
{
    unsigned int msg[16];
    unsigned int sha256Digest[8];

    fillCompressedShaMsg(x, yOdd, msg);
    crypto::sha256Init(sha256Digest);
    crypto::sha256(msg, sha256Digest);
    shaDigestToRipemdMsg(sha256Digest, msg);
    crypto::ripemd160(msg, digest);
}

#ifdef _OPENMP
#include <omp.h>
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

CpuKeySearchDevice::CpuKeySearchDevice(int threads, int pointsPerThread, int blocks)
{
    if(threads <= 0) {
        throw KeySearchException("At least 1 thread required");
    }

    if(pointsPerThread <= 0) {
        throw KeySearchException("At least 1 point per thread required");
    }

    if(blocks <= 0) {
        blocks = 1;
    }

    _threads = threads;
    _blocks = blocks;
    _pointsPerThread = pointsPerThread;
    _iterations = 0;
    _singleTarget = false;
    _clipToEnd = false;
    _hammingEnabled = false;
    _hammingMinOnes = 0;
    _hammingMaxOnes = 0;

    unsigned int hw = std::thread::hardware_concurrency();
    _deviceName = "CPU";
    if(hw > 0) {
        _deviceName += " (" + util::format(hw) + " cores)";
    }
}

void CpuKeySearchDevice::init(const secp256k1::uint256 &start, int compression, const secp256k1::uint256 &stride)
{
    if(start.cmp(secp256k1::N) >= 0) {
        throw KeySearchException("Starting key is out of range");
    }

    _startExponent = start;
    _compression = compression;
    _stride = stride;
    _iterations = 0;

    uint64_t totalPoints = keysPerStep();

    Logger::log(LogLevel::Info, "Generating " + util::formatThousands(totalPoints) + " starting points on CPU");

    secp256k1::ecpoint g = secp256k1::G();

    std::vector<secp256k1::uint256> exponents((size_t)totalPoints);
    exponents[0] = _startExponent;
    for(uint64_t i = 1; i < totalPoints; i++) {
        exponents[(size_t)i] = exponents[(size_t)i - 1].add(_stride);
    }

    std::vector<secp256k1::ecpoint> points;
    secp256k1::generateKeyPairsBulk(g, exponents, points);

    const size_t n = points.size();
    _fx.resize(n * 4);
    _fy.resize(n * 4);
    for(size_t i = 0; i < n; i++) {
        uint256ToLimbs(points[i].x, &_fx[i * 4]);
        uint256ToLimbs(points[i].y, &_fy[i * 4]);
    }

    secp256k1::ecpoint step = secp256k1::multiplyPoint(secp256k1::uint256(totalPoints) * _stride, g);
    uint256ToLimbs(step.x, _stepQx);
    uint256ToLimbs(step.y, _stepQy);

    if(crypto::sha256UsesHardware()) {
        Logger::log(LogLevel::Info, "SHA-256: hardware SHA-NI 4-way");
    } else {
        Logger::log(LogLevel::Info, "SHA-256: software");
    }

    if(crypto::ripemd160UsesAvx512()) {
        Logger::log(LogLevel::Info, "RIPEMD-160: AVX-512 16-way");
    } else if(crypto::ripemd160UsesAvx2()) {
        Logger::log(LogLevel::Info, "RIPEMD-160: AVX2 8-way");
    } else {
        Logger::log(LogLevel::Info, "RIPEMD-160: software");
    }

    Logger::log(LogLevel::Info, "Done");
}

void CpuKeySearchDevice::setTargets(const std::set<KeySearchTarget> &targets)
{
    _targets = targets;
    _singleTarget = false;

    if(targets.size() == 1) {
        _singleTarget = true;
        memcpy(_singleTargetHash, targets.begin()->value, sizeof(_singleTargetHash));
    }
}

void CpuKeySearchDevice::setEndKey(const secp256k1::uint256 &endKey)
{
    _endKey = endKey;
    _clipToEnd = true;
}

void CpuKeySearchDevice::setHammingFilter(int minOnes, int maxOnes)
{
    if(minOnes < 0 || maxOnes < minOnes) {
        throw KeySearchException("Invalid Hamming filter bounds");
    }
    _hammingEnabled = true;
    _hammingMinOnes = minOnes;
    _hammingMaxOnes = maxOnes;
}

uint32_t CpuKeySearchDevice::top7HexBits(const secp256k1::uint256 &k)
{
    // Top 7 hex digits (28 bits) of the puzzle-71 zero-padded 18-hex representation.
    return ((k.v[2] & 0xFFu) << 20) | (k.v[1] >> 12);
}

int CpuKeySearchDevice::popcount28(uint32_t x)
{
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_popcount(x);
#else
    int n = 0;
    while(x) {
        n += x & 1;
        x >>= 1;
    }
    return n;
#endif
}

bool CpuKeySearchDevice::passesHammingFilter(uint64_t index)
{
    if(!_hammingEnabled) {
        return true;
    }
    const int ones = popcount28(top7HexBits(privateKeyAtIndex(index)));
    return ones >= _hammingMinOnes && ones <= _hammingMaxOnes;
}

secp256k1::uint256 CpuKeySearchDevice::privateKeyAtIndex(uint64_t index)
{
    secp256k1::uint256 offset = (secp256k1::uint256(keysPerStep()) * _iterations + secp256k1::uint256(index)) * _stride;
    return secp256k1::addModN(_startExponent, offset);
}

uint64_t CpuKeySearchDevice::keysToHashThisStep()
{
    uint64_t n = keysPerStep();
    if(!_clipToEnd) {
        return n;
    }

    secp256k1::uint256 next = getNextKey();
    if(next.cmp(_endKey) > 0) {
        return 0;
    }

    uint64_t lo = 0;
    uint64_t hi = n;
    while(lo < hi) {
        uint64_t mid = lo + (hi - lo) / 2;
        secp256k1::uint256 key = next + secp256k1::uint256(mid) * _stride;
        if(key.cmp(_endKey) > 0) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }

    return lo;
}

bool CpuKeySearchDevice::checkAndRecord(uint64_t index, bool compressed, const unsigned int digest[5])
{
    if(_singleTarget) {
#if defined(__GNUC__)
        if(__builtin_expect(digest[0] != _singleTargetHash[0], 1) ||
           digest[1] != _singleTargetHash[1] ||
           digest[2] != _singleTargetHash[2] ||
           digest[3] != _singleTargetHash[3] ||
           digest[4] != _singleTargetHash[4]) {
            return false;
        }
#else
        if(digest[0] != _singleTargetHash[0] || digest[1] != _singleTargetHash[1] ||
           digest[2] != _singleTargetHash[2] || digest[3] != _singleTargetHash[3] ||
           digest[4] != _singleTargetHash[4]) {
            return false;
        }
#endif
    } else {
        KeySearchTarget target(digest);
        if(_targets.find(target) == _targets.end()) {
            return false;
        }
    }

    KeySearchResult result;
    result.privateKey = privateKeyAtIndex(index);
    if(_clipToEnd && result.privateKey.cmp(_endKey) > 0) {
        return false;
    }
    result.publicKey = limbsToPoint(&_fx[(size_t)index * 4], &_fy[(size_t)index * 4]);
    result.compressed = compressed;
    memcpy(result.hash, digest, sizeof(unsigned int) * 5);

    std::lock_guard<std::mutex> lock(_resultsMutex);
    _results.push_back(result);

    return true;
}

void CpuKeySearchDevice::processOne(uint64_t index)
{
    if(!passesHammingFilter(index)) {
        return;
    }

    const uint64_t *x = &_fx[(size_t)index * 4];
    const uint64_t *y = &_fy[(size_t)index * 4];
    unsigned int digest[5];

    if(_compression == PointCompressionType::COMPRESSED) {
        hashPublicKeyCompressedLimbs(x, y[0] & 1, digest);
        checkAndRecord(index, true, digest);
        return;
    }

    unsigned int xWords[8];
    unsigned int yWords[8];
    limbsToBeWords(x, xWords);
    limbsToBeWords(y, yWords);

    if(_compression == PointCompressionType::UNCOMPRESSED || _compression == PointCompressionType::BOTH) {
        Hash::hashPublicKey(xWords, yWords, digest);
        checkAndRecord(index, false, digest);
    }

    if(_compression == PointCompressionType::COMPRESSED || _compression == PointCompressionType::BOTH) {
        Hash::hashPublicKeyCompressed(xWords, yWords, digest);
        checkAndRecord(index, true, digest);
    }
}

void CpuKeySearchDevice::processFour(uint64_t index)
{
    if(_hammingEnabled) {
        for(int lane = 0; lane < 4; lane++) {
            processOne(index + (uint64_t)lane);
        }
        return;
    }

    unsigned int shaMsg[4][16];
    unsigned int shaDigest[4][8];
    unsigned int ripeMsg[4][16];
    unsigned int digest[4][5];

    for(int lane = 0; lane < 4; lane++) {
        const size_t i = (size_t)index + (size_t)lane;
        fillCompressedShaMsg(&_fx[i * 4], _fy[i * 4] & 1, shaMsg[lane]);
    }

    crypto::sha2564FromIv(shaMsg[0], shaDigest[0], shaMsg[1], shaDigest[1],
        shaMsg[2], shaDigest[2], shaMsg[3], shaDigest[3]);

    for(int lane = 0; lane < 4; lane++) {
        shaDigestToRipemdMsg(shaDigest[lane], ripeMsg[lane]);
    }

    crypto::ripemd160x4(ripeMsg, digest);

    for(int lane = 0; lane < 4; lane++) {
        checkAndRecord(index + (uint64_t)lane, true, digest[lane]);
    }
}

void CpuKeySearchDevice::processEight(uint64_t index)
{
    if(_hammingEnabled) {
        for(int lane = 0; lane < 8; lane++) {
            processOne(index + (uint64_t)lane);
        }
        return;
    }

    unsigned int shaMsg[8][16];
    unsigned int shaDigest[8][8];
    unsigned int digest[8][5];

    for(int lane = 0; lane < 8; lane++) {
        const size_t i = (size_t)index + (size_t)lane;
        fillCompressedShaMsg(&_fx[i * 4], _fy[i * 4] & 1, shaMsg[lane]);
    }

    crypto::sha2564FromIv(shaMsg[0], shaDigest[0], shaMsg[1], shaDigest[1],
        shaMsg[2], shaDigest[2], shaMsg[3], shaDigest[3]);
    crypto::sha2564FromIv(shaMsg[4], shaDigest[4], shaMsg[5], shaDigest[5],
        shaMsg[6], shaDigest[6], shaMsg[7], shaDigest[7]);

    crypto::ripemd160FromSha256x8(shaDigest, digest);

    for(int lane = 0; lane < 8; lane++) {
        checkAndRecord(index + (uint64_t)lane, true, digest[lane]);
    }
}

void CpuKeySearchDevice::processSixteen(uint64_t index)
{
    if(_hammingEnabled) {
        for(int lane = 0; lane < 16; lane++) {
            processOne(index + (uint64_t)lane);
        }
        return;
    }

    unsigned int shaMsg[16][16];
    unsigned int shaDigest[16][8];
    unsigned int digest[16][5];

    for(int lane = 0; lane < 16; lane++) {
        const size_t i = (size_t)index + (size_t)lane;
        fillCompressedShaMsg(&_fx[i * 4], _fy[i * 4] & 1, shaMsg[lane]);
    }

    crypto::sha2564FromIv(shaMsg[0], shaDigest[0], shaMsg[1], shaDigest[1],
        shaMsg[2], shaDigest[2], shaMsg[3], shaDigest[3]);
    crypto::sha2564FromIv(shaMsg[4], shaDigest[4], shaMsg[5], shaDigest[5],
        shaMsg[6], shaDigest[6], shaMsg[7], shaDigest[7]);
    crypto::sha2564FromIv(shaMsg[8], shaDigest[8], shaMsg[9], shaDigest[9],
        shaMsg[10], shaDigest[10], shaMsg[11], shaDigest[11]);
    crypto::sha2564FromIv(shaMsg[12], shaDigest[12], shaMsg[13], shaDigest[13],
        shaMsg[14], shaDigest[14], shaMsg[15], shaDigest[15]);

    crypto::ripemd160FromSha256x16(shaDigest, digest);

    for(int lane = 0; lane < 16; lane++) {
        checkAndRecord(index + (uint64_t)lane, true, digest[lane]);
    }
}

void CpuKeySearchDevice::processRange(uint64_t begin, uint64_t end)
{
    uint64_t i = begin;
    if(_compression == PointCompressionType::COMPRESSED) {
        if(crypto::ripemd160UsesAvx512()) {
            while(i + 16 <= end) {
#if defined(__GNUC__)
                if(i + 32 < end) {
                    __builtin_prefetch(&_fx[((size_t)i + 32) * 4], 0, 3);
                    __builtin_prefetch(&_fy[((size_t)i + 32) * 4], 0, 3);
                }
#endif
                processSixteen(i);
                i += 16;
            }
        }
        if(crypto::ripemd160UsesAvx2()) {
            while(i + 8 <= end) {
#if defined(__GNUC__)
                if(i + 24 < end) {
                    __builtin_prefetch(&_fx[((size_t)i + 24) * 4], 0, 3);
                    __builtin_prefetch(&_fy[((size_t)i + 24) * 4], 0, 3);
                }
#endif
                processEight(i);
                i += 8;
            }
        }
        while(i + 4 <= end) {
#if defined(__GNUC__)
            if(i + 12 < end) {
                __builtin_prefetch(&_fx[((size_t)i + 12) * 4], 0, 3);
                __builtin_prefetch(&_fy[((size_t)i + 12) * 4], 0, 3);
            }
#endif
            processFour(i);
            i += 4;
        }
    }

    for(; i < end; i++) {
#if defined(__GNUC__)
        if(i + 8 < end) {
            __builtin_prefetch(&_fx[((size_t)i + 8) * 4], 0, 3);
            __builtin_prefetch(&_fy[((size_t)i + 8) * 4], 0, 3);
        }
#endif
        processOne(i);
    }
}

void CpuKeySearchDevice::runWorkers(void (CpuKeySearchDevice::*fn)(uint64_t, uint64_t), uint64_t totalPoints)
{
    if(totalPoints == 0) {
        return;
    }

    std::vector<std::thread> workers;
    uint64_t chunk = totalPoints / (uint64_t)_threads;
    uint64_t remainder = totalPoints % (uint64_t)_threads;
    uint64_t offset = 0;

    workers.reserve((size_t)_threads);

    try {
        for(int t = 0; t < _threads; t++) {
            uint64_t count = chunk + (t < (int)remainder ? 1 : 0);
            uint64_t begin = offset;
            uint64_t end = offset + count;
            offset = end;

            workers.push_back(std::thread(fn, this, begin, end));
        }
    } catch(...) {
        for(size_t i = 0; i < workers.size(); i++) {
            if(workers[i].joinable()) {
                workers[i].join();
            }
        }
        throw;
    }

    for(size_t i = 0; i < workers.size(); i++) {
        workers[i].join();
    }
}

void CpuKeySearchDevice::doStep()
{
    const uint64_t hashCount = keysToHashThisStep();
#ifdef _OPENMP
    if(_compression == PointCompressionType::COMPRESSED) {
        uint64_t i = 0;
        if(crypto::ripemd160UsesAvx512()) {
            const int64_t n16 = (int64_t)(hashCount & ~(uint64_t)15);
            #pragma omp parallel for schedule(static) num_threads(_threads)
            for(int64_t j = 0; j < n16; j += 16) {
#if defined(__GNUC__)
                if(j + 32 < (int64_t)hashCount) {
                    __builtin_prefetch(&_fx[((size_t)j + 32) * 4], 0, 3);
                    __builtin_prefetch(&_fy[((size_t)j + 32) * 4], 0, 3);
                }
#endif
                processSixteen((uint64_t)j);
            }
            i = (uint64_t)n16;
        }
        if(crypto::ripemd160UsesAvx2()) {
            const int64_t n8 = (int64_t)(hashCount & ~(uint64_t)7);
            if((int64_t)i < n8) {
                #pragma omp parallel for schedule(static) num_threads(_threads)
                for(int64_t j = (int64_t)i; j < n8; j += 8) {
#if defined(__GNUC__)
                    if(j + 24 < (int64_t)hashCount) {
                        __builtin_prefetch(&_fx[((size_t)j + 24) * 4], 0, 3);
                        __builtin_prefetch(&_fy[((size_t)j + 24) * 4], 0, 3);
                    }
#endif
                    processEight((uint64_t)j);
                }
            }
            i = (uint64_t)n8;
        }
        {
            const int64_t n4 = (int64_t)(hashCount & ~(uint64_t)3);
            if((int64_t)i < n4) {
                #pragma omp parallel for schedule(static) num_threads(_threads)
                for(int64_t j = (int64_t)i; j < n4; j += 4) {
#if defined(__GNUC__)
                    if(j + 12 < (int64_t)hashCount) {
                        __builtin_prefetch(&_fx[((size_t)j + 12) * 4], 0, 3);
                        __builtin_prefetch(&_fy[((size_t)j + 12) * 4], 0, 3);
                    }
#endif
                    processFour((uint64_t)j);
                }
            }
            i = (uint64_t)n4;
        }
        for(; i < hashCount; i++) {
            processOne(i);
        }
    } else {
        #pragma omp parallel for schedule(static) num_threads(_threads)
        for(int64_t i = 0; i < (int64_t)hashCount; i++) {
            processOne((uint64_t)i);
        }
    }
#else
    runWorkers(&CpuKeySearchDevice::processRange, hashCount);
#endif
    secp256k1::addPointsBulkXY(_fx.data(), _fy.data(), _fx.size() / 4, _stepQx, _stepQy, _threads);
    _iterations++;
}

size_t CpuKeySearchDevice::getResults(std::vector<KeySearchResult> &resultsOut)
{
    std::lock_guard<std::mutex> lock(_resultsMutex);

    for(size_t i = 0; i < _results.size(); i++) {
        resultsOut.push_back(_results[i]);
    }

    size_t count = _results.size();
    _results.clear();

    return count;
}

uint64_t CpuKeySearchDevice::keysPerStep()
{
    return (uint64_t)_blocks * (uint64_t)_threads * (uint64_t)_pointsPerThread;
}

std::string CpuKeySearchDevice::getDeviceName()
{
    return _deviceName;
}

void CpuKeySearchDevice::getMemoryInfo(uint64_t &freeMem, uint64_t &totalMem)
{
    totalMem = 0;
    freeMem = 0;

#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if(GlobalMemoryStatusEx(&status)) {
        totalMem = status.ullTotalPhys;
        freeMem = status.ullAvailPhys;
    }
#else
    long pages = sysconf(_SC_PHYS_PAGES);
    long pageSize = sysconf(_SC_PAGE_SIZE);
    if(pages > 0 && pageSize > 0) {
        totalMem = (uint64_t)pages * (uint64_t)pageSize;
    }

    std::ifstream meminfo("/proc/meminfo");
    std::string line;
    while(std::getline(meminfo, line)) {
        if(line.compare(0, 8, "MemFree:") == 0) {
            freeMem = (uint64_t)util::parseUInt64(util::trim(line.substr(8)));
            freeMem *= 1024;
        } else if(line.compare(0, 12, "MemAvailable") == 0) {
            freeMem = (uint64_t)util::parseUInt64(util::trim(line.substr(13)));
            freeMem *= 1024;
            break;
        }
    }
#endif
}

secp256k1::uint256 CpuKeySearchDevice::getNextKey()
{
    return _startExponent + secp256k1::uint256(keysPerStep()) * _iterations * _stride;
}
