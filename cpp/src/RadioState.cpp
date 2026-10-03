#include "hpsdr/RadioState.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <random>
#include <stdexcept>

namespace hpsdr {

HwInfo hwInfo(HpsdrHw hw) {
    switch (hw) {
    case HpsdrHw::Atlas:       return {hw, 0, 2};
    case HpsdrHw::Hermes:      return {hw, 1, 4};
    case HpsdrHw::HermesII:    return {hw, 2, 4};
    case HpsdrHw::Angelia:     return {hw, 3, 5};
    case HpsdrHw::Orion:       return {hw, 4, 5};
    case HpsdrHw::OrionMkII:   return {hw, 5, 8};
    case HpsdrHw::HermesLite:  return {hw, 6, 2};
    case HpsdrHw::Saturn:      return {hw, 10, 10};
    case HpsdrHw::SaturnMkII:  return {hw, 11, 10};
    }
    throw std::invalid_argument("unknown HPSDR hardware");
}

HpsdrHw parseRadioName(const std::string& input) {
    std::string name;
    name.reserve(input.size());
    for (unsigned char c : input)
        name.push_back(static_cast<char>(std::tolower(c)));

    if (name == "atlas") return HpsdrHw::Atlas;
    if (name == "hermes") return HpsdrHw::Hermes;
    if (name == "hermesii") return HpsdrHw::HermesII;
    if (name == "angelia") return HpsdrHw::Angelia;
    if (name == "orion") return HpsdrHw::Orion;
    if (name == "orionmkii") return HpsdrHw::OrionMkII;
    if (name == "hermeslite") return HpsdrHw::HermesLite;
    if (name == "saturn") return HpsdrHw::Saturn;
    if (name == "saturnmkii") return HpsdrHw::SaturnMkII;
    throw std::invalid_argument("unknown radio type: " + input);
}

std::string radioName(HpsdrHw hw) {
    switch (hw) {
    case HpsdrHw::Atlas: return "atlas";
    case HpsdrHw::Hermes: return "hermes";
    case HpsdrHw::HermesII: return "hermesii";
    case HpsdrHw::Angelia: return "angelia";
    case HpsdrHw::Orion: return "orion";
    case HpsdrHw::OrionMkII: return "orionmkii";
    case HpsdrHw::HermesLite: return "hermeslite";
    case HpsdrHw::Saturn: return "saturn";
    case HpsdrHw::SaturnMkII: return "saturnmkii";
    }
    return "unknown";
}

std::uint32_t RadioState::nextSeq(const std::string& stream) {
    auto& value = sequence[stream];
    const auto result = value;
    value = value + 1U;
    return result;
}

std::array<std::uint8_t, 6> RadioState::randomMac() {
    std::random_device rd;
    std::array<std::uint8_t, 6> mac{};
    for (auto& b : mac) b = static_cast<std::uint8_t>(rd());
    mac[0] = static_cast<std::uint8_t>((mac[0] | 0x02U) & 0xFEU);
    return mac;
}

} // namespace hpsdr
