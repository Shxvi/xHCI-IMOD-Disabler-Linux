#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "xhci_common.hpp"
#include "controller.hpp"
#include "backend.hpp"
#include "backup.hpp"

class TerminalRawMode
{
public:
    TerminalRawMode()
    {
        if (isatty(STDIN_FILENO))
        {
            tcgetattr(STDIN_FILENO, &orig_termios);
            struct termios raw = orig_termios;
            raw.c_lflag &= ~(ECHO | ICANON);
            raw.c_cc[VMIN] = 1;
            raw.c_cc[VTIME] = 0;
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
            active = true;
        }
    }

    ~TerminalRawMode()
    {
        if (active)
        {
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        }
    }

private:
    struct termios orig_termios {};
    bool active = false;
};

namespace Tui
{
    inline void ClearScreen()
    {
        std::cout << "\033[2J\033[H" << std::flush;
    }

    inline void HideCursor()
    {
        std::cout << "\033[?25l" << std::flush;
    }

    inline void ShowCursor()
    {
        std::cout << "\033[?25h" << std::flush;
    }

    inline void PrintBanner()
    {
        std::cout << Color::CYAN << Color::BOLD << Glyph::POINTER << " xHCI IMOD Disabler " 
                  << xHCI::VERSION << Color::RESET << "\n\n";
    }

    inline int ReadKey()
    {
        TerminalRawMode raw;
        char c = 0;
        if (read(STDIN_FILENO, &c, 1) <= 0)
            return -1;

        if (c == '\033') // Escape sequence
        {
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) <= 0) return '\033';
            if (read(STDIN_FILENO, &seq[1], 1) <= 0) return '\033';

