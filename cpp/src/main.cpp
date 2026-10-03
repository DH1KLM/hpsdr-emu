#include "hpsdr/EchoBuffer.h"
#include "hpsdr/Protocol1Server.h"
#include "hpsdr/Protocol2Server.h"
#include "hpsdr/RadioState.h"
#include "hpsdr/SignalGenerator.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <csignal>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

std::atomic_bool g_stop{false};

void signalHandler(int) {
    g_stop.store(true);
}

std::array<std::uint8_t, 6> parseMac(const std::string& value) {
    std::string hex;
    hex.reserve(12);

    for (char c : value) {
        if (c == ':' || c == '-') {
            continue;
        }
        if (!std::isxdigit(static_cast<unsigned char>(c))) {
            throw std::invalid_argument("MAC address contains a non-hexadecimal character");
        }
        hex.push_back(c);
    }

    if (hex.size() != 12) {
        throw std::invalid_argument("MAC address must be 6 bytes");
    }

    std::array<std::uint8_t, 6> mac{};
    for (std::size_t i = 0; i < mac.size(); ++i) {
        mac[i] = static_cast<std::uint8_t>(
            std::stoul(hex.substr(i * 2, 2), nullptr, 16));
    }
    return mac;
}

std::string macString(const std::array<std::uint8_t, 6>& mac) {
    std::ostringstream out;
    out << std::hex;
    for (std::size_t i = 0; i < mac.size(); ++i) {
        if (i != 0) {
            out << ':';
        }
        if (mac[i] < 0x10) {
            out << '0';
        }
        out << static_cast<unsigned>(mac[i]);
    }
    return out.str();
}

void printUsage() {
    std::cout
        << "Usage: hpsdr-emu --protocol {1,2} [options]\n"
        << "\n"
        << "OpenHPSDR Protocol 1 & 2 radio emulator\n"
        << "\n"
        << "Options:\n"
        << "  --protocol {1,2}       Protocol version (required)\n"
        << "  --radio TYPE           Radio hardware type (default: hermeslite)\n"
        << "  --mac MAC              MAC address, e.g. 00:1c:c0:a2:22:5e\n"
        << "  --freq HZ              Test tone offset (default: 1000)\n"
        << "  --noise LEVEL          Noise fraction of full-scale (default: 3e-6)\n"
        << "  --echo                 Record TX IQ and loop it back on RX\n"
        << "  -v, --verbose          Enable verbose startup logging\n"
        << "  -h, --help             Show this help\n";
}

} // namespace

int main(int argc, char** argv) {
    int protocol = 0;
    std::string radio = "hermeslite";
    std::string macArgument;
    double freq = 1000.0;
    double noise = 3e-6;
    bool echoEnabled = false;
    bool verbose = false;

    try {
        for (int i = 1; i < argc; ++i) {
            const std::string argument = argv[i];

            auto requireValue = [&](const char* option) -> const char* {
                if (i + 1 >= argc) {
                    throw std::invalid_argument(
                        std::string("missing value for ") + option);
                }
                return argv[++i];
            };

            if (argument == "--protocol") {
                protocol = std::stoi(requireValue("--protocol"));
            } else if (argument == "--radio") {
                radio = requireValue("--radio");
            } else if (argument == "--mac") {
                macArgument = requireValue("--mac");
            } else if (argument == "--freq") {
                freq = std::stod(requireValue("--freq"));
            } else if (argument == "--noise") {
                noise = std::stod(requireValue("--noise"));
            } else if (argument == "--echo") {
                echoEnabled = true;
            } else if (argument == "-v" || argument == "--verbose") {
                verbose = true;
            } else if (argument == "-h" || argument == "--help") {
                printUsage();
                return 0;
            } else {
                throw std::invalid_argument("unknown argument: " + argument);
            }
        }

        if (protocol != 1 && protocol != 2) {
            throw std::invalid_argument(
                "--protocol is required and must be 1 or 2");
        }

        const hpsdr::HpsdrHw hw = hpsdr::parseRadioName(radio);
        const auto info = hpsdr::hwInfo(hw);

        // Protocol 1 defaults to 48 kS/s; Protocol 2 defaults to 192 kS/s.
        const std::uint32_t sampleRate = protocol == 1 ? 48000U : 192000U;

        hpsdr::RadioState state;
        state.hw = hw;
        state.mac = macArgument.empty()
            ? hpsdr::RadioState::randomMac()
            : parseMac(macArgument);
        state.sampleRate = sampleRate;
        state.nddc = info.maxDdcs;

        hpsdr::SignalGenerator siggen(sampleRate, freq, noise);
        hpsdr::EchoBuffer echo(sampleRate);
        hpsdr::EchoBuffer* echoPtr = echoEnabled ? &echo : nullptr;

        std::signal(SIGINT, signalHandler);
#ifdef SIGTERM
        std::signal(SIGTERM, signalHandler);
#endif

        if (verbose) {
            std::cout
                << "Starting HPSDR emulator: protocol=" << protocol
                << ", radio=" << hpsdr::radioName(state.hw)
                << ", tone=" << freq << " Hz"
                << ", noise=" << noise
                << ", echo=" << (echoEnabled ? "on" : "off")
                << ", sampleRate=" << sampleRate
                << ", nDDC=" << static_cast<unsigned>(state.nddc)
                << ", mac=" << macString(state.mac)
                << "\n";
        }

        if (protocol == 1) {
            hpsdr::Protocol1Server server(state, siggen, echoPtr);
            if (!server.start()) {
                std::cerr << "Failed to bind Protocol 1 UDP port 1024\n";
                return 1;
            }

            std::cout << "Protocol 1 server started\n"
                      << "radio: " << hpsdr::radioName(state.hw)
                      << " code=" << static_cast<unsigned>(info.code)
                      << " maxDDCs=" << static_cast<unsigned>(info.maxDdcs)
                      << "\n";

            while (!g_stop.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            server.stop();
        } else {
            hpsdr::Protocol2Server server(state, siggen, echoPtr);
            if (!server.start()) {
                std::cerr << "Failed to bind Protocol 2 UDP ports\n";
                return 1;
            }

            std::cout << "Protocol 2 server started\n"
                      << "radio: " << hpsdr::radioName(state.hw)
                      << " code=" << static_cast<unsigned>(info.code)
                      << " maxDDCs=" << static_cast<unsigned>(info.maxDdcs)
                      << "\n";

            while (!g_stop.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            server.stop();
        }

        if (verbose) {
            std::cout << "Shutting down...\n";
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 2;
    }
}
