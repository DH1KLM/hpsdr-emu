#include "hpsdr/EchoBuffer.h"
#include "hpsdr/Protocol1Server.h"
#include "hpsdr/Protocol2Server.h"
#include "hpsdr/RadioState.h"
#include "hpsdr/SignalGenerator.h"
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char** argv) {
    int protocol = 1;
    std::string radio = "hermeslite";
    double freq = 1000.0;
    double noise = 3e-6;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if ((a == "--protocol") && i + 1 < argc) protocol = std::stoi(argv[++i]);
        else if (a == "--radio" && i + 1 < argc) radio = argv[++i];
        else if (a == "--freq" && i + 1 < argc) freq = std::stod(argv[++i]);
        else if (a == "--noise" && i + 1 < argc) noise = std::stod(argv[++i]);
        else if (a == "-v" || a == "--verbose") {}
        else if (a == "-h" || a == "--help") {
            std::cout << "hpsdr-emu --protocol {1,2} --radio TYPE --freq HZ --noise LEVEL\n";
            return 0;
        } else {
            std::cerr << "Unknown argument: " << a << "\n";
            return 2;
        }
    }

    if (protocol != 1 && protocol != 2) {
        std::cerr << "Unsupported protocol: " << protocol << "\n";
        return 2;
    }

    try {
        hpsdr::RadioState state;
        state.hw = hpsdr::parseRadioName(radio);
        state.mac = hpsdr::RadioState::randomMac();
        state.nddc = 1;

        hpsdr::SignalGenerator siggen(48000, freq, noise);
        hpsdr::EchoBuffer echo(state.sampleRate);

        if (protocol == 1) {
            hpsdr::Protocol1Server server(state, siggen, &echo);
            if (!server.start()) {
                std::cerr << "Failed to bind Protocol 1 UDP port 1024\n";
                return 1;
            }
            const auto info = hpsdr::hwInfo(state.hw);
            std::cout << "Protocol 1 server started\n"
                      << "radio: " << hpsdr::radioName(state.hw)
                      << " code=" << int(info.code)
                      << " maxDDCs=" << int(info.maxDdcs) << "\n";
            for (;;) std::this_thread::sleep_for(std::chrono::hours(24));
        } else {
            hpsdr::Protocol2Server server(state, siggen, &echo);
            if (!server.start()) {
                std::cerr << "Failed to bind Protocol 2 UDP ports\n";
                return 1;
            }
            const auto info = hpsdr::hwInfo(state.hw);
            std::cout << "Protocol 2 server started\n"
                      << "radio: " << hpsdr::radioName(state.hw)
                      << " code=" << int(info.code)
                      << " maxDDCs=" << int(info.maxDdcs) << "\n";
            for (;;) std::this_thread::sleep_for(std::chrono::hours(24));
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