            if (seq[0] == '[')
            {
                if (seq[1] == 'A') return 1000; // UP
                if (seq[1] == 'B') return 1001; // DOWN
                if (seq[1] == 'C') return 1002; // RIGHT
                if (seq[1] == 'D') return 1003; // LEFT
            }
            return '\033';
        }

        if (c == '\r' || c == '\n')
            return 10; // ENTER

        return c;
    }

    inline void WaitForKey()
    {
        std::cout << "\n" << Color::DIM << Glyph::ARROW << " Press any key to return..." << Color::RESET << std::flush;
        ReadKey();
        std::cout << "\n";
    }

    inline std::string ChipColor(int chips)
    {
        if (chips == 0) return Color::GREEN;
        if (chips == 1) return Color::ORANGE;
        return Color::RED;
    }

    inline void PrintControllerDetails(const USBController& ctrl, const ControllerImodStatus& status, bool isSelected)
    {
        std::string selMark = isSelected ? std::string(Color::GREEN) + Glyph::CHECK + Color::RESET
                                         : std::string(Color::RED) + Glyph::CROSS + Color::RESET;

        std::cout << "  " << selMark << "  " << Color::BOLD << ctrl.bdf << Color::RESET 
                  << " - " << Color::WHITE << ctrl.description << Color::RESET << "\n";

        std::cout << "      " << Color::DIM << "Location:" << Color::RESET << " " 
                  << ChipColor(ctrl.chipsToCpu) << ctrl.location << Color::RESET 
                  << Color::DIM << " (" << (ctrl.chipsToCpu == 0 ? "0 chips to CPU" : "1 chip to CPU") << ")" << Color::RESET << "\n";

        std::cout << "      " << Color::DIM << "Interrupt Mode:" << Color::RESET << " " << ctrl.interruptMode << "\n";

        std::cout << "      " << Color::DIM << "Current IMOD:" << Color::RESET << " ";
        if (!status.accessible)
        {
            std::cout << Color::GRAY << "Unknown (" << status.errorMessage << ")" << Color::RESET << "\n";
        }
        else if (status.commonInterval == 0)
        {
            std::cout << Color::GREEN << Color::BOLD << "Disabled (0 µs / 0x0000)" << Color::RESET << "\n";
        }
        else
        {
            std::cout << Color::ORANGE << HexString(status.commonInterval, 4) << Color::RESET 
                      << " (" << FormatIntervalUs(status.commonInterval) << " - " 
                      << FormatFrequency(status.commonInterval) << ")\n";
        }

        if (!ctrl.attachedDevices.empty())
        {
            std::cout << "      " << Color::DIM << "Connected Devices (" << ctrl.attachedDevices.size() << "):" << Color::RESET << "\n";
            for (const auto& dev : ctrl.attachedDevices)
            {
                std::string icon = dev.isMouse ? "  🐭" : (dev.isKeyboard ? "  ⌨ " : "   •");
                std::cout << "      " << icon << " " << Color::WHITE << dev.name << Color::RESET 
                          << " " << Color::CYAN << "[" << dev.types << "]" << Color::RESET;
                if (dev.port > 0)
                    std::cout << Color::DIM << " (Port " << dev.port << ")" << Color::RESET;
                if (!dev.hubs.empty())
                    std::cout << Color::RED << " [Via Hub]" << Color::RESET;
                std::cout << "\n";
            }
        }
        else
        {
            std::cout << "      " << Color::DIM << "No USB devices connected" << Color::RESET << "\n";
        }
        std::cout << "\n";
    }

    inline void PrintConnectionTip(const std::vector<USBController>& controllers)
    {
        const USBController* mouseCtrl = nullptr;
        const UsbDevice* mouseDev = nullptr;
        size_t mouseCtrlTotalDevices = 0;

        for (const auto& ctrl : controllers)
        {
            for (const auto& dev : ctrl.attachedDevices)
            {
                if (dev.isMouse)
                {
                    mouseCtrl = &ctrl;
                    mouseDev = &dev;
                    mouseCtrlTotalDevices = ctrl.attachedDevices.size();
                    break;
                }
            }
            if (mouseCtrl) break;
        }

        if (!mouseCtrl)
            return;

        std::cout << Color::CYAN << Color::BOLD << Glyph::POINTER << " Latency Optimization Tip:" << Color::RESET << "\n";
        std::cout << "  Mouse detected: " << Color::WHITE << mouseDev->name << Color::RESET 
                  << " on controller " << Color::BOLD << mouseCtrl->bdf << Color::RESET << " (" 
                  << ChipColor(mouseCtrl->chipsToCpu) << mouseCtrl->location << Color::RESET << ").\n";

        if (!mouseDev->hubs.empty())
        {
            std::cout << "  " << Color::RED << Glyph::CROSS << " WARNING: Your mouse is plugged into an external USB Hub! "
                      << "Plug it directly into a motherboard USB port to minimize latency." << Color::RESET << "\n";
        }
        else if (mouseCtrl->chipsToCpu == 0)
        {
            std::cout << "  " << Color::GREEN << Glyph::CHECK << " Great! Your mouse is connected to a CPU-direct controller. "
                      << "Disabling IMOD here yields maximum responsiveness." << Color::RESET << "\n";
        }
        else
        {
            std::cout << "  " << Color::ORANGE << Glyph::CHECK << " Your mouse is connected to a Chipset-routed controller. "
                      << "If your PC has a CPU-direct port, moving it there can further shave ~1-2 µs." << Color::RESET << "\n";
        }

        if (mouseCtrlTotalDevices > 2)
        {
            std::cout << "  " << Color::DIM << "Note: " << mouseCtrlTotalDevices 
                      << " devices share this controller. Keep it dedicated to mouse/keyboard for lowest jitter." 
                      << Color::RESET << "\n";
        }
        std::cout << "\n";
    }

    inline int RunMenu(const std::string& title, const std::vector<std::string>& options, int defaultIndex = 0)
    {
        int selected = defaultIndex;
        int count = static_cast<int>(options.size());

        while (true)
        {
            ClearScreen();
            PrintBanner();
            if (!title.empty())
                std::cout << Color::BOLD << title << Color::RESET << "\n\n";

            for (int i = 0; i < count; ++i)
            {
                if (i == selected)
                {
                    std::cout << Color::CYAN << Color::BOLD << " " << Glyph::POINTER << " " 
                              << options[i] << Color::RESET << "\n";
                }
                else
                {
                    std::cout << "   " << options[i] << "\n";
                }
            }

            std::cout << "\n" << Color::DIM << "Use [↑/↓] or [1-" << count << "] to select, [Enter] to confirm." << Color::RESET << "\n";

            int key = ReadKey();
            if (key == 1000) // UP
            {
                selected = (selected - 1 + count) % count;
            }
            else if (key == 1001) // DOWN
            {
                selected = (selected + 1) % count;
            }
            else if (key >= '1' && key <= '0' + count)
            {
                return (key - '1');
            }
            else if (key == 10) // ENTER
            {
                return selected;
            }
            else if (key == 'q' || key == 'Q' || key == '\033')
            {
                return count - 1; // Return last option (typically Back / Exit)
            }
        }
    }
}
