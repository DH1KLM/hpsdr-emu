#pragma once
#include "RadioState.h"
#include "SignalGenerator.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>
namespace hpsdr {
class EchoBuffer;
class Protocol1Server {
public:
    Protocol1Server(RadioState&,SignalGenerator&,EchoBuffer* echo=nullptr);
    ~Protocol1Server();
    Protocol1Server(const Protocol1Server&)=delete;
    Protocol1Server& operator=(const Protocol1Server&)=delete;
    bool start(); void stop();
private:
    void receiveLoop(); void streamLoop();
    void handleDatagram(const std::uint8_t*,std::size_t,const std::array<std::uint8_t,4>&,std::uint16_t);
    void handleDiscovery(const std::array<std::uint8_t,4>&,std::uint16_t);
    void handleStart(const std::array<std::uint8_t,4>&,std::uint16_t);
    void handleStop(); void handleHostData(const std::uint8_t*,std::size_t);
    void processControl(std::uint8_t,std::uint8_t,std::uint8_t,std::uint8_t,std::uint8_t);
    std::vector<std::uint8_t> buildDataPacket();
    void fillSubframe(std::vector<std::uint8_t>&,std::size_t);
    RadioState& state_; SignalGenerator& siggen_; EchoBuffer* echo_;
    int socket_=-1; std::atomic_bool running_{false}; std::thread receiveThread_,streamThread_;
    std::array<std::uint8_t,4> clientIp_{}; std::uint16_t clientPort_=0; bool haveClient_=false;
    unsigned controlIndex_=0;
};
}