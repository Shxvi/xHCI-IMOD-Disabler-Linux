#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <algorithm>
#include "xhci_common.hpp"
#include "controller.hpp"
#include "backend.hpp"
#include "backup.hpp"
#include "tui.hpp"

namespace App
{
    UnifiedBackend backend;

    void PrintControllerResult(bool success, const std::string& bdf, const std::string& msg)
    {
        std::cout << "  " << (success ? std::string(Color::GREEN) + Glyph::CHECK : std::string(Color::RED) + Glyph::CROSS)
                  << Color::RESET << " [" << Color::BOLD << bdf << Color::RESET << "]: " << msg << "\n";
    }

    bool BackupAction(const std::vector<USBController>& controllers)
    {
        std::vector<BackupController> backups;
        bool anyAccessible = false;

        for (const auto& ctrl : controllers)
        {
            auto status = backend.GetStatus(ctrl);
            if (!status.accessible)
            {
                PrintControllerResult(false, ctrl.bdf, "Could not access controller: " + status.errorMessage);
                continue;
            }

            anyAccessible = true;
            BackupController bc;
            bc.bdf = ctrl.bdf;
            for (const auto& ir : status.interrupters)
            {
                BackupRegister reg;
                reg.address = ir.mmioAddress;
                reg.value = ir.imodValue;
                bc.registers.push_back(reg);
            }
            backups.push_back(bc);
            PrintControllerResult(true, ctrl.bdf, "Backed up " + std::to_string(bc.registers.size()) + " interrupter(s)");
        }

        if (anyAccessible && ConfigUtil::SaveBackup(backups))
        {
            std::cout << "\n" << Color::GREEN << Glyph::CHECK << " Backup saved to: " 
                      << ConfigUtil::GetBackupPath().string() << Color::RESET << "\n";
            return true;
        }

        std::cout << "\n" << Color::RED << Glyph::CROSS << " Failed to save backup or no controllers accessible.\n" << Color::RESET;
        return false;
    }

    bool RestoreAction(const std::vector<USBController>& controllers)
    {
        std::vector<BackupController> backups;
        if (!ConfigUtil::LoadBackup(backups))
        {
            std::cout << "\n" << Color::RED << Glyph::CROSS << " No valid backup found at: " 
                      << ConfigUtil::GetBackupPath().string() << Color::RESET << "\n";
            return false;
        }

        bool allOk = true;
        for (const auto& bc : backups)
        {
            auto it = std::find_if(controllers.begin(), controllers.end(), [&](const USBController& c) {
                return c.bdf == bc.bdf;
            });

            if (it == controllers.end())
            {
                PrintControllerResult(false, bc.bdf, "Controller not currently present");
                allOk = false;
                continue;
            }

            if (bc.registers.empty())
                continue;

            uint32_t targetVal = bc.registers[0].value;
            std::string err;
            if (backend.SetImod(*it, targetVal, err))
            {
                PrintControllerResult(true, bc.bdf, "Restored IMOD to " + HexString(targetVal, 4) + " (" + FormatIntervalUs(targetVal) + ")");
            }
            else
            {
                PrintControllerResult(false, bc.bdf, "Failed to restore: " + err);
                allOk = false;
            }
        }

        return allOk;
    }

