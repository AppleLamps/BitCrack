#ifndef _CPU_KEY_SEARCH_DEVICE_H
#define _CPU_KEY_SEARCH_DEVICE_H

#include "KeySearchDevice.h"
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
    secp256k1::ecpoint _stepIncrement;

    uint64_t _iterations;

    std::vector<secp256k1::ecpoint> _points;
    std::set<KeySearchTarget> _targets;
    std::vector<KeySearchResult> _results;
    std::mutex _resultsMutex;

    void processRange(uint64_t begin, uint64_t end);
    bool checkAndRecord(uint64_t index, const secp256k1::ecpoint &point, bool compressed, const unsigned int digest[5]);

public:
    CpuKeySearchDevice(int threads, int pointsPerThread, int blocks = 1);

    virtual void init(const secp256k1::uint256 &start, int compression, const secp256k1::uint256 &stride);
    virtual void doStep();
    virtual void setTargets(const std::set<KeySearchTarget> &targets);
    virtual size_t getResults(std::vector<KeySearchResult> &results);
    virtual uint64_t keysPerStep();
    virtual std::string getDeviceName();
    virtual void getMemoryInfo(uint64_t &freeMem, uint64_t &totalMem);
    virtual secp256k1::uint256 getNextKey();
};

#endif
