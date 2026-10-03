#include "hpsdr/PacketCodec.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace hpsdr {

std::int32_t readI24BE(const std::uint8_t* p) {
    std::int32_t v = (static_cast<std::int32_t>(p[0]) << 16) |
                     (static_cast<std::int32_t>(p[1]) << 8) |
                     static_cast<std::int32_t>(p[2]);
    if (v & 0x800000) v |= static_cast<std::int32_t>(0xFF000000U);
    return v;
}

void writeI24BE(std::uint8_t* p, std::int32_t value) {
    const auto v = static_cast<std::uint32_t>(value) & 0x00FFFFFFU;
    p[0] = static_cast<std::uint8_t>(v >> 16);
    p[1] = static_cast<std::uint8_t>(v >> 8);
    p[2] = static_cast<std::uint8_t>(v);
}

std::int16_t readI16BE(const std::uint8_t* p) {
    const auto v = static_cast<std::uint16_t>(p[0] << 8 | p[1]);
    return static_cast<std::int16_t>(v);
}

void writeI16BE(std::uint8_t* p, std::int16_t value) {
    const auto v = static_cast<std::uint16_t>(value);
    p[0] = static_cast<std::uint8_t>(v >> 8);
    p[1] = static_cast<std::uint8_t>(v);
}

std::uint32_t readU32BE(const std::uint8_t* p) {
    return (static_cast<std::uint32_t>(p[0]) << 24) |
           (static_cast<std::uint32_t>(p[1]) << 16) |
           (static_cast<std::uint32_t>(p[2]) << 8) |
           static_cast<std::uint32_t>(p[3]);
}

void writeU32BE(std::uint8_t* p, std::uint32_t value) {
    p[0] = static_cast<std::uint8_t>(value >> 24);
    p[1] = static_cast<std::uint8_t>(value >> 16);
    p[2] = static_cast<std::uint8_t>(value >> 8);
    p[3] = static_cast<std::uint8_t>(value);
}

std::vector<std::uint8_t> packIq24(std::span<const std::complex<float>> iq) {
    std::vector<std::uint8_t> out(iq.size() * 6);
    constexpr float maxValue = 8388607.0F;
    for (std::size_t i = 0; i < iq.size(); ++i) {
        const auto clamp = [](float x) { return std::clamp(x, -1.0F, 1.0F); };
        writeI24BE(out.data() + i * 6,
                   static_cast<std::int32_t>(clamp(iq[i].real()) * maxValue));
        writeI24BE(out.data() + i * 6 + 3,
                   static_cast<std::int32_t>(clamp(iq[i].imag()) * maxValue));
    }
    return out;
}

std::vector<std::complex<float>> unpackTxIq16(std::span<const std::uint8_t> data) {
    const auto count = data.size() / 8;
    std::vector<std::complex<float>> out(count);
    for (std::size_t k = 0; k < count; ++k) {
        const auto off = k * 8;
        out[k] = {readI16BE(data.data() + off + 4) / 32768.0F,
                  readI16BE(data.data() + off + 6) / 32768.0F};
    }
    return out;
}

std::vector<std::complex<float>> unpackTxAudio16(std::span<const std::uint8_t> data) {
    const auto count = data.size() / 4;
    std::vector<std::complex<float>> out(count);
    for (std::size_t k = 0; k < count; ++k) {
        const auto off = k * 4;
        out[k] = {readI16BE(data.data() + off) / 32768.0F,
                  readI16BE(data.data() + off + 2) / 32768.0F};
    }
    return out;
}

std::vector<std::complex<float>> unpackTxIq24(std::span<const std::uint8_t> data) {
    const auto count = data.size() / 6;
    std::vector<std::complex<float>> out(count);
    for (std::size_t k = 0; k < count; ++k) {
        const auto off = k * 6;
        out[k] = {readI24BE(data.data() + off) / 8388607.0F,
                  readI24BE(data.data() + off + 3) / 8388607.0F};
    }
    return out;
}

} // namespace hpsdr