    bool SetImodAction(const std::vector<USBController>& controllers, uint32_t interval, const std::string& actionName)
    {
        auto skipped = ConfigUtil::LoadSkipped();
        bool anyChanged = false;
        bool anyFailed = false;

        std::cout << "\n" << Color::BOLD << Glyph::POINTER << " Applying " << actionName << " (Target: " 
                  << HexString(interval, 4) << " / " << FormatIntervalUs(interval) << "):" << Color::RESET << "\n";

        for (const auto& ctrl : controllers)
        {
            if (skipped.find(ctrl.bdf) != skipped.end())
            {
                std::cout << "  " << Color::GRAY << Glyph::ARROW << " [" << ctrl.bdf << "]: Skipped (user preference)" << Color::RESET << "\n";
                continue;
            }

            auto st = backend.GetStatus(ctrl);
            if (st.accessible && !st.isMixed && st.commonInterval == interval)
            {
                PrintControllerResult(true, ctrl.bdf, "Already set to " + HexString(interval, 4));
                anyChanged = true;
                continue;
            }

            std::string err;
            if (backend.SetImod(ctrl, interval, err))
            {
                PrintControllerResult(true, ctrl.bdf, "Successfully set IMOD to " + HexString(interval, 4) + " (" + FormatIntervalUs(interval) + ")");
                anyChanged = true;
            }
            else
            {
                PrintControllerResult(false, ctrl.bdf, "Failed: " + err);
                anyFailed = true;
            }
        }

        return anyChanged && !anyFailed;
    }

    void ShowStatus(const std::vector<USBController>& controllers)
    {
        Tui::ClearScreen();
        Tui::PrintBanner();

        auto skipped = ConfigUtil::LoadSkipped();
        std::cout << Color::BOLD << "xHCI USB Host Controllers Status:" << Color::RESET << "\n\n";

        for (const auto& ctrl : controllers)
        {
            bool isSelected = (skipped.find(ctrl.bdf) == skipped.end());
            auto status = backend.GetStatus(ctrl);
            Tui::PrintControllerDetails(ctrl, status, isSelected);
        }

        Tui::PrintConnectionTip(controllers);
    }

    void ControllerSelectionMenu(std::vector<USBController>& controllers)
    {
        auto skipped = ConfigUtil::LoadSkipped();

        while (true)
        {
            Tui::ClearScreen();
            Tui::PrintBanner();
            std::cout << Color::BOLD << "Controller Selection (Toggle controllers to include/exclude):" << Color::RESET << "\n\n";

            std::vector<std::string> menuOptions;
            for (size_t i = 0; i < controllers.size(); ++i)
            {
                const auto& ctrl = controllers[i];
                bool isSelected = (skipped.find(ctrl.bdf) == skipped.end());
                std::string mark = isSelected ? std::string(Color::GREEN) + Glyph::CHECK : std::string(Color::RED) + Glyph::CROSS;
                menuOptions.push_back("[" + mark + Color::RESET + "] " + ctrl.bdf + " (" + ctrl.description + ")");
            }

            menuOptions.push_back("Select All");
            menuOptions.push_back("Deselect All");
            menuOptions.push_back("Save & Back");

            int choice = Tui::RunMenu("Choose an item to toggle:", menuOptions, 0);

            if (choice < 0 || choice == static_cast<int>(menuOptions.size()) - 1)
            {
                ConfigUtil::SaveSkipped(skipped);
                break;
            }
            else if (choice == static_cast<int>(menuOptions.size()) - 3) // Select All
            {
                skipped.clear();
            }
            else if (choice == static_cast<int>(menuOptions.size()) - 2) // Deselect All
            {
                for (const auto& ctrl : controllers)
                    skipped.insert(ctrl.bdf);
            }
            else if (choice >= 0 && choice < static_cast<int>(controllers.size()))
            {
                std::string bdf = controllers[choice].bdf;
                if (skipped.find(bdf) != skipped.end())
                    skipped.erase(bdf);
                else
                    skipped.insert(bdf);
            }
        }
    }

