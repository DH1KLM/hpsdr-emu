#include "hpsdr/Protocol2Server.h"
#include "hpsdr/RadioState.h"
#include "hpsdr/SignalGenerator.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <set>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using Socket = SOCKET;
constexpr Socket InvalidSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
using Socket = int;
constexpr Socket InvalidSocket = -1;
#endif

namespace {

void closeSocket(Socket s) {
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

void setReceiveTimeout(Socket s, int milliseconds) {
#ifdef _WIN32
    const DWORD timeout = static_cast<DWORD>(milliseconds);
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
    timeval timeout{};
    timeout.tv_sec = milliseconds / 1000;
    timeout.tv_usec = (milliseconds % 1000) * 1000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif
}

void writeU16BE(std::uint8_t* p, std::uint16_t value) {
    p[0] = static_cast<std::uint8_t>(value >> 8U);
    p[1] = static_cast<std::uint8_t>(value);
}

void writeU32BE(std::uint8_t* p, std::uint32_t value) {
    p[0] = static_cast<std::uint8_t>(value >> 24U);
    p[1] = static_cast<std::uint8_t>(value >> 16U);
    p[2] = static_cast<std::uint8_t>(value >> 8U);
    p[3] = static_cast<std::uint8_t>(value);
}

} // namespace

int main() {
#ifdef _WIN32
    WSADATA wsa{};
    assert(WSAStartup(MAKEWORD(2, 2), &wsa) == 0);
#endif

    hpsdr::RadioState state;
    state.hw = hpsdr::HpsdrHw::HermesLite;
    state.mac = {0x00, 0x1c, 0xc0, 0xa2, 0x13, 0xdd};
    state.firmwareVersion = 0x4a;
    state.mercuryVersions = {1, 2, 3, 4};
    state.pennyVersion = 5;
    state.metisVersion = 6;
    state.nddc = 2;

    hpsdr::SignalGenerator siggen(192000, 1000.0, 0.0);
    hpsdr::Protocol2Server server(state, siggen);

    //DH1KLM: This integration test owns the Protocol 2 UDP ports while it runs.
    assert(server.start());

    const Socket client = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    assert(client != InvalidSocket);
    setReceiveTimeout(client, 2000);

    sockaddr_in radio{};
    radio.sin_family = AF_INET;
    radio.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    radio.sin_port = htons(1024);

    //DH1KLM: P2 discovery is a 5-byte general packet with byte 4 set to 0x02.
    const std::array<std::uint8_t, 5> discovery{0, 0, 0, 0, 0x02};
    auto sent = sendto(
        client,
        reinterpret_cast<const char*>(discovery.data()),
        static_cast<int>(discovery.size()),
        0,
        reinterpret_cast<const sockaddr*>(&radio),
        sizeof(radio));
    assert(sent == static_cast<int>(discovery.size()));

    std::array<std::uint8_t, 2048> packet{};
    sockaddr_in peer{};
#ifdef _WIN32
    int peerLen = sizeof(peer);
#else
    socklen_t peerLen = sizeof(peer);
#endif

    auto received = recvfrom(
        client, reinterpret_cast<char*>(packet.data()), static_cast<int>(packet.size()),
        0, reinterpret_cast<sockaddr*>(&peer), &peerLen);
    assert(received == 60);
    assert(packet[4] == 0x02);
    assert(std::memcmp(packet.data() + 5, state.mac.data(), 6) == 0);
    assert(packet[11] == 0x06);
    assert(packet[12] == 0x01);
    assert(packet[13] == state.firmwareVersion);
    assert(packet[14] == 1);
    assert(packet[15] == 2);
    assert(packet[16] == 3);
    assert(packet[17] == 4);
    assert(packet[18] == 5);
    assert(packet[19] == 6);
    assert(packet[20] == 2);

    //DH1KLM: Register the host as the general Protocol 2 client.
    const std::array<std::uint8_t, 5> general{0, 0, 0, 0, 0x00};
    sent = sendto(
        client, reinterpret_cast<const char*>(general.data()), static_cast<int>(general.size()),
        0, reinterpret_cast<const sockaddr*>(&radio), sizeof(radio));
    assert(sent == static_cast<int>(general.size()));

    //DH1KLM: RX-specific config selects RX0/RX1 and sets 192 kS/s for RX0.
    std::array<std::uint8_t, 20> rxSpecific{};
    rxSpecific[7] = 0x03;
    writeU16BE(rxSpecific.data() + 18, 192);
    sockaddr_in rxPort = radio;
    rxPort.sin_port = htons(1025);
    sent = sendto(
        client, reinterpret_cast<const char*>(rxSpecific.data()), static_cast<int>(rxSpecific.size()),
        0, reinterpret_cast<const sockaddr*>(&rxPort), sizeof(rxPort));
    assert(sent == static_cast<int>(rxSpecific.size()));
    assert(state.sampleRate == 192000);

    //DH1KLM: High-priority host control carries RUN/PTT, RX frequencies, TX frequency and drive.
    std::array<std::uint8_t, 346> hp{};
    hp[4] = 0x03; // RUN + PTT
    writeU32BE(hp.data() + 9, 14'350'000U);
    writeU32BE(hp.data() + 13, 7'074'000U);
    writeU32BE(hp.data() + 329, 7'074'000U);
    hp[345] = 0x28;
    sockaddr_in hpPort = radio;
    hpPort.sin_port = htons(1027);
    sent = sendto(
        client, reinterpret_cast<const char*>(hp.data()), static_cast<int>(hp.size()),
        0, reinterpret_cast<const sockaddr*>(&hpPort), sizeof(hpPort));
    assert(sent == static_cast<int>(hp.size()));

    assert(state.running);
    assert(state.ptt);
    assert(state.rxFrequencies[0] == 14'350'000U);
    assert(state.rxFrequencies[1] == 7'074'000U);
    assert(state.txFrequency == 7'074'000U);
    assert(state.txDrive == 0x28);

    //DH1KLM: Collect the three P2 radio-to-host streams from their protocol-defined source ports.
    std::set<std::uint16_t> seenPorts;
    bool ddcChecked = false;
    bool hpChecked = false;
    bool micChecked = false;

    for (int attempt = 0; attempt < 20 && (!ddcChecked || !hpChecked || !micChecked); ++attempt) {
        peerLen = sizeof(peer);
        received = recvfrom(
            client, reinterpret_cast<char*>(packet.data()), static_cast<int>(packet.size()),
            0, reinterpret_cast<sockaddr*>(&peer), &peerLen);
        assert(received > 0);
        const auto sourcePort = ntohs(peer.sin_port);
        seenPorts.insert(sourcePort);

        if (sourcePort == 1035) {
            assert(received == 1444);
            assert(packet[12] == 0x00);
            assert(packet[13] == 0x18); // 24 bits/sample
            assert(packet[14] == 0x00);
            assert(packet[15] == 238);
            ddcChecked = true;
        } else if (sourcePort == 1025) {
            assert(received == 60);
            assert(packet[4] == 0x01); // PTT
            const std::uint16_t exc = static_cast<std::uint16_t>(packet[6] << 8U | packet[7]);
            const std::uint16_t fwd = static_cast<std::uint16_t>(packet[14] << 8U | packet[15]);
            const std::uint16_t rev = static_cast<std::uint16_t>(packet[22] << 8U | packet[23]);
            assert(exc == 0x28U * 10U);
            assert(fwd == (0x28U * 0x28U) / 16U);
            assert(rev == std::max<std::uint16_t>(1, fwd / 50));
            hpChecked = true;
        } else if (sourcePort == 1026) {
            assert(received == 132);
            assert(packet[0] == 0x00);
            assert(packet[1] == 0x00);
            assert(packet[2] == 0x00);
            assert(packet[3] == 0x00);
            micChecked = true;
        }
    }

    assert(ddcChecked);
    assert(hpChecked);
    assert(micChecked);

    //DH1KLM: Stop RUN via the high-priority control packet.
    hp[4] = 0x00;
    sent = sendto(
        client, reinterpret_cast<const char*>(hp.data()), static_cast<int>(hp.size()),
        0, reinterpret_cast<const sockaddr*>(&hpPort), sizeof(hpPort));
    assert(sent == static_cast<int>(hp.size()));
    assert(!state.running);

    closeSocket(client);
    server.stop();

#ifdef _WIN32
    WSACleanup();
#endif

    std::cout << "Protocol 2 discovery/control/stream integration test passed\n";
    return 0;
}
