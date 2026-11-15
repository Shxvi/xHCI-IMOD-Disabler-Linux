#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <cstring>
#include <cerrno>
#include <iostream>
#include <filesystem>
#include "xhci_common.hpp"
#include "controller.hpp"

struct InterrupterState
{
    uint32_t index = 0;
    uint32_t imodValue = 0;
    bool enabled = false;
    uint64_t mmioAddress = 0;
};

struct ControllerImodStatus
{
    bool accessible = false;
    std::string backendUsed;      // "Userspace MMIO" or "Kernel Module"
    std::string errorMessage;
    uint32_t interrupterCount = 0;
    std::vector<InterrupterState> interrupters;
    uint32_t commonInterval = 0;
    bool isMixed = false;
};

class IXhciBackend
{
public:
    virtual ~IXhciBackend() = default;
    virtual bool IsAvailable(const USBController& ctrl, std::string& reason) = 0;
    virtual ControllerImodStatus ReadStatus(const USBController& ctrl) = 0;
    virtual bool WriteImod(const USBController& ctrl, uint32_t interval, std::string& error) = 0;
    virtual bool WriteInterrupterImod(const USBController& ctrl, uint32_t interrupter, uint32_t interval, std::string& error) = 0;
};

// Backend 1: Direct sysfs /sys/bus/pci/devices/<bdf>/resource0 MMIO
class UserspaceMmioBackend : public IXhciBackend
{
public:
    bool IsAvailable(const USBController& ctrl, std::string& reason) override
    {
        std::string resPath = ctrl.sysfsPath + "/resource0";
        if (getuid() != 0)
        {
            reason = "Root (sudo) permissions required to access PCI resource";
            return false;
        }

        int fd = open(resPath.c_str(), O_RDWR | O_SYNC);
        if (fd < 0)
        {
            reason = "Failed to open " + resPath + ": " + std::strerror(errno);
            return false;
        }

        // Try mapping the first page
        void* map = mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (map == MAP_FAILED)
        {
            int err = errno;
            close(fd);
            if (err == EINVAL || err == EPERM)
            {
                std::ifstream cmdlineFile("/proc/cmdline");
                std::string cmdline;
                std::getline(cmdlineFile, cmdline);
                if (cmdline.find("iomem=relaxed") == std::string::npos)
                {
                    reason = "Blocked by CONFIG_IO_STRICT_DEVMEM: 'iomem=relaxed' is NOT active in /proc/cmdline. "
                             "If on CachyOS/Arch UKI, run 'sudo mkinitcpio -P' and reboot. Or on GRUB run 'sudo grub-mkconfig -o /boot/grub/grub.cfg'.";
                }
                else
                {
                    reason = "Blocked by kernel security (CONFIG_IO_STRICT_DEVMEM). Use the xhci_imod kernel module.";
                }
            }
            else
            {
                reason = "mmap failed: " + std::string(std::strerror(err));
            }
            return false;
        }

        munmap(map, 4096);
        close(fd);
        return true;
    }

