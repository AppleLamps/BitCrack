#include "CpuKeySearchDevice.h"

#include "AddressUtil.h"
#include "Logger.h"
#include "util.h"

#include <cstring>
#include <fstream>
#include <thread>

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
    secp256k1::ecpoint stridePoint = secp256k1::multiplyPoint(_stride, g);
    secp256k1::ecpoint base = secp256k1::multiplyPoint(_startExponent, g);

    _points.resize((size_t)totalPoints);
    _points[0] = base;

    for(uint64_t i = 1; i < totalPoints; i++) {
        _points[(size_t)i] = secp256k1::addPoints(_points[(size_t)i - 1], stridePoint);
    }

    _stepIncrement = secp256k1::multiplyPoint(secp256k1::uint256(totalPoints) * _stride, g);

    Logger::log(LogLevel::Info, "Done");
}

void CpuKeySearchDevice::setTargets(const std::set<KeySearchTarget> &targets)
{
    _targets = targets;
}

bool CpuKeySearchDevice::checkAndRecord(uint64_t index, const secp256k1::ecpoint &point, bool compressed, const unsigned int digest[5])
{
    KeySearchTarget target(digest);
    if(_targets.find(target) == _targets.end()) {
        return false;
    }

    KeySearchResult result;
    secp256k1::uint256 offset = (secp256k1::uint256(_iterations * keysPerStep() + index) * _stride);
    result.privateKey = secp256k1::addModN(_startExponent, offset);
    result.publicKey = point;
    result.compressed = compressed;
    memcpy(result.hash, digest, sizeof(unsigned int) * 5);

    std::lock_guard<std::mutex> lock(_resultsMutex);
    _results.push_back(result);

    return true;
}

void CpuKeySearchDevice::processRange(uint64_t begin, uint64_t end)
{
    unsigned int xWords[8];
    unsigned int yWords[8];
    unsigned int digest[5];

    for(uint64_t i = begin; i < end; i++) {
        const secp256k1::ecpoint &point = _points[(size_t)i];

        point.x.exportWords(xWords, 8, secp256k1::uint256::BigEndian);
        point.y.exportWords(yWords, 8, secp256k1::uint256::BigEndian);

        if(_compression == PointCompressionType::UNCOMPRESSED || _compression == PointCompressionType::BOTH) {
            Hash::hashPublicKey(xWords, yWords, digest);
            checkAndRecord(i, point, false, digest);
        }

        if(_compression == PointCompressionType::COMPRESSED || _compression == PointCompressionType::BOTH) {
            Hash::hashPublicKeyCompressed(xWords, yWords, digest);
            checkAndRecord(i, point, true, digest);
        }
    }
}

void CpuKeySearchDevice::doStep()
{
    uint64_t totalPoints = keysPerStep();
    std::vector<std::thread> workers;
    uint64_t chunk = totalPoints / (uint64_t)_threads;
    uint64_t remainder = totalPoints % (uint64_t)_threads;
    uint64_t offset = 0;

    workers.reserve((size_t)_threads);

    for(int t = 0; t < _threads; t++) {
        uint64_t count = chunk + (t < (int)remainder ? 1 : 0);
        uint64_t begin = offset;
        uint64_t end = offset + count;
        offset = end;

        workers.push_back(std::thread(&CpuKeySearchDevice::processRange, this, begin, end));
    }

    for(size_t i = 0; i < workers.size(); i++) {
        workers[i].join();
    }

    for(uint64_t i = 0; i < totalPoints; i++) {
        _points[(size_t)i] = secp256k1::addPoints(_points[(size_t)i], _stepIncrement);
    }

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
    return _startExponent + secp256k1::uint256(keysPerStep() * _iterations) * _stride;
}
