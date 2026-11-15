#pragma once

#include <cstdint>
#include <string>
#include "xhci_common.hpp"

struct ControllerModel
{
    uint16_t vendor;
    uint16_t device;
    const char* location;
    const char* platform;
};

constexpr const char* LOC_CPU = "CPU-direct";
constexpr const char* LOC_TB = "Thunderbolt";
constexpr const char* LOC_CHIPSET = "Chipset";
constexpr const char* LOC_ADDIN = "Add-in card";

constexpr ControllerModel CONTROLLER_MODELS[] =
{
    // Intel, inside the CPU
    { 0x8086, 0x8A13, LOC_CPU, "Intel Ice Lake" },
    { 0x8086, 0x9A13, LOC_CPU, "Intel Tiger Lake-LP" },
    { 0x8086, 0x9A17, LOC_CPU, "Intel Tiger Lake-H" },
    { 0x8086, 0x461E, LOC_CPU, "Intel Alder Lake-P" },
    { 0x8086, 0x464E, LOC_CPU, "Intel Alder Lake-N" },
    { 0x8086, 0xA71E, LOC_CPU, "Intel Raptor Lake-P" },
    { 0x8086, 0x7EC0, LOC_CPU, "Intel Meteor Lake-P" },
    { 0x8086, 0xA831, LOC_CPU, "Intel Lunar Lake-M" },
    // Intel, chipset (PCH)
    { 0x8086, 0x7F6E, LOC_CHIPSET, "Intel 800 Series PCH" },
    { 0x8086, 0x7A60, LOC_CHIPSET, "Intel 700 Series PCH" },
    { 0x8086, 0x7A61, LOC_CHIPSET, "Intel 700 Series PCH" },
    { 0x8086, 0x7AE0, LOC_CHIPSET, "Intel 600 Series PCH (Desktop)" },
    { 0x8086, 0x51ED, LOC_CHIPSET, "Intel 600 Series PCH" },
    { 0x8086, 0x54ED, LOC_CHIPSET, "Intel Alder Lake-N PCH" },
    { 0x8086, 0x7E7D, LOC_CHIPSET, "Intel Meteor Lake PCH" },
    { 0x8086, 0x777D, LOC_CHIPSET, "Intel Arrow Lake" },
    { 0x8086, 0xA87D, LOC_CHIPSET, "Intel Lunar Lake PCH" },
    { 0x8086, 0xA0ED, LOC_CHIPSET, "Intel 500 Series PCH" },
    { 0x8086, 0x43ED, LOC_CHIPSET, "Intel 500 Series PCH-H" },
    { 0x8086, 0xA3AF, LOC_CHIPSET, "Intel 400 Series PCH" },
    { 0x8086, 0x02ED, LOC_CHIPSET, "Intel 400 Series PCH-LP" },
    { 0x8086, 0x06ED, LOC_CHIPSET, "Intel 400 Series PCH" },
    { 0x8086, 0xA36D, LOC_CHIPSET, "Intel 300 Series PCH" },
    { 0x8086, 0x9DED, LOC_CHIPSET, "Intel 300 Series PCH-LP" },
    { 0x8086, 0xA2AF, LOC_CHIPSET, "Intel 200 Series PCH" },
    { 0x8086, 0xA12F, LOC_CHIPSET, "Intel 100 Series PCH" },
    { 0x8086, 0x9D2F, LOC_CHIPSET, "Intel 100 Series PCH-LP" },
    { 0x8086, 0x8CB1, LOC_CHIPSET, "Intel 9 Series PCH" },
    { 0x8086, 0x9CB1, LOC_CHIPSET, "Intel 9 Series PCH-LP" },
    { 0x8086, 0x8C31, LOC_CHIPSET, "Intel 8 Series PCH" },
    { 0x8086, 0x9C31, LOC_CHIPSET, "Intel 8 Series PCH-LP" },
    { 0x8086, 0x1E31, LOC_CHIPSET, "Intel 7 Series PCH" },
    { 0x8086, 0x8D31, LOC_CHIPSET, "Intel X99/C610" },
    { 0x8086, 0xA1AF, LOC_CHIPSET, "Intel C620" },
    // Intel Thunderbolt
    { 0x8086, 0x5782, LOC_TB, "Intel Barlow Ridge Host 80G" },
    { 0x8086, 0x5785, LOC_TB, "Intel Barlow Ridge Host 40G" },
    { 0x8086, 0x5787, LOC_TB, "Intel Barlow Ridge Hub 80G" },
    { 0x8086, 0x57A5, LOC_TB, "Intel Barlow Ridge Hub 40G" },
    { 0x8086, 0x1138, LOC_TB, "Intel Maple Ridge 4C" },
    { 0x8086, 0x1135, LOC_TB, "Intel Maple Ridge 2C" },
    { 0x8086, 0x0B27, LOC_TB, "Intel Goshen Ridge" },
    { 0x8086, 0x15E9, LOC_TB, "Intel Titan Ridge 2C" },
    { 0x8086, 0x15EC, LOC_TB, "Intel Titan Ridge 4C" },
    { 0x8086, 0x15F0, LOC_TB, "Intel Titan Ridge DD" },
    { 0x8086, 0x15B5, LOC_TB, "Intel Alpine Ridge 2C" },
    { 0x8086, 0x15B6, LOC_TB, "Intel Alpine Ridge 4C" },
    { 0x8086, 0x15C1, LOC_TB, "Intel Alpine Ridge LP" },
    { 0x8086, 0x15D4, LOC_TB, "Intel Alpine Ridge 4C" },
    { 0x8086, 0x15DB, LOC_TB, "Intel Alpine Ridge 2C" },
    // AMD, inside the CPU
    { 0x1022, 0x15B6, LOC_CPU, "AMD Ryzen 7000/9000 (AM5)" },
    { 0x1022, 0x15B7, LOC_CPU, "AMD Ryzen 7000/9000 (AM5)" },
    { 0x1022, 0x15B8, LOC_CPU, "AMD Ryzen 7000/9000 (AM5)" },
    { 0x1022, 0x1587, LOC_CPU, "AMD Strix Halo" },
    { 0x1022, 0x1588, LOC_CPU, "AMD Strix Halo" },
    { 0x1022, 0x1589, LOC_CPU, "AMD Strix Halo" },
    { 0x1022, 0x158B, LOC_CPU, "AMD Strix Halo" },
    { 0x1022, 0x158D, LOC_CPU, "AMD Strix Halo" },
    { 0x1022, 0x158E, LOC_CPU, "AMD Strix Halo" },
    { 0x1022, 0x161A, LOC_CPU, "AMD Ryzen 6000 Mobile" },
    { 0x1022, 0x161B, LOC_CPU, "AMD Ryzen 6000 Mobile" },
    { 0x1022, 0x161C, LOC_CPU, "AMD Ryzen 6000 Mobile" },
    { 0x1022, 0x161D, LOC_CPU, "AMD Ryzen 6000 Mobile" },
    { 0x1022, 0x161E, LOC_CPU, "AMD Ryzen 6000 Mobile" },
    { 0x1022, 0x161F, LOC_CPU, "AMD Ryzen 6000 Mobile" },
    { 0x1022, 0x15D6, LOC_CPU, "AMD Ryzen 6000 Mobile" },
    { 0x1022, 0x15D7, LOC_CPU, "AMD Ryzen 6000 Mobile" },
    { 0x1022, 0x162E, LOC_CPU, "AMD Ryzen 6000 Mobile" },
    { 0x1022, 0x162F, LOC_CPU, "AMD Ryzen 6000 Mobile" },
    { 0x1022, 0x15C4, LOC_CPU, "AMD Ryzen 7040 Mobile" },
    { 0x1022, 0x15C5, LOC_CPU, "AMD Ryzen 7040 Mobile" },
    { 0x1022, 0x1668, LOC_CPU, "AMD Pink Sardine" },
    { 0x1022, 0x1669, LOC_CPU, "AMD Pink Sardine" },
    { 0x1022, 0x1639, LOC_CPU, "AMD Ryzen 4000/5000 APU" },
    { 0x1022, 0x15E0, LOC_CPU, "AMD Ryzen 2000 APU" },
    { 0x1022, 0x15E1, LOC_CPU, "AMD Ryzen 2000 APU" },
    { 0x1022, 0x15E5, LOC_CPU, "AMD Ryzen 3000 APU" },
    { 0x1022, 0x149C, LOC_CPU, "AMD Ryzen 3000/5000 (AM4)" },
    { 0x1022, 0x148C, LOC_CPU, "AMD EPYC Rome / Threadripper 3000" },
    { 0x1022, 0x145F, LOC_CPU, "AMD Ryzen 1000" },
    { 0x1022, 0x145C, LOC_CPU, "AMD Ryzen 1000" },
    { 0x1022, 0x162C, LOC_CPU, "AMD Steam Deck (Van Gogh)" },
    { 0x1022, 0x163A, LOC_CPU, "AMD Steam Deck (Van Gogh)" },
    { 0x1022, 0x163B, LOC_CPU, "AMD Steam Deck (Van Gogh)" },
    { 0x1022, 0x15D4, LOC_CPU, "AMD FireFlight" },
    { 0x1022, 0x15D5, LOC_CPU, "AMD FireFlight" },
    { 0x1022, 0x13ED, LOC_CPU, "AMD Ariel" },
    { 0x1022, 0x13EE, LOC_CPU, "AMD Ariel" },
    { 0x1022, 0x1557, LOC_CPU, "AMD EPYC Turin" },
    // AMD, chipset
    { 0x1022, 0x43FC, LOC_CHIPSET, "AMD X870/B850 (AM5)" },
    { 0x1022, 0x43FD, LOC_CHIPSET, "AMD X870/B850 (AM5)" },
    { 0x1022, 0x43F7, LOC_CHIPSET, "AMD X670/B650 (AM5)" },
    { 0x1022, 0x43EE, LOC_CHIPSET, "AMD X570/B550 (AM4)" },
    { 0x1022, 0x43EC, LOC_CHIPSET, "AMD A520 (AM4)" },
    { 0x1022, 0x43D5, LOC_CHIPSET, "AMD X470/B450 (AM4)" },
    { 0x1022, 0x43B9, LOC_CHIPSET, "AMD X370 (AM4)" },
    { 0x1022, 0x43BA, LOC_CHIPSET, "AMD X399 (Threadripper)" },
    { 0x1022, 0x43BB, LOC_CHIPSET, "AMD B350 (AM4)" },
    { 0x1022, 0x43BC, LOC_CHIPSET, "AMD A320 (AM4)" },
    { 0x1022, 0x7814, LOC_CHIPSET, "AMD FCH" },
    { 0x1022, 0x7812, LOC_CHIPSET, "AMD FCH" },
};

