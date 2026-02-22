#include "statio/system_info.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <ifaddrs.h>
#include <map>
#include <netdb.h>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <thread>
#include <unistd.h>

namespace statio {
namespace {

std::string trim(std::string value) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string readFileFirstLine(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        return {};
    }

    std::string line;
    std::getline(file, line);
    return trim(line);
}

std::vector<std::string> split(const std::string& input, char delimiter) {
    std::vector<std::string> out;
    std::stringstream ss(input);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        out.push_back(token);
    }
    return out;
}

std::uint64_t bytesToMB(std::uint64_t bytes) {
    return bytes / (1024ULL * 1024ULL);
}

std::uint64_t bytesToGB(std::uint64_t bytes) {
    return bytes / (1024ULL * 1024ULL * 1024ULL);
}

std::size_t mountDepth(const std::string& mountPoint) {
    if (mountPoint == "/") {
        return 0;
    }
    return static_cast<std::size_t>(std::count(mountPoint.begin(), mountPoint.end(), '/'));
}

bool isUsefulMountPoint(const std::string& mountPoint) {
    static const std::set<std::string> allowedExact = {
        "/", "/home", "/boot", "/boot/efi", "/var", "/opt", "/mnt", "/media", "/srv"};

    if (allowedExact.count(mountPoint) != 0) {
        return true;
    }

    return false;
}

CpuInfo collectCpuInfo() {
    CpuInfo info;
    info.logicalThreads = std::thread::hardware_concurrency();

    std::ifstream cpuInfoFile("/proc/cpuinfo");
    if (!cpuInfoFile) {
        return info;
    }

    std::string line;
    while (std::getline(cpuInfoFile, line)) {
        auto pos = line.find(':');
        if (pos == std::string::npos) {
            continue;
        }

        std::string key = trim(line.substr(0, pos));
        std::string value = trim(line.substr(pos + 1));

        if (key == "model name" && info.model.empty()) {
            info.model = value;
        } else if (key == "cpu cores" && info.physicalCores == 0) {
            try {
                info.physicalCores = static_cast<unsigned int>(std::stoul(value));
            } catch (...) {
            }
        } else if (key == "cpu MHz" && info.currentMHz <= 0.0) {
            try {
                info.currentMHz = std::stod(value);
            } catch (...) {
            }
        }
    }

    return info;
}

MemoryInfo collectMemoryInfo() {
    MemoryInfo info;

    struct sysinfo data {};
    if (sysinfo(&data) != 0) {
        return info;
    }

    const std::uint64_t unit = data.mem_unit;
    info.totalMB = bytesToMB(data.totalram * unit);
    info.freeMB = bytesToMB(data.freeram * unit);
    info.availableMB = bytesToMB((data.freeram + data.bufferram) * unit);
    info.swapTotalMB = bytesToMB(data.totalswap * unit);
    info.swapFreeMB = bytesToMB(data.freeswap * unit);

    std::ifstream meminfo("/proc/meminfo");
    std::string line;
    while (std::getline(meminfo, line)) {
        if (line.rfind("MemAvailable:", 0) == 0) {
            auto tokens = split(line, ' ');
            for (const auto& token : tokens) {
                if (token.empty()) {
                    continue;
                }
                try {
                    info.availableMB = std::stoull(token) / 1024ULL;
                    return info;
                } catch (...) {
                }
            }
        }
    }

    return info;
}

OsInfo collectOsInfo() {
    OsInfo info;
    struct utsname uts {};

    if (uname(&uts) == 0) {
        info.kernel = uts.release;
        info.architecture = uts.machine;
        info.hostname = uts.nodename;
    }

    std::ifstream osRelease("/etc/os-release");
    std::string line;
    while (std::getline(osRelease, line)) {
        auto pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        if (!value.empty() && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }

        if (key == "PRETTY_NAME") {
            info.distro = value;
        } else if (key == "VERSION_ID") {
            info.version = value;
        }
    }

    return info;
}

