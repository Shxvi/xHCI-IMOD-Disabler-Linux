#pragma once

#include <string>
#include <vector>
#include <set>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include "xhci_common.hpp"

struct UsbDevice
{
    std::string name;
    std::string types;
    int port = 0;
    std::vector<std::string> hubs;
    bool isMouse = false;
    bool isKeyboard = false;
    std::string speed;
    std::string vendorId;
    std::string productId;
};

namespace UsbUtil
{
    inline std::string ReadTrimmed(const std::filesystem::path& path)
    {
        std::ifstream file(path);
        if (!file.is_open())
            return "";
        std::string line;
        if (std::getline(file, line))
        {
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' '))
                line.pop_back();
            return line;
        }
        return "";
    }

    inline std::string ToLower(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
        return s;
    }

    inline std::vector<UsbDevice> ScanUsbDevicesForController(const std::filesystem::path& pciDevPath)
    {
        std::vector<UsbDevice> devices;
        std::error_code ec;

        if (!std::filesystem::exists(pciDevPath, ec))
            return devices;

        // Iterate over subdirectories like usb1, usb2, usb3, usb4 (root hubs)
        for (const auto& entry : std::filesystem::directory_iterator(pciDevPath, ec))
        {
            if (!entry.is_directory(ec))
                continue;

            std::string filename = entry.path().filename().string();
            if (filename.rfind("usb", 0) != 0)
                continue; // only root hubs

            // Now scan devices under this root hub
            for (const auto& devEntry : std::filesystem::directory_iterator(entry.path(), ec))
            {
                if (!devEntry.is_directory(ec))
                    continue;

                std::string devName = devEntry.path().filename().string();
                // Device folders have format like "3-5" or "3-5.1" (contains a hyphen and no colon)
                if (devName.find('-') == std::string::npos || devName.find(':') != std::string::npos)
                    continue;

                std::string product = ReadTrimmed(devEntry.path() / "product");
                std::string manufacturer = ReadTrimmed(devEntry.path() / "manufacturer");
                std::string speed = ReadTrimmed(devEntry.path() / "speed");
                std::string devpath = ReadTrimmed(devEntry.path() / "devpath");
                std::string idVendor = ReadTrimmed(devEntry.path() / "idVendor");
                std::string idProduct = ReadTrimmed(devEntry.path() / "idProduct");

                std::string displayName;
                if (!manufacturer.empty() && !product.empty())
                {
                    if (product.find(manufacturer) == std::string::npos)
                        displayName = manufacturer + " " + product;
                    else
                        displayName = product;
                }
                else if (!product.empty())
                    displayName = product;
                else if (!manufacturer.empty())
                    displayName = manufacturer + " USB Device";
                else
                    displayName = "USB Device [" + idVendor + ":" + idProduct + "]";

                std::set<std::string> types;
                bool isMouse = false;
                bool isKeyboard = false;

                // Inspect interfaces under this device
                for (const auto& ifEntry : std::filesystem::directory_iterator(devEntry.path(), ec))
                {
                    if (!ifEntry.is_directory(ec))
                        continue;

                    std::string ifName = ifEntry.path().filename().string();
                    if (ifName.find(':') == std::string::npos)
                        continue;

                    std::string ifClass = ReadTrimmed(ifEntry.path() / "bInterfaceClass");
                    std::string ifProto = ReadTrimmed(ifEntry.path() / "bInterfaceProtocol");

                    if (ifClass == "03") // HID
                    {
                        if (ifProto == "01")
                        {
                            types.insert("Keyboard");
                            isKeyboard = true;
                        }
                        else if (ifProto == "02")
                        {
                            types.insert("Mouse");
                            isMouse = true;
                        }
                        else
                        {
                            types.insert("HID");
                        }
                    }
                    else if (ifClass == "01") types.insert("Audio");
                    else if (ifClass == "08") types.insert("Storage");
                    else if (ifClass == "0e") types.insert("Camera");
                    else if (ifClass == "e0") types.insert("Bluetooth");
                    else if (ifClass == "09") types.insert("Hub");
                    else if (ifClass == "02") types.insert("Network");
                }

                // Check product name keywords if class didn't catch mouse/keyboard
                std::string lowerProduct = ToLower(product + " " + displayName);
                if (lowerProduct.find("mouse") != std::string::npos || lowerProduct.find("dongle") != std::string::npos)
                {
                    types.insert("Mouse");
                    isMouse = true;
                }
                if (lowerProduct.find("keyboard") != std::string::npos)
                {
                    types.insert("Keyboard");
                    isKeyboard = true;
                }

                if (types.size() > 1)
                    types.erase("HID");

                std::string typeList;
                for (const auto& t : types)
                    typeList += (typeList.empty() ? "" : ", ") + t;
                if (typeList.empty())
                    typeList = "USB device";

                // Parse port and hub chain from devpath (e.g. "5.1" -> port 1 on hub connected to port 5)
                int port = 0;
                std::vector<std::string> hubs;
                if (!devpath.empty())
                {
                    size_t lastDot = devpath.find_last_of('.');
                    if (lastDot != std::string::npos)
                    {
                        port = std::stoi(devpath.substr(lastDot + 1));
                        // Count dots for hubs
                        size_t hubCount = std::count(devpath.begin(), devpath.end(), '.');
                        for (size_t h = 0; h < hubCount; ++h)
                            hubs.push_back("External Hub");
                    }
                    else
                    {
                        try { port = std::stoi(devpath); } catch (...) { port = 0; }
                    }
                }

                UsbDevice dev;
                dev.name = displayName;
                dev.types = typeList;
                dev.port = port;
                dev.hubs = hubs;
                dev.isMouse = isMouse;
                dev.isKeyboard = isKeyboard;
                dev.speed = speed;
                dev.vendorId = idVendor;
                dev.productId = idProduct;

                devices.push_back(dev);
            }
        }

        return devices;
    }
}
