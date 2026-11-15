#pragma once

#include <cstdint>
#include <string>
#include <sstream>
#include <iomanip>

namespace xHCI
{
    constexpr const char* VERSION = "V1.2 (Linux Port)";

    // IMOD interval values, in 250 ns units.
    constexpr uint32_t DESIRED_IMOD_INTERVAL = 0x0000;      // Moderation off (0 us)
    constexpr uint32_t TEST_IMOD_INTERVAL = 0xFA00;         // 16 ms, ~62.5 Hz
    constexpr uint32_t LINUX_DEFAULT_INTERVAL = 0x00A0;     // 40 us (Linux kernel default)

    // Mask for the interval (IMODI: bits 15:0)
    constexpr uint32_t IMOD_INTERVAL_MASK = 0x0000FFFF;

    // PCI class code for xHCI USB controller
    constexpr uint32_t PCI_CLASS_XHCI = 0x0C0330;

    // xHCI register offsets (xHCI specification sections 5.3 & 5.5)
    constexpr uint32_t CAPABILITY_WINDOW_SIZE = 0x1000;
    constexpr uint32_t HCSPARAMS1_OFFSET = 0x04;
    constexpr uint32_t RTSOFF_OFFSET = 0x18;
    constexpr uint32_t INTERRUPTER_OFFSET = 0x20;
    constexpr uint32_t INTERRUPTER_SIZE = 0x20;
    constexpr uint32_t IMAN_OFFSET = 0x00;
    constexpr uint32_t IMOD_OFFSET = 0x04;
    constexpr uint32_t MAX_INTERRUPTERS = 1024;
}

namespace Glyph
{
    constexpr const char* CHECK = "✔";           // U+2714
    constexpr const char* CROSS = "✖";           // U+2716
    constexpr const char* ARROW = "➔";           // U+2794
    constexpr const char* POINTER = "►";         // U+25BA
    constexpr const char* LEFT_POINTER = "◄";    // U+25C4
    constexpr const char* DOT = "•";             // U+2022
}

namespace Color
{
    constexpr const char* RESET = "\033[0m";
    constexpr const char* BOLD = "\033[1m";
    constexpr const char* DIM = "\033[2m";
    constexpr const char* RED = "\033[38;2;255;107;107m";
    constexpr const char* GREEN = "\033[38;2;0;255;135m";
    constexpr const char* ORANGE = "\033[38;2;255;179;71m";
    constexpr const char* BLUE = "\033[38;2;100;180;255m";
    constexpr const char* MAGENTA = "\033[38;2;220;120;255m";
    constexpr const char* CYAN = "\033[38;2;100;220;240m";
    constexpr const char* WHITE = "\033[97m";
    constexpr const char* GRAY = "\033[90m";
}

inline std::string HexString(uint64_t value, int width = 0)
{
    std::ostringstream stream;
    stream << "0x" << std::uppercase << std::hex;
    if (width > 0)
        stream << std::setw(width) << std::setfill('0');
    stream << value;
    return stream.str();
}

inline std::string FormatIntervalUs(uint32_t interval)
{
    double us = interval * 0.25;
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << us << " µs";
    return ss.str();
}

inline std::string FormatFrequency(uint32_t interval)
{
    if (interval == 0)
        return "Immediate (No Throttle)";
    double hz = 1.0 / (interval * 0.25e-6);
    std::ostringstream ss;
    if (hz >= 1000.0)
        ss << std::fixed << std::setprecision(1) << (hz / 1000.0) << " kHz";
    else
        ss << std::fixed << std::setprecision(1) << hz << " Hz";
    return ss.str();
}