    void StartupMenu()
    {
        const std::string servicePath = "/etc/systemd/system/xhci-imod.service";
        while (true)
        {
            std::vector<std::string> options = {
                "1. Install & Enable Systemd Service (Disables IMOD on boot)",
                "2. Disable & Remove Systemd Service",
                "3. Back"
            };

            int choice = Tui::RunMenu("Startup Service Configuration (Linux systemd):", options, 0);
            if (choice == 0) // Install
            {
                Tui::ClearScreen();
                Tui::PrintBanner();
                if (getuid() != 0)
                {
                    std::cout << Color::RED << Glyph::CROSS << " Root permissions required to configure systemd service.\n"
                              << "Please run this tool with sudo: sudo ./xhci-imod-disabler" << Color::RESET << "\n";
                }
                else
                {
                    char selfPath[4096] = {0};
                    ssize_t len = readlink("/proc/self/exe", selfPath, sizeof(selfPath) - 1);
                    std::string execPath = (len > 0) ? std::string(selfPath, len) : "/usr/local/bin/xhci-imod-disabler";

                    std::ofstream svc(servicePath);
                    if (svc.is_open())
                    {
                        svc << "[Unit]\n";
                        svc << "Description=xHCI IMOD Disabler (USB Interrupt Moderation Zero Latency)\n";
                        svc << "After=systemd-udev-settle.service local-fs.target\n\n";
                        svc << "[Service]\n";
                        svc << "Type=oneshot\n";
                        svc << "ExecStart=" << execPath << " --silent\n";
                        svc << "RemainAfterExit=yes\n\n";
                        svc << "[Install]\n";
                        svc << "WantedBy=multi-user.target\n";
                        svc.close();

                        system("systemctl daemon-reload");
                        system("systemctl enable --now xhci-imod.service");

                        std::cout << Color::GREEN << Glyph::CHECK << " Systemd service installed and enabled successfully!\n"
                                  << "Service file: " << servicePath << Color::RESET << "\n";
                    }
                    else
                    {
                        std::cout << Color::RED << Glyph::CROSS << " Failed to write service file: " << servicePath << Color::RESET << "\n";
                    }
                }
                Tui::WaitForKey();
            }
            else if (choice == 1) // Remove
            {
                Tui::ClearScreen();
                Tui::PrintBanner();
                if (getuid() != 0)
                {
                    std::cout << Color::RED << Glyph::CROSS << " Root permissions required to remove systemd service." << Color::RESET << "\n";
                }
                else
                {
                    system("systemctl disable --now xhci-imod.service 2>/dev/null");
                    unlink(servicePath.c_str());
                    system("systemctl daemon-reload");
                    std::cout << Color::GREEN << Glyph::CHECK << " Systemd service disabled and removed." << Color::RESET << "\n";
                }
                Tui::WaitForKey();
            }
            else
            {
                break;
            }
        }
    }

    void ImodMenu(const std::vector<USBController>& controllers)
    {
        while (true)
        {
            std::vector<std::string> options = {
                "1. Backup Current IMOD Values",
                "2. Restore IMOD Values from Backup",
                "3. Test IMOD (62.5 Hz / 16 ms latency)",
                "4. Disable IMOD (0 µs / Zero Moderation)",
                "5. Set Custom IMOD Value",
                "6. Back"
            };

            int choice = Tui::RunMenu("IMOD Modification Options:", options, 3);
            Tui::ClearScreen();
            Tui::PrintBanner();

            if (choice == 0) // Backup
            {
                BackupAction(controllers);
                Tui::WaitForKey();
            }
            else if (choice == 1) // Restore
            {
                RestoreAction(controllers);
                Tui::WaitForKey();
            }
            else if (choice == 2) // Test (62.5 Hz)
            {
                SetImodAction(controllers, xHCI::TEST_IMOD_INTERVAL, "Test IMOD (16 ms / 62.5 Hz)");
                Tui::WaitForKey();
            }
            else if (choice == 3) // Disable
            {
                SetImodAction(controllers, xHCI::DESIRED_IMOD_INTERVAL, "Disable IMOD (0 µs)");
                Tui::WaitForKey();
            }
            else if (choice == 4) // Custom
            {
                std::cout << "Enter custom IMOD interval in 250ns units:\n"
                          << "  Examples: 0 = Disable, 0x00A0 (160) = Linux default (40 µs), 0x00C8 (200) = Windows default (50 µs)\n"
                          << "Value [hex or dec]: " << std::flush;
                std::string input;
                std::cin >> input;
                uint32_t val = 0;
                try {
                    val = std::stoul(input, nullptr, 0);
                    SetImodAction(controllers, val, "Custom Interval");
                } catch (...) {
                    std::cout << Color::RED << Glyph::CROSS << " Invalid number format." << Color::RESET << "\n";
                }
                Tui::WaitForKey();
            }
            else
            {
                break;
            }
        }
    }