struct AddInVendor
{
    uint16_t vendor;
    const char* name;
};

constexpr AddInVendor ADDIN_VENDORS[] =
{
    { 0x1B21, "ASMedia" },
    { 0x1106, "VIA" },
    { 0x1B73, "Fresco Logic" },
    { 0x1912, "Renesas" },
    { 0x1B6F, "Etron" },
    { 0x104C, "Texas Instruments" },
};

inline std::string ControllerLocation(uint16_t vendor, uint16_t device)
{
    const std::string arrow = std::string(" ") + Glyph::ARROW + " ";

    for (const auto& model : CONTROLLER_MODELS)
    {
        if (model.vendor == vendor && model.device == device)
            return std::string(model.location) + arrow + model.platform;
    }

    for (const auto& addIn : ADDIN_VENDORS)
    {
        if (addIn.vendor == vendor)
            return std::string(LOC_ADDIN) + arrow + addIn.name;
    }

    return "Unknown Location";
}

inline int ChipsToCpu(uint16_t vendor, uint16_t device)
{
    for (const auto& model : CONTROLLER_MODELS)
    {
        if (model.vendor == vendor && model.device == device)
            return (model.location == LOC_CPU || model.location == LOC_TB) ? 0 : 1;
    }

    for (const auto& addIn : ADDIN_VENDORS)
    {
        if (addIn.vendor == vendor)
            return 1;
    }

    return -1;
}
