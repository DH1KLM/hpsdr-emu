#pragma once
#include "RadioState.h"
#include "SignalGenerator.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <chrono>
#include <thread>
#include <mutex>
#include <vector>
namespace hpsdr {
//DH1KLM: Protocol 2 uses dedicated UDP ports for control and data streams.
class EchoBuffer;
class Protocol2Server {
public:
 Protocol2Server(RadioState&,SignalGenerator&,EchoBuffer* echo=nullptr); ~Protocol2Server();
 Protocol2Server(const Protocol2Server&)=delete; Protocol2Server& operator=(const Protocol2Server&)=delete;
 bool start(); void stop();
private:
 static constexpr int PortGeneral=1024,PortRxSpecific=1025,PortTxSpecific=1026,PortHighPriority=1027,PortTxAudio=1028,PortTxIq=1029,PortMic=1026,PortDdcBase=1035;
 static constexpr std::size_t SamplesPerDdcPacket=238,SamplesPerMicPacket=64;
 struct Peer{std::uint32_t address=0;std::uint16_t port=0;bool valid=false;};
 void receiveLoop(); void streamLoop();
 void handlePacket(int,const std::uint8_t*,std::size_t,std::uint32_t,std::uint16_t);
 void handleGeneral(const std::uint8_t*,std::size_t,std::uint32_t,std::uint16_t);
 void handleRxSpecific(const std::uint8_t*,std::size_t,std::uint32_t,std::uint16_t);
 void handleTxSpecific(const std::uint8_t*,std::size_t,std::uint32_t,std::uint16_t);
 void handleHighPriority(const std::uint8_t*,std::size_t,std::uint32_t,std::uint16_t);
 void handleTxAudio(const std::uint8_t*,std::size_t,std::uint32_t,std::uint16_t);
 void handleTxIq(const std::uint8_t*,std::size_t,std::uint32_t,std::uint16_t);
 std::vector<std::uint8_t> buildDiscoveryResponse() const;
 std::vector<std::uint8_t> buildHpStatus();
 std::vector<std::uint8_t> buildDdcIqPacket(std::size_t);
 std::vector<std::uint8_t> buildMicPacket();
 bool openSockets(); void closeSockets(); bool sendFromPort(int,const std::vector<std::uint8_t>&);
 RadioState& state_; SignalGenerator& siggen_; EchoBuffer* echo_; std::array<int,6> sockets_{}; std::vector<int> ddcSockets_;
 std::atomic_bool running_{false}; std::thread receiveThread_,streamThread_; Peer client_{}; bool echoTxActive_=false; std::mutex mutex_; std::chrono::steady_clock::time_point lastEchoTxData_{};
};
}
