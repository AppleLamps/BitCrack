#ifndef _CPU_KEY_SEARCH_DEVICE_H
#define _CPU_KEY_SEARCH_DEVICE_H

#include "KeySearchDevice.h"
#include <cstdint>
#include <mutex>
#include <set>
#include <vector>

class CpuKeySearchDevice : public KeySearchDevice {

private:
    int _threads;
    int _blocks;
    int _pointsPerThread;
    int _compression;

    std::string _deviceName;

    secp256k1::uint256 _startExponent;
    secp256k1::uint256 _stride;
    secp256k1::uint256 _endKey;
    uint64_t _stepQx[4];
    uint64_t _stepQy[4];
    bool _clipToEnd;

    uint64_t _iterations;

    // Working points as 4 little-endian 64-bit field limbs each (n*4).
    std::vector<uint64_t> _fx;
    std::vector<uint64_t> _fy;
    std::set<KeySearchTarget> _targets;
    unsigned int _singleTargetHash[5];
    bool _singleTarget;

    std::vector<KeySearchResult> _results;
    std::mutex _resultsMutex;

    bool _hammingEnabled;
    int _hammingMinOnes;
    int _hammingMaxOnes;

    bool passesHammingFilter(uint64_t index);
    static uint32_t top7HexBits(const secp256k1::uint256 &k);
    static int popcount28(uint32_t x);

    void processOne(uint64_t index);
    void processFour(uint64_t index);
    void processEight(uint64_t index);
    void processSixteen(uint64_t index);
    void processRange(uint64_t begin, uint64_t end);
    void runWorkers(void (CpuKeySearchDevice::*fn)(uint64_t, uint64_t), uint64_t totalPoints);
    bool checkAndRecord(uint64_t index, bool compressed, const unsigned int digest[5]);
    uint64_t keysToHashThisStep();
    secp256k1::uint256 privateKeyAtIndex(uint64_t index);

public:
    CpuKeySearchDevice(int threads, int pointsPerThread, int blocks = 1);

    virtual void init(const secp256k1::uint256 &start, int compression, const secp256k1::uint256 &stride);
    virtual void doStep();
    virtual void setTargets(const std::set<KeySearchTarget> &targets);
    virtual void setEndKey(const secp256k1::uint256 &endKey);
    void setHammingFilter(int minOnes, int maxOnes);
    virtual size_t getResults(std::vector<KeySearchResult> &results);
    virtual uint64_t keysPerStep();
    virtual std::string getDeviceName();
    virtual void getMemoryInfo(uint64_t &freeMem, uint64_t &totalMem);
    virtual secp256k1::uint256 getNextKey();
};

#endif
