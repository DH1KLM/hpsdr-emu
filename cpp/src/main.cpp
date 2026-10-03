#include "hpsdr/RadioState.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    hpsdr::HpsdrHw hw = hpsdr::HpsdrHw::HermesLite;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--radio" && i + 1 < argc) {
            hw = hpsdr::parseRadioName(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "hpsdr-emu-cpp --radio TYPE\n";
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            return 2;
        }
    }

    try {
        const auto info = hpsdr::hwInfo(hw);
        hpsdr::RadioState state;
        state.hw = hw;
        state.mac = hpsdr::RadioState::randomMac();
        state.nddc = 1;

        std::cout << "hpsdr-emu-cpp\n"
                  << "radio: " << hpsdr::radioName(hw) << "\n"
                  << "board code: " << static_cast<int>(info.code) << "\n"
                  << "max DDCs: " << static_cast<int>(info.maxDdcs) << "\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
    return 0;
}
