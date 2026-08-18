#include "CpuKeySearchDevice.h"

#include "AddressUtil.h"
#include "CryptoUtil.h"
#include "Logger.h"
#include "util.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <thread>

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

    secp256k1::generateKeyPairsBulk(g, exponents, _points);

    _stepIncrement = secp256k1::multiplyPoint(secp256k1::uint256(totalPoints) * _stride, g);

    if(crypto::sha256UsesHardware()) {
        Logger::log(LogLevel::Info, "SHA-256: hardware SHA-NI");
    } else {
        Logger::log(LogLevel::Info, "SHA-256: software");
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

bool CpuKeySearchDevice::checkAndRecord(uint64_t index, const secp256k1::ecpoint &point, bool compressed, const unsigned int digest[5])
{
    if(_singleTarget) {
        if(digest[0] != _singleTargetHash[0] || digest[1] != _singleTargetHash[1] ||
           digest[2] != _singleTargetHash[2] || digest[3] != _singleTargetHash[3] ||
           digest[4] != _singleTargetHash[4]) {
            return false;
        }
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
    result.publicKey = point;
    result.compressed = compressed;
    memcpy(result.hash, digest, sizeof(unsigned int) * 5);

    std::lock_guard<std::mutex> lock(_resultsMutex);
    _results.push_back(result);

    return true;
}

void CpuKeySearchDevice::processOne(uint64_t index)
{
    const secp256k1::ecpoint &point = _points[(size_t)index];
    unsigned int digest[5];

    if(_compression == PointCompressionType::COMPRESSED) {
        Hash::hashPublicKeyCompressed(point, digest);
        checkAndRecord(index, point, true, digest);
        return;
    }

    unsigned int xWords[8];
    unsigned int yWords[8];
    point.x.exportWords(xWords, 8, secp256k1::uint256::BigEndian);
    point.y.exportWords(yWords, 8, secp256k1::uint256::BigEndian);

    if(_compression == PointCompressionType::UNCOMPRESSED || _compression == PointCompressionType::BOTH) {
        Hash::hashPublicKey(xWords, yWords, digest);
        checkAndRecord(index, point, false, digest);
    }

    if(_compression == PointCompressionType::COMPRESSED || _compression == PointCompressionType::BOTH) {
        Hash::hashPublicKeyCompressed(xWords, yWords, digest);
        checkAndRecord(index, point, true, digest);
    }
}

void CpuKeySearchDevice::processRange(uint64_t begin, uint64_t end)
{
    for(uint64_t i = begin; i < end; i++) {
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
    #pragma omp parallel for schedule(static) num_threads(_threads)
    for(int64_t i = 0; i < (int64_t)hashCount; i++) {
        processOne((uint64_t)i);
    }
#else
    runWorkers(&CpuKeySearchDevice::processRange, hashCount);
#endif
    secp256k1::addPointsBulk(_points, _stepIncrement, _threads);
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
