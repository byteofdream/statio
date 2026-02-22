#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace statio {

struct CpuInfo {
    std::string model;
    unsigned int logicalThreads = 0;
    unsigned int physicalCores = 0;
    double currentMHz = 0.0;
};

struct MemoryInfo {
    std::uint64_t totalMB = 0;
    std::uint64_t freeMB = 0;
    std::uint64_t availableMB = 0;
    std::uint64_t swapTotalMB = 0;
    std::uint64_t swapFreeMB = 0;
};

struct OsInfo {
    std::string distro;
    std::string version;
    std::string kernel;
    std::string architecture;
    std::string hostname;
};

struct DiskInfo {
    std::string mountPoint;
    std::string filesystem;
    std::uint64_t totalGB = 0;
    std::uint64_t freeGB = 0;
};

struct NetworkInfo {
    std::string name;
    std::string ipv4;
    std::string mac;
    std::uint64_t rxBytes = 0;
    std::uint64_t txBytes = 0;
};

struct GpuInfo {
    std::string adapter;
    bool detected = false;
};

struct SystemSnapshot {
    CpuInfo cpu;
    MemoryInfo memory;
    OsInfo os;
    std::vector<DiskInfo> disks;
    std::vector<NetworkInfo> network;
    std::vector<GpuInfo> gpus;
};

SystemSnapshot collectSystemSnapshot();
std::string renderReport(const SystemSnapshot& snapshot);

} // namespace statio