std::vector<DiskInfo> collectDiskInfo() {
    std::vector<DiskInfo> disks;
    std::ifstream mounts("/proc/mounts");
    if (!mounts) {
        return disks;
    }

    std::set<std::string> seen;
    std::string line;
    while (std::getline(mounts, line)) {
        auto parts = split(line, ' ');
        if (parts.size() < 4) {
            continue;
        }

        const std::string& source = parts[0];
        const std::string& mountPoint = parts[1];
        const std::string& fsType = parts[2];
        const std::string& options = parts[3];

        static const std::set<std::string> pseudo = {
            "proc", "sysfs", "tmpfs", "devtmpfs", "cgroup", "cgroup2", "overlay", "squashfs", "devpts", "securityfs", "pstore", "mqueue", "tracefs", "fusectl"};

        if (pseudo.count(fsType) != 0 || seen.count(mountPoint) != 0) {
            continue;
        }
        if (source.rfind("/dev/", 0) != 0) {
            continue;
        }
        if (options.find("bind") != std::string::npos) {
            continue;
        }
        if (mountPoint.find("/.") != std::string::npos) {
            continue;
        }
        if (mountDepth(mountPoint) > 2 && mountPoint != "/boot/efi") {
            continue;
        }
        if (!isUsefulMountPoint(mountPoint)) {
            continue;
        }

        struct statvfs stat {};
        if (statvfs(mountPoint.c_str(), &stat) != 0) {
            continue;
        }

        seen.insert(mountPoint);
        DiskInfo d;
        d.mountPoint = mountPoint;
        d.filesystem = fsType;
        d.totalGB = bytesToGB(stat.f_blocks * stat.f_frsize);
        d.freeGB = bytesToGB(stat.f_bavail * stat.f_frsize);
        disks.push_back(d);
    }

    std::sort(disks.begin(), disks.end(), [](const DiskInfo& a, const DiskInfo& b) {
        return a.mountPoint < b.mountPoint;
    });

    return disks;
}

std::vector<NetworkInfo> collectNetworkInfo() {
    std::vector<NetworkInfo> list;
    std::map<std::string, NetworkInfo> byName;

    ifaddrs* ifAddrList = nullptr;
    if (getifaddrs(&ifAddrList) != 0) {
        return list;
    }

    for (ifaddrs* it = ifAddrList; it != nullptr; it = it->ifa_next) {
        if (!it->ifa_name) {
            continue;
        }

        std::string ifaceName = it->ifa_name;
        auto& entry = byName[ifaceName];
        entry.name = ifaceName;

        if (!it->ifa_addr) {
            continue;
        }

        const int family = it->ifa_addr->sa_family;
        if (family == AF_INET) {
            std::array<char, NI_MAXHOST> host{};
            int rc = getnameinfo(it->ifa_addr,
                                 sizeof(sockaddr_in),
                                 host.data(),
                                 static_cast<socklen_t>(host.size()),
                                 nullptr,
                                 0,
                                 NI_NUMERICHOST);
            if (rc == 0) {
                entry.ipv4 = host.data();
            }
        }
    }
    freeifaddrs(ifAddrList);

    for (auto& [name, entry] : byName) {
        entry.mac = readFileFirstLine("/sys/class/net/" + name + "/address");

        std::string rx = readFileFirstLine("/sys/class/net/" + name + "/statistics/rx_bytes");
        std::string tx = readFileFirstLine("/sys/class/net/" + name + "/statistics/tx_bytes");

        try {
            if (!rx.empty()) {
                entry.rxBytes = std::stoull(rx);
            }
            if (!tx.empty()) {
                entry.txBytes = std::stoull(tx);
            }
        } catch (...) {
        }

        list.push_back(entry);
    }

    std::sort(list.begin(), list.end(), [](const NetworkInfo& a, const NetworkInfo& b) {
        return a.name < b.name;
    });

    return list;
}

