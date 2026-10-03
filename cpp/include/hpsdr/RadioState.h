#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace hpsdr {

enum class HpsdrHw : std::uint8_t {
    Atlas = 0,
    Hermes = 1,
    HermesII = 2,
    Angelia = 3,
    Orion = 4,
    OrionMkII = 5,
    HermesLite = 6,
    Saturn = 10,
    SaturnMkII = 11
};

struct HwInfo {
    HpsdrHw hw;
    std::uint8_t code;
    std::uint8_t maxDdcs;
};

HwInfo hwInfo(HpsdrHw hw);
HpsdrHw parseRadioName(const std::string& name);
std::string radioName(HpsdrHw hw);

struct RadioState {
    HpsdrHw hw = HpsdrHw::HermesLite;
    std::array<std::uint8_t, 6> mac{};
    std::uint8_t firmwareVersion = 25;
    std::uint8_t protocolVersion = 0;
    std::array<std::uint8_t, 4> mercuryVersions{25, 25, 25, 25};
    std::uint8_t pennyVersion = 25;
    std::uint8_t metisVersion = 25;

    std::uint32_t sampleRate = 48000;
    std::uint8_t nddc = 1;
    std::array<std::uint32_t, 12> rxFrequencies{};
    std::uint32_t txFrequency = 7074000;
    std::uint8_t txDrive = 0;
    bool running = false;
    bool ptt = false;
    std::unordered_map<std::string, std::uint32_t> sequence;

    std::uint32_t nextSeq(const std::string& stream);
    static std::array<std::uint8_t, 6> randomMac();
};

} // namespace hpsdr