    void MainMenu()
    {
        while (true)
        {
            auto controllers = ControllerUtil::ScanControllers();
            if (controllers.empty())
            {
                Tui::ClearScreen();
                Tui::PrintBanner();
                std::cout << Color::RED << Glyph::CROSS << " No xHCI USB controllers detected in /sys/bus/pci/devices!" << Color::RESET << "\n";
                Tui::WaitForKey();
                return;
            }

            std::vector<std::string> options = {
                "1. Controller Status & USB Topology",
                "2. IMOD Options (Disable / Test / Restore / Backup)",
                "3. Controller Selection (Include / Exclude)",
                "4. Startup Options (Systemd Service)",
                "5. Exit"
            };

            int choice = Tui::RunMenu("Main Menu:", options, 1);
            if (choice == 0)
            {
                ShowStatus(controllers);
                Tui::WaitForKey();
            }
            else if (choice == 1)
            {
                ImodMenu(controllers);
            }
            else if (choice == 2)
            {
                ControllerSelectionMenu(controllers);
            }
            else if (choice == 3)
            {
                StartupMenu();
            }
            else
            {
                Tui::ClearScreen();
                break;
            }
        }
    }
}

int main(int argc, char* argv[])
{
    // CLI mode checks
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--silent" || arg == "-s")
        {
            auto controllers = ControllerUtil::ScanControllers();
            return App::SetImodAction(controllers, xHCI::DESIRED_IMOD_INTERVAL, "Silent Disable") ? 0 : 1;
        }
        else if (arg == "--status")
        {
            auto controllers = ControllerUtil::ScanControllers();
            App::ShowStatus(controllers);
            return 0;
        }
        else if (arg == "--disable")
        {
            auto controllers = ControllerUtil::ScanControllers();
            return App::SetImodAction(controllers, xHCI::DESIRED_IMOD_INTERVAL, "Disable IMOD") ? 0 : 1;
        }
        else if (arg == "--restore")
        {
            auto controllers = ControllerUtil::ScanControllers();
            return App::RestoreAction(controllers) ? 0 : 1;
        }
        else if (arg == "--test")
        {
            auto controllers = ControllerUtil::ScanControllers();
            return App::SetImodAction(controllers, xHCI::TEST_IMOD_INTERVAL, "Test IMOD") ? 0 : 1;
        }
        else if (arg == "--set" && i + 1 < argc)
        {
            uint32_t val = std::stoul(argv[++i], nullptr, 0);
            auto controllers = ControllerUtil::ScanControllers();
            return App::SetImodAction(controllers, val, "Set IMOD") ? 0 : 1;
        }
        else if (arg == "--help" || arg == "-h")
        {
            std::cout << "xHCI IMOD Disabler for Linux " << xHCI::VERSION << "\n"
                      << "Usage:\n"
                      << "  " << argv[0] << " [options]\n\n"
                      << "Options:\n"
                      << "  --silent, -s       Headless disable without UI (used by systemd)\n"
                      << "  --status           Print current controllers, IMOD values, and USB devices\n"
                      << "  --disable          Disable IMOD (set to 0 µs)\n"
                      << "  --restore          Restore IMOD values from backup\n"
                      << "  --test             Set test IMOD interval (16 ms / 62.5 Hz)\n"
                      << "  --set <val>        Set custom IMOD interval (e.g. 0x00A0 or 160)\n"
                      << "  --help, -h         Show this help message\n";
            return 0;
        }
    }

    App::MainMenu();
    return 0;
}