std::vector<GpuInfo> collectGpuInfo() {
    std::vector<GpuInfo> gpus;

    // Lightweight fallback: check DRM cards without external dependencies.
    for (int i = 0; i < 8; ++i) {
        const std::string base = "/sys/class/drm/card" + std::to_string(i);
        std::ifstream probe(base + "/device/vendor");
        if (!probe) {
            continue;
        }

        std::string vendor;
        std::getline(probe, vendor);
        GpuInfo gpu;
        gpu.detected = true;
        gpu.adapter = "card" + std::to_string(i) + " vendor=" + trim(vendor);
        gpus.push_back(gpu);
    }

    if (gpus.empty()) {
        gpus.push_back(GpuInfo{"No GPU details (platform-specific collector needed)", false});
    }

    return gpus;
}

} // namespace

SystemSnapshot collectSystemSnapshot() {
    SystemSnapshot snapshot;
    snapshot.cpu = collectCpuInfo();
    snapshot.memory = collectMemoryInfo();
    snapshot.os = collectOsInfo();
    snapshot.disks = collectDiskInfo();
    snapshot.network = collectNetworkInfo();
    snapshot.gpus = collectGpuInfo();
    return snapshot;
}

std::string renderReport(const SystemSnapshot& snapshot) {
    std::ostringstream out;
    out << "Statio v0.1 - Hardware/OS Diagnostic Report\n";
    out << "==========================================\n\n";

    out << "[OS]\n";
    out << "Distro: " << (snapshot.os.distro.empty() ? "N/A" : snapshot.os.distro) << '\n';
    out << "Version: " << (snapshot.os.version.empty() ? "N/A" : snapshot.os.version) << '\n';
    out << "Kernel: " << (snapshot.os.kernel.empty() ? "N/A" : snapshot.os.kernel) << '\n';
    out << "Arch: " << (snapshot.os.architecture.empty() ? "N/A" : snapshot.os.architecture) << '\n';
    out << "Host: " << (snapshot.os.hostname.empty() ? "N/A" : snapshot.os.hostname) << "\n\n";

    out << "[CPU]\n";
    out << "Model: " << (snapshot.cpu.model.empty() ? "N/A" : snapshot.cpu.model) << '\n';
    out << "Physical cores: " << snapshot.cpu.physicalCores << '\n';
    out << "Logical threads: " << snapshot.cpu.logicalThreads << '\n';
    out << "Current MHz: " << std::fixed << std::setprecision(2) << snapshot.cpu.currentMHz << "\n\n";

    out << "[Memory]\n";
    out << "Total RAM: " << snapshot.memory.totalMB << " MB\n";
    out << "Free RAM: " << snapshot.memory.freeMB << " MB\n";
    out << "Available RAM*: " << snapshot.memory.availableMB << " MB\n";
    out << "Total Swap: " << snapshot.memory.swapTotalMB << " MB\n";
    out << "Free Swap: " << snapshot.memory.swapFreeMB << " MB\n\n";

    out << "[Disks]\n";
    for (const auto& d : snapshot.disks) {
        out << d.mountPoint << " (" << d.filesystem << ") total=" << d.totalGB << "GB free=" << d.freeGB << "GB\n";
    }
    if (snapshot.disks.empty()) {
        out << "No mounted disks detected\n";
    }
    out << '\n';

    out << "[Network]\n";
    for (const auto& n : snapshot.network) {
        out << n.name
            << " ipv4=" << (n.ipv4.empty() ? "N/A" : n.ipv4)
            << " mac=" << (n.mac.empty() ? "N/A" : n.mac)
            << " rx=" << n.rxBytes
            << " tx=" << n.txBytes
            << '\n';
    }
    if (snapshot.network.empty()) {
        out << "No network interfaces detected\n";
    }
    out << '\n';

    out << "[GPU]\n";
    for (const auto& g : snapshot.gpus) {
        out << g.adapter << '\n';
    }

    out << "\n*Available RAM approximation uses free + buffer memory.\n";

    return out.str();
}

} // namespace statio
