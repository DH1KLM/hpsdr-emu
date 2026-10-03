#include "hpsdr/Protocol1Server.h"
#include "hpsdr/RadioState.h"
#include "hpsdr/SignalGenerator.h"

#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

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

    hpsdr::SignalGenerator siggen(48000, 1000.0, 0.0);
    hpsdr::Protocol1Server server(state, siggen);

    //DH1KLM: This integration test owns UDP port 1024 while it is running.
    assert(server.start());

    const Socket client = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    assert(client != InvalidSocket);
    setReceiveTimeout(client, 1000);

    sockaddr_in radio{};
    radio.sin_family = AF_INET;
    radio.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    radio.sin_port = htons(1024);

    //DH1KLM: P1 discovery is EF FE 02 followed by 60 zero bytes.
    std::array<std::uint8_t, 63> discovery{};
    discovery[0] = 0xef;
    discovery[1] = 0xfe;
    discovery[2] = 0x02;

    const auto sent = sendto(
        client,
        reinterpret_cast<const char*>(discovery.data()),
        static_cast<int>(discovery.size()),
        0,
        reinterpret_cast<const sockaddr*>(&radio),
        sizeof(radio));
    assert(sent == static_cast<int>(discovery.size()));

    std::array<std::uint8_t, 128> response{};
    sockaddr_in peer{};
#ifdef _WIN32
    int peerLen = sizeof(peer);
#else
    socklen_t peerLen = sizeof(peer);
#endif
    const auto received = recvfrom(
        client,
        reinterpret_cast<char*>(response.data()),
        static_cast<int>(response.size()),
        0,
        reinterpret_cast<sockaddr*>(&peer),
        &peerLen);

    assert(received == 60);
    assert(response[0] == 0xef);
    assert(response[1] == 0xfe);
    assert(response[2] == 0x02);
    assert(std::memcmp(response.data() + 3, state.mac.data(), 6) == 0);
    assert(response[9] == state.firmwareVersion);
    assert(response[10] == 0x06);
    assert(response[11] == 0x00);
    assert(response[14] == 1);
    assert(response[15] == 2);
    assert(response[16] == 3);
    assert(response[17] == 4);
    assert(response[18] == 5);
    assert(response[19] == 6);
    assert(response[20] == 2);

    //DH1KLM: Verify that the start command establishes the streaming client.
    std::array<std::uint8_t, 64> start{};
    start[0] = 0xef;
    start[1] = 0xfe;
    start[2] = 0x04;
    start[3] = 0x01;

    const auto startSent = sendto(
        client,
        reinterpret_cast<const char*>(start.data()),
        static_cast<int>(start.size()),
        0,
        reinterpret_cast<const sockaddr*>(&radio),
        sizeof(radio));
    assert(startSent == static_cast<int>(start.size()));

    std::array<std::uint8_t, 2048> data{};
    peerLen = sizeof(peer);
    const auto dataReceived = recvfrom(
        client,
        reinterpret_cast<char*>(data.data()),
        static_cast<int>(data.size()),
        0,
        reinterpret_cast<sockaddr*>(&peer),
        &peerLen);

    assert(dataReceived == 1032);
    assert(data[0] == 0xef);
    assert(data[1] == 0xfe);
    assert(data[2] == 0x01);
    assert(data[3] == 0x06);
    assert(data[8] == 0x7f);
    assert(data[9] == 0x7f);
    assert(data[10] == 0x7f);

    std::array<std::uint8_t, 64> stop{};
    stop[0] = 0xef;
    stop[1] = 0xfe;
    stop[2] = 0x04;
    stop[3] = 0x00;
    const auto stopSent = sendto(
        client,
        reinterpret_cast<const char*>(stop.data()),
        static_cast<int>(stop.size()),
        0,
        reinterpret_cast<const sockaddr*>(&radio),
        sizeof(radio));
    assert(stopSent == static_cast<int>(stop.size()));

    closeSocket(client);
    server.stop();

    //DH1KLM: The server thread has stopped before these final state assertions,
    // so the test does not race the UDP receive thread.
    assert(state.sampleRate == 96000);
    assert(state.nddc == 2);
    assert(state.txFrequency == 0x14350000U);
    assert(state.rxFrequencies[0] == 0x00d88000U);
    assert(state.txDrive == 0x28);
    assert(state.ptt);

#ifdef _WIN32
    WSACleanup();
#endif

    std::cout << "Protocol 1 discovery/start/stream integration test passed\n";
    return 0;
}