    ControllerImodStatus ReadStatus(const USBController& ctrl) override
    {
        ControllerImodStatus status;
        status.backendUsed = "Userspace MMIO";

        std::string resPath = ctrl.sysfsPath + "/resource0";
        int fd = open(resPath.c_str(), O_RDWR | O_SYNC);
        if (fd < 0)
        {
            status.accessible = false;
            status.errorMessage = std::strerror(errno);
            return status;
        }

        size_t mapSize = ctrl.bar0Size;
        if (mapSize == 0) mapSize = 65536;

        void* base = mmap(nullptr, mapSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        close(fd);

        if (base == MAP_FAILED)
        {
            status.accessible = false;
            std::ifstream cmdlineFile("/proc/cmdline");
            std::string cmdline;
            std::getline(cmdlineFile, cmdline);
            if (cmdline.find("iomem=relaxed") == std::string::npos)
            {
                status.errorMessage = "CONFIG_IO_STRICT_DEVMEM: 'iomem=relaxed' is missing from /proc/cmdline. "
                                      "Run 'sudo mkinitcpio -P' (on UKI/CachyOS) or 'sudo update-grub' and reboot.";
            }
            else
            {
                status.errorMessage = "mmap failed (CONFIG_IO_STRICT_DEVMEM): load xhci_imod kernel module";
            }
            return status;
        }

        volatile uint8_t* mmio = static_cast<volatile uint8_t*>(base);
        uint8_t caplength = mmio[0];
        uint16_t hciversion = *reinterpret_cast<volatile uint16_t*>(mmio + 0x02);
        uint32_t hcsparams1 = *reinterpret_cast<volatile uint32_t*>(mmio + xHCI::HCSPARAMS1_OFFSET);
        uint32_t rtsoff = *reinterpret_cast<volatile uint32_t*>(mmio + xHCI::RTSOFF_OFFSET) & ~0x1Fu;

        if (caplength == 0xFF || hciversion < 0x0096)
        {
            munmap(base, mapSize);
            status.accessible = false;
            status.errorMessage = "Invalid xHCI capability header";
            return status;
        }

        uint32_t maxInterrupters = (hcsparams1 >> 8) & 0x7FF;
        if (maxInterrupters == 0 || maxInterrupters > xHCI::MAX_INTERRUPTERS)
            maxInterrupters = 1;

        status.interrupterCount = maxInterrupters;
        status.accessible = true;

        std::set<uint32_t> distinctIntervals;

        for (uint32_t i = 0; i < maxInterrupters; ++i)
        {
            uint32_t irOffset = rtsoff + xHCI::INTERRUPTER_OFFSET + (i * xHCI::INTERRUPTER_SIZE);
            if (irOffset + xHCI::INTERRUPTER_SIZE > mapSize)
                break;

            uint32_t iman = *reinterpret_cast<volatile uint32_t*>(mmio + irOffset + xHCI::IMAN_OFFSET);
            uint32_t imod = *reinterpret_cast<volatile uint32_t*>(mmio + irOffset + xHCI::IMOD_OFFSET);

            InterrupterState is;
            is.index = i;
            is.imodValue = imod & xHCI::IMOD_INTERVAL_MASK;
            is.enabled = (iman & 0x02) != 0;
            is.mmioAddress = ctrl.bar0Address + irOffset + xHCI::IMOD_OFFSET;

            status.interrupters.push_back(is);

            if (is.enabled || i == 0)
                distinctIntervals.insert(is.imodValue);
        }

        munmap(base, mapSize);

        if (distinctIntervals.size() == 1)
        {
            status.commonInterval = *distinctIntervals.begin();
            status.isMixed = false;
        }
        else
        {
            status.isMixed = true;
            status.commonInterval = status.interrupters.empty() ? 0 : status.interrupters[0].imodValue;
        }

        return status;
    }

    bool WriteImod(const USBController& ctrl, uint32_t interval, std::string& error) override
    {
        std::string resPath = ctrl.sysfsPath + "/resource0";
        int fd = open(resPath.c_str(), O_RDWR | O_SYNC);
        if (fd < 0)
        {
            error = std::strerror(errno);
            return false;
        }

        size_t mapSize = ctrl.bar0Size;
        if (mapSize == 0) mapSize = 65536;

        void* base = mmap(nullptr, mapSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        close(fd);

        if (base == MAP_FAILED)
        {
            error = "mmap failed: " + std::string(std::strerror(errno));
            return false;
        }

        volatile uint8_t* mmio = static_cast<volatile uint8_t*>(base);
        uint32_t hcsparams1 = *reinterpret_cast<volatile uint32_t*>(mmio + xHCI::HCSPARAMS1_OFFSET);
        uint32_t rtsoff = *reinterpret_cast<volatile uint32_t*>(mmio + xHCI::RTSOFF_OFFSET) & ~0x1Fu;
        uint32_t maxInterrupters = (hcsparams1 >> 8) & 0x7FF;
        if (maxInterrupters == 0 || maxInterrupters > xHCI::MAX_INTERRUPTERS)
            maxInterrupters = 1;

        for (uint32_t i = 0; i < maxInterrupters; ++i)
        {
            uint32_t irOffset = rtsoff + xHCI::INTERRUPTER_OFFSET + (i * xHCI::INTERRUPTER_SIZE);
            if (irOffset + xHCI::INTERRUPTER_SIZE > mapSize)
                break;

            volatile uint32_t* imodReg = reinterpret_cast<volatile uint32_t*>(mmio + irOffset + xHCI::IMOD_OFFSET);
            *imodReg = (interval & xHCI::IMOD_INTERVAL_MASK);
        }

        munmap(base, mapSize);
        return true;
    }

    bool WriteInterrupterImod(const USBController& ctrl, uint32_t interrupter, uint32_t interval, std::string& error) override
    {
        std::string resPath = ctrl.sysfsPath + "/resource0";
        int fd = open(resPath.c_str(), O_RDWR | O_SYNC);
        if (fd < 0)
        {
            error = std::strerror(errno);
            return false;
        }

        size_t mapSize = ctrl.bar0Size;
        if (mapSize == 0) mapSize = 65536;

        void* base = mmap(nullptr, mapSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        close(fd);

        if (base == MAP_FAILED)
        {
            error = "mmap failed: " + std::string(std::strerror(errno));
            return false;
        }

        volatile uint8_t* mmio = static_cast<volatile uint8_t*>(base);
        uint32_t rtsoff = *reinterpret_cast<volatile uint32_t*>(mmio + xHCI::RTSOFF_OFFSET) & ~0x1Fu;
        uint32_t irOffset = rtsoff + xHCI::INTERRUPTER_OFFSET + (interrupter * xHCI::INTERRUPTER_SIZE);

        if (irOffset + xHCI::INTERRUPTER_SIZE <= mapSize)
        {
            volatile uint32_t* imodReg = reinterpret_cast<volatile uint32_t*>(mmio + irOffset + xHCI::IMOD_OFFSET);
            *imodReg = (interval & xHCI::IMOD_INTERVAL_MASK);
        }
        else
        {
            munmap(base, mapSize);
            error = "Interrupter offset out of bounds";
            return false;
        }

        munmap(base, mapSize);
        return true;
    }
};

// Backend 2: Companion Linux Kernel Module (/sys/kernel/xhci_imod/<bdf>/imod)
class KernelModuleBackend : public IXhciBackend
{
public:
    bool IsAvailable(const USBController& ctrl, std::string& reason) override
    {
        std::filesystem::path sysfsNode = "/sys/kernel/xhci_imod/" + ctrl.bdf + "/imod";
        std::error_code ec;
        if (std::filesystem::exists(sysfsNode, ec))
            return true;

        reason = "Kernel module xhci_imod is not loaded (/sys/kernel/xhci_imod not found)";
        return false;
    }

    ControllerImodStatus ReadStatus(const USBController& ctrl) override
    {
        ControllerImodStatus status;
        status.backendUsed = "Kernel Module";

        std::filesystem::path sysfsNode = "/sys/kernel/xhci_imod/" + ctrl.bdf + "/imod";
        std::ifstream file(sysfsNode);
        if (!file.is_open())
        {
            status.accessible = false;
            status.errorMessage = "Could not read " + sysfsNode.string();
            return status;
        }

        std::string valStr;
        file >> valStr;

        uint32_t val = 0;
        try {
            val = std::stoul(valStr, nullptr, 0);
        } catch (...) {
            status.accessible = false;
            status.errorMessage = "Invalid value read from kernel module sysfs";
            return status;
        }

        status.accessible = true;
        status.commonInterval = val & xHCI::IMOD_INTERVAL_MASK;
        status.isMixed = false;
        status.interrupterCount = 1;

        InterrupterState is;
        is.index = 0;
        is.imodValue = status.commonInterval;
        is.enabled = true;
        is.mmioAddress = ctrl.bar0Address;
        status.interrupters.push_back(is);

        return status;
    }

    bool WriteImod(const USBController& ctrl, uint32_t interval, std::string& error) override
    {
        std::filesystem::path sysfsNode = "/sys/kernel/xhci_imod/" + ctrl.bdf + "/imod";
        std::ofstream file(sysfsNode);
        if (!file.is_open())
        {
            error = "Could not open " + sysfsNode.string() + " for writing (root required)";
            return false;
        }

        file << HexString(interval & xHCI::IMOD_INTERVAL_MASK) << "\n";
        if (file.fail())
        {
            error = "Failed to write value to " + sysfsNode.string();
            return false;
        }

        return true;
    }

    bool WriteInterrupterImod(const USBController& ctrl, uint32_t interrupter, uint32_t interval, std::string& error) override
    {
        (void)interrupter;
        return WriteImod(ctrl, interval, error);
    }
};

// Unified Backend: tries Userspace MMIO first, falls back to Kernel Module
class UnifiedBackend
{
public:
    UnifiedBackend()
        : mmioBackend(std::make_unique<UserspaceMmioBackend>()),
          kmodBackend(std::make_unique<KernelModuleBackend>())
    {}

    IXhciBackend* GetBestBackend(const USBController& ctrl, std::string& diag)
    {
        std::string mmioReason, kmodReason;
        if (mmioBackend->IsAvailable(ctrl, mmioReason))
            return mmioBackend.get();

        if (kmodBackend->IsAvailable(ctrl, kmodReason))
            return kmodBackend.get();

        diag = "Userspace MMIO: " + mmioReason + "\n  Kernel Module: " + kmodReason;
        return nullptr;
    }

    ControllerImodStatus GetStatus(const USBController& ctrl)
    {
        std::string diag;
        IXhciBackend* backend = GetBestBackend(ctrl, diag);
        if (backend)
            return backend->ReadStatus(ctrl);

        ControllerImodStatus st;
        st.accessible = false;
        st.errorMessage = diag;
        return st;
    }

    bool SetImod(const USBController& ctrl, uint32_t interval, std::string& error)
    {
        std::string diag;
        IXhciBackend* backend = GetBestBackend(ctrl, diag);
        if (!backend)
        {
            error = diag;
            return false;
        }
        return backend->WriteImod(ctrl, interval, error);
    }

private:
    std::unique_ptr<UserspaceMmioBackend> mmioBackend;
    std::unique_ptr<KernelModuleBackend> kmodBackend;
};
