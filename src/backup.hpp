#pragma once

#include <string>
#include <vector>
#include <set>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <unistd.h>
#include "xhci_common.hpp"

struct BackupRegister
{
    uint64_t address = 0;
    uint32_t value = 0;
};

struct BackupController
{
    std::string bdf;
    std::vector<BackupRegister> registers;
};

namespace ConfigUtil
{
    inline std::filesystem::path GetConfigDir()
    {
        std::filesystem::path configDir;
        if (getuid() == 0)
        {
            configDir = "/etc/xhci-imod";
        }
        else
        {
            const char* xdg = std::getenv("XDG_CONFIG_HOME");
            if (xdg && *xdg)
                configDir = std::filesystem::path(xdg) / "xhci-imod";
            else
            {
                const char* home = std::getenv("HOME");
                if (home && *home)
                    configDir = std::filesystem::path(home) / ".config" / "xhci-imod";
                else
                    configDir = "/tmp/xhci-imod";
            }
        }

        std::error_code ec;
        std::filesystem::create_directories(configDir, ec);
        return configDir;
    }

    inline std::filesystem::path GetBackupPath()
    {
        return GetConfigDir() / "backup.json";
    }

    inline std::filesystem::path GetSkippedPath()
    {
        return GetConfigDir() / "skipped.txt";
    }

    inline std::set<std::string> LoadSkipped()
    {
        std::set<std::string> skipped;
        std::ifstream file(GetSkippedPath());
        if (!file.is_open())
            return skipped;

        std::string line;
        while (std::getline(file, line))
        {
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' '))
                line.pop_back();
            if (!line.empty() && line[0] != '#')
                skipped.insert(line);
        }
        return skipped;
    }

    inline bool SaveSkipped(const std::set<std::string>& skipped)
    {
        std::ofstream file(GetSkippedPath());
        if (!file.is_open())
            return false;

        file << "# xHCI IMOD Disabler - Skipped Controllers List\n";
        for (const auto& bdf : skipped)
            file << bdf << "\n";
        return true;
    }

    inline bool SaveBackup(const std::vector<BackupController>& backups)
    {
        std::ofstream file(GetBackupPath());
        if (!file.is_open())
            return false;

        file << "{\n";
        file << "  \"version\": 1,\n";
        file << "  \"controllers\": [\n";

        for (size_t i = 0; i < backups.size(); ++i)
        {
            const auto& ctrl = backups[i];
            file << "    {\n";
            file << "      \"bdf\": \"" << ctrl.bdf << "\",\n";
            file << "      \"registers\": [\n";
            for (size_t j = 0; j < ctrl.registers.size(); ++j)
            {
                const auto& reg = ctrl.registers[j];
                file << "        { \"address\": \"" << HexString(reg.address) << "\", \"value\": \"" << HexString(reg.value) << "\" }";
                if (j + 1 < ctrl.registers.size())
                    file << ",";
                file << "\n";
            }
            file << "      ]\n";
            file << "    }";
            if (i + 1 < backups.size())
                file << ",";
            file << "\n";
        }

        file << "  ]\n";
        file << "}\n";
        return true;
    }

    inline bool LoadBackup(std::vector<BackupController>& backups)
    {
        backups.clear();
        std::ifstream file(GetBackupPath());
        if (!file.is_open())
            return false;

        std::string line;
        BackupController currentCtrl;
        bool inController = false;

        while (std::getline(file, line))
        {
            size_t bdfPos = line.find("\"bdf\": \"");
            if (bdfPos != std::string::npos)
            {
                if (inController && !currentCtrl.bdf.empty())
                    backups.push_back(currentCtrl);
                currentCtrl = BackupController{};
                inController = true;
                size_t start = bdfPos + 8;
                size_t end = line.find('"', start);
                if (end != std::string::npos)
                    currentCtrl.bdf = line.substr(start, end - start);
                continue;
            }

            size_t valPos = line.find("\"value\": \"");
            size_t addrPos = line.find("\"address\": \"");
            if (valPos != std::string::npos && addrPos != std::string::npos)
            {
                size_t addrStart = addrPos + 12;
                size_t addrEnd = line.find('"', addrStart);
                size_t valStart = valPos + 10;
                size_t valEnd = line.find('"', valStart);

                if (addrEnd != std::string::npos && valEnd != std::string::npos)
                {
                    BackupRegister reg;
                    try {
                        reg.address = std::stoull(line.substr(addrStart, addrEnd - addrStart), nullptr, 16);
                        reg.value = std::stoul(line.substr(valStart, valEnd - valStart), nullptr, 16);
                        currentCtrl.registers.push_back(reg);
                    } catch (...) {}
                }
            }
        }

        if (inController && !currentCtrl.bdf.empty())
            backups.push_back(currentCtrl);

        return !backups.empty();
    }
}
