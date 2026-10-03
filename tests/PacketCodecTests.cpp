#include "hpsdr/PacketCodec.h"
#include <cassert>
#include <cstdint>
#include <vector>

int main() {
    using namespace hpsdr;

    {
        std::uint8_t b[3]{};
        writeI24BE(b, 0x007fffff);
        assert(readI24BE(b) == 0x007fffff);
        writeI24BE(b, -1);
        assert(readI24BE(b) == -1);
        writeI24BE(b, -8388608);
        assert(readI24BE(b) == -8388608);
    }

    {
        std::uint8_t b[4]{};
        writeU32BE(b, 0x12345678U);
        assert(readU32BE(b) == 0x12345678U);
    }

    {
        std::vector<std::complex<float>> iq{{1.0F, -1.0F}, {0.5F, -0.5F}};
        const auto packed = packIq24(iq);
        assert(packed.size() == 12);
        assert(readI24BE(packed.data()) == 8388607);
        assert(readI24BE(packed.data() + 3) == -8388607);
        assert(readI24BE(packed.data() + 6) == 4194303);
        assert(readI24BE(packed.data() + 9) == -4194303);
    }

    {
        const std::uint8_t p1[] = {0,0,0,0, 0x7f,0xff, 0x80,0x00};
        const auto iq = unpackTxIq16(p1);
        assert(iq.size() == 1);
        assert(iq[0].real() > 0.99F);
        assert(iq[0].imag() < -0.99F);
    }

    {
        const std::uint8_t p2[] = {0x7f,0xff,0x80,0x00};
        const auto iq = unpackTxAudio16(p2);
        assert(iq.size() == 1);
        assert(iq[0].real() > 0.99F);
        assert(iq[0].imag() < -0.99F);
    }

    {
        //DH1KLM: One Protocol 2 IQ sample is I(24-bit) followed by Q(24-bit).
        const std::uint8_t p2[] = {0x7f,0xff,0xff,0x80,0x00,0x00};
        const auto iq = unpackTxIq24(p2);
        assert(iq.size() == 1);
        assert(iq[0].real() > 0.99F);
        assert(iq[0].imag() < -0.99F);
    }

    return 0;
}
