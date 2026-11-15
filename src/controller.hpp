#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include "xhci_common.hpp"
#include "pci_models.hpp"
#include "usb_device.hpp"

struct USBController
{
    std::string bdf;              // e.g. "0000:00:14.0"
    std::string sysfsPath;        // "/sys/bus/pci/devices/0000:00:14.0"
    uint16_t vendor = 0;
    uint16_t device = 0;
    uint16_t subVendor = 0;
    uint16_t subDevice = 0;
    std::string description;
    std::string location;
    int chipsToCpu = -1;
    std::string interruptMode;    // "MSI", "MSI-X", "Line-based"
    uint64_t bar0Address = 0;
    uint64_t bar0Size = 0;
    bool isMemoryBar = false;

    std::vector<UsbDevice> attachedDevices;
};

namespace ControllerUtil
{
    inline std::string ReadFileTrimmed(const std::filesystem::path& path)
    {
        std::ifstream f(path);
        if (!f.is_open()) return "";
        std::string s;
        if (std::getline(f, s))
        {
            while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' '))
                s.pop_back();
            return s;
        }
        return "";
    }

    inline uint64_t ParseHex(const std::string& str)
    {
        if (str.empty()) return 0;
        try {
            return std::stoull(str, nullptr, 16);
        } catch (...) {
            return 0;
        }
    }

    inline std::vector<USBController> ScanControllers()
    {
        std::vector<USBController> controllers;
        const std::filesystem::path pciDir = "/sys/bus/pci/devices";
        std::error_code ec;

        if (!std::filesystem::exists(pciDir, ec))
            return controllers;

        for (const auto& entry : std::filesystem::directory_iterator(pciDir, ec))
        {
            if (!entry.is_directory(ec))
                continue;

            std::string bdf = entry.path().filename().string();
            std::string classStr = ReadFileTrimmed(entry.path() / "class");
            uint32_t pciClass = static_cast<uint32_t>(ParseHex(classStr));

            // xHCI class is 0x0c0330 (Serial bus controller -> USB -> xHCI)
            if (pciClass != xHCI::PCI_CLASS_XHCI)
                continue;

            USBController ctrl;
            ctrl.bdf = bdf;
            ctrl.sysfsPath = entry.path().string();
            ctrl.vendor = static_cast<uint16_t>(ParseHex(ReadFileTrimmed(entry.path() / "vendor")));
            ctrl.device = static_cast<uint16_t>(ParseHex(ReadFileTrimmed(entry.path() / "device")));
            ctrl.subVendor = static_cast<uint16_t>(ParseHex(ReadFileTrimmed(entry.path() / "subsystem_vendor")));
            ctrl.subDevice = static_cast<uint16_t>(ParseHex(ReadFileTrimmed(entry.path() / "subsystem_device")));

            // Parse BAR0 from "resource" file
            std::ifstream resFile(entry.path() / "resource");
            if (resFile.is_open())
            {
                std::string line;
                if (std::getline(resFile, line))
                {
                    std::istringstream iss(line);
                    std::string startStr, endStr, flagsStr;
                    if (iss >> startStr >> endStr >> flagsStr)
                    {
                        uint64_t start = ParseHex(startStr);
                        uint64_t end = ParseHex(endStr);
                        uint64_t flags = ParseHex(flagsStr);
                        ctrl.bar0Address = start;
                        if (end >= start)
                            ctrl.bar0Size = (end - start) + 1;
                        ctrl.isMemoryBar = ((flags & 0x1) == 0);
                    }
                }
            }

            // Interrupt mode: check msi_irqs directory
            std::filesystem::path msiPath = entry.path() / "msi_irqs";
            if (std::filesystem::exists(msiPath, ec) && std::filesystem::is_directory(msiPath, ec))
            {
                size_t irqCount = 0;
                for (const auto& _ : std::filesystem::directory_iterator(msiPath, ec))
                    irqCount++;
                ctrl.interruptMode = irqCount > 1 ? "MSI-X (" + std::to_string(irqCount) + " vectors)" : "MSI";
            }
            else
            {
                ctrl.interruptMode = "Line-based";
            }

            // Location & Chips
            ctrl.location = ControllerLocation(ctrl.vendor, ctrl.device);
            ctrl.chipsToCpu = ChipsToCpu(ctrl.vendor, ctrl.device);

            // Friendly name
            std::string locPart = ctrl.location;
            size_t arrowPos = locPart.find(Glyph::ARROW);
            if (arrowPos != std::string::npos)
                ctrl.description = locPart.substr(arrowPos + std::string(Glyph::ARROW).length() + 1);
            else
                ctrl.description = "USB xHCI Compliant Controller";

            // Attached devices
            ctrl.attachedDevices = UsbUtil::ScanUsbDevicesForController(entry.path());

            controllers.push_back(ctrl);
        }

        std::sort(controllers.begin(), controllers.end(), [](const USBController& a, const USBController& b) {
            return a.bdf < b.bdf;
        });

        return controllers;
    }
}
