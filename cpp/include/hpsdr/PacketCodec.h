#pragma once

#include <complex>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace hpsdr {

std::int32_t readI24BE(const std::uint8_t* p);
void writeI24BE(std::uint8_t* p, std::int32_t value);
std::int16_t readI16BE(const std::uint8_t* p);
std::uint16_t readU16BE(const std::uint8_t* p);
void writeI16BE(std::uint8_t* p, std::int16_t value);
void writeU16BE(std::uint8_t* p, std::uint16_t value);
std::uint32_t readU32BE(const std::uint8_t* p);
void writeU32BE(std::uint8_t* p, std::uint32_t value);

std::vector<std::uint8_t> packIq24(std::span<const std::complex<float>> iq);
std::vector<std::complex<float>> unpackTxIq16(std::span<const std::uint8_t> data);
std::vector<std::complex<float>> unpackTxAudio16(std::span<const std::uint8_t> data);
std::vector<std::complex<float>> unpackTxIq24(std::span<const std::uint8_t> data);

} // namespace hpsdr
