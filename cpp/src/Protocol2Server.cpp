#include "hpsdr/Protocol2Server.h"
#include "hpsdr/PacketCodec.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <thread>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using SocketType=SOCKET; static constexpr SocketType InvalidSocket=INVALID_SOCKET;
#else
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
using SocketType=int; static constexpr SocketType InvalidSocket=-1;
#endif
namespace hpsdr {
namespace {
constexpr std::array<int,6> Ports{1024,1025,1026,1027,1028,1029};
void closeSocket(SocketType s){
#ifdef _WIN32
 closesocket(s);
#else
 close(s);
#endif
}
std::uint64_t nowMicros(){return std::uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count());}
}
Protocol2Server::Protocol2Server(RadioState& s,SignalGenerator& g):state_(s),siggen_(g){sockets_.fill(-1);}
Protocol2Server::~Protocol2Server(){stop();}
bool Protocol2Server::openSockets(){
#ifdef _WIN32
 WSADATA wsa{}; if(WSAStartup(MAKEWORD(2,2),&wsa)!=0)return false;
#endif
 for(std::size_t i=0;i<Ports.size();++i){
  auto s=::socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP); if(s==InvalidSocket){closeSockets();return false;}
  int reuse=1;
#ifdef _WIN32
  setsockopt(s,SOL_SOCKET,SO_REUSEADDR,reinterpret_cast<const char*>(&reuse),sizeof(reuse));
#else
  setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
#endif
  sockaddr_in a{};a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_ANY);a.sin_port=htons(std::uint16_t(Ports[i]));
  if(bind(s,reinterpret_cast<sockaddr*>(&a),sizeof(a))<0){closeSocket(s);closeSockets();return false;} sockets_[i]=int(s);
 }
 for(std::size_t d=0;d<std::max<std::size_t>(1,state_.nddc);++d){
  auto s=::socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);if(s==InvalidSocket){closeSockets();return false;}
  int reuse=1;
#ifdef _WIN32
  setsockopt(s,SOL_SOCKET,SO_REUSEADDR,reinterpret_cast<const char*>(&reuse),sizeof(reuse));
#else
  setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
#endif
  sockaddr_in a{};a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_ANY);a.sin_port=htons(std::uint16_t(PortDdcBase+d));
  if(bind(s,reinterpret_cast<sockaddr*>(&a),sizeof(a))<0){closeSocket(s);closeSockets();return false;}ddcSockets_.push_back(int(s));
 }
 return true;
}
void Protocol2Server::closeSockets(){
 for(auto&s:sockets_)if(s>=0){closeSocket(SocketType(s));s=-1;}
 for(auto s:ddcSockets_)if(s>=0)closeSocket(SocketType(s));ddcSockets_.clear();
#ifdef _WIN32
 WSACleanup();
#endif
}
bool Protocol2Server::start(){if(!openSockets())return false;running_=true;receiveThread_=std::thread(&Protocol2Server::receiveLoop,this);streamThread_=std::thread(&Protocol2Server::streamLoop,this);return true;}
void Protocol2Server::stop(){if(!running_.exchange(false))return;state_.running=false;closeSockets();if(receiveThread_.joinable())receiveThread_.join();if(streamThread_.joinable())streamThread_.join();}
void Protocol2Server::receiveLoop(){
 std::array<std::uint8_t,4096>b{};
 while(running_){
  fd_set set;FD_ZERO(&set);int maxfd=-1;for(auto s:sockets_)if(s>=0){FD_SET(SocketType(s),&set);
#ifndef _WIN32
   maxfd=std::max(maxfd,s);
#endif
  }
  timeval tv{0,100000};
#ifdef _WIN32
  if(select(0,&set,nullptr,nullptr,&tv)<=0)continue;
#else
  if(select(maxfd+1,&set,nullptr,nullptr,&tv)<=0)continue;
#endif
  for(std::size_t i=0;i<sockets_.size();++i){auto s=sockets_[i];if(s<0||!FD_ISSET(SocketType(s),&set))continue;sockaddr_in p{};
#ifdef _WIN32
   int l=sizeof(p);
#else
   socklen_t l=sizeof(p);
#endif
   auto n=recvfrom(SocketType(s),reinterpret_cast<char*>(b.data()),int(b.size()),0,reinterpret_cast<sockaddr*>(&p),&l);
   if(n>0)handlePacket(Ports[i],b.data(),std::size_t(n),p.sin_addr.s_addr,ntohs(p.sin_port));
  }
 }
}
void Protocol2Server::handlePacket(int port,const std::uint8_t*d,std::size_t n,std::uint32_t a,std::uint16_t p){
 switch(port){case PortGeneral:handleGeneral(d,n,a,p);break;case PortRxSpecific:handleRxSpecific(d,n,a,p);break;case PortTxSpecific:handleTxSpecific(d,n,a,p);break;case PortHighPriority:handleHighPriority(d,n,a,p);break;case PortTxAudio:handleTxAudio(d,n,a,p);break;case PortTxIq:handleTxIq(d,n,a,p);break;default:break;}
}
void Protocol2Server::handleGeneral(const std::uint8_t*d,std::size_t n,std::uint32_t a,std::uint16_t p){
 if(n<5)return;client_={a,p,true};if(d[4]!=0x02)return;auto r=buildDiscoveryResponse();sockaddr_in dst{};dst.sin_family=AF_INET;dst.sin_addr.s_addr=a;dst.sin_port=htons(p);sendto(SocketType(sockets_[0]),reinterpret_cast<const char*>(r.data()),int(r.size()),0,reinterpret_cast<sockaddr*>(&dst),sizeof(dst));
}
void Protocol2Server::handleRxSpecific(const std::uint8_t*d,std::size_t n,std::uint32_t a,std::uint16_t p){
 if(n<5)return;client_={a,p,true};if(n>19){auto khz=readU16BE(d+18);if(khz){state_.sampleRate=std::uint32_t(khz)*1000U;siggen_.setSampleRate(state_.sampleRate);}}
}
void Protocol2Server::handleTxSpecific(const std::uint8_t*,std::size_t,std::uint32_t a,std::uint16_t p){client_={a,p,true};}
void Protocol2Server::handleHighPriority(const std::uint8_t*d,std::size_t n,std::uint32_t a,std::uint16_t p){
 if(n<57)return;client_={a,p,true};state_.ptt=(d[4]&2)!=0;
 for(std::size_t i=0;i<12;++i){auto o=9+i*4;if(o+4>n)break;auto f=readU32BE(d+o);if(f)state_.rxFrequencies[i]=f;}
 if(n>332){auto f=readU32BE(d+329);if(f)state_.txFrequency=f;}if(n>345)state_.txDrive=d[345];
 const bool run=(d[4]&1)!=0;if(run!=state_.running)state_.running=run;
}
void Protocol2Server::handleTxAudio(const std::uint8_t*,std::size_t,std::uint32_t a,std::uint16_t p){client_={a,p,true};}
void Protocol2Server::handleTxIq(const std::uint8_t*,std::size_t,std::uint32_t a,std::uint16_t p){client_={a,p,true};}
std::vector<std::uint8_t> Protocol2Server::buildDiscoveryResponse()const{
 std::vector<std::uint8_t>b(60);b[4]=2;std::copy(state_.mac.begin(),state_.mac.end(),b.begin()+5);b[11]=hwInfo(state_.hw).code;b[12]=1;b[13]=state_.firmwareVersion;b[14]=state_.mercuryVersions[0];b[15]=state_.mercuryVersions[1];b[16]=state_.mercuryVersions[2];b[17]=state_.mercuryVersions[3];b[18]=state_.pennyVersion;b[19]=state_.metisVersion;b[20]=state_.nddc;return b;
}
std::vector<std::uint8_t> Protocol2Server::buildHpStatus(){std::vector<std::uint8_t>b(60);writeU32BE(b.data(),state_.nextSeq("hp_status"));b[4]=state_.ptt?1:0;if(state_.ptt&&state_.txDrive){auto exc=std::uint16_t(state_.txDrive*10U),fwd=std::uint16_t((state_.txDrive*state_.txDrive)>>4),rev=std::max<std::uint16_t>(1,std::uint16_t(fwd/50));writeU16BE(b.data()+6,exc);writeU16BE(b.data()+14,fwd);writeU16BE(b.data()+22,rev);}return b;}
std::vector<std::uint8_t> Protocol2Server::buildDdcIqPacket(std::size_t ddc){std::vector<std::uint8_t>b(16);writeU32BE(b.data(),state_.nextSeq("ddc_"+std::to_string(ddc)));auto t=nowMicros();for(unsigned i=0;i<8;++i)b[4+i]=std::uint8_t(t>>(56-8*i));writeU16BE(b.data()+12,24);writeU16BE(b.data()+14,std::uint16_t(SamplesPerDdcPacket));auto iq=siggen_.generateIq(SamplesPerDdcPacket,ddc);auto packed=packIq24(iq);b.insert(b.end(),packed.begin(),packed.end());return b;}
std::vector<std::uint8_t> Protocol2Server::buildMicPacket(){std::vector<std::uint8_t>b(4+SamplesPerMicPacket*2);writeU32BE(b.data(),state_.nextSeq("mic"));return b;}
bool Protocol2Server::sendFromPort(int port,const std::vector<std::uint8_t>&b){
 if(!client_.valid)return false;SocketType s=InvalidSocket;if(port>=PortDdcBase){auto i=std::size_t(port-PortDdcBase);if(i>=ddcSockets_.size())return false;s=SocketType(ddcSockets_[i]);}else if(port==PortHighPriority)s=SocketType(sockets_[1]);else if(port==PortMic)s=SocketType(sockets_[2]);if(s==InvalidSocket)return false;sockaddr_in dst{};dst.sin_family=AF_INET;dst.sin_addr.s_addr=client_.address;dst.sin_port=htons(client_.port);return sendto(s,reinterpret_cast<const char*>(b.data()),int(b.size()),0,reinterpret_cast<sockaddr*>(&dst),sizeof(dst))>=0;
}
void Protocol2Server::streamLoop(){
 auto hp=std::chrono::steady_clock::now();while(running_){if(!state_.running||!client_.valid){std::this_thread::sleep_for(std::chrono::milliseconds(2));hp=std::chrono::steady_clock::now();continue;}auto now=std::chrono::steady_clock::now();if(now>=hp){sendFromPort(PortHighPriority,buildHpStatus());hp+=std::chrono::milliseconds(100);}auto count=std::min<std::size_t>(state_.nddc,ddcSockets_.size());for(std::size_t d=0;d<count;++d)sendFromPort(PortDdcBase+int(d),buildDdcIqPacket(d));sendFromPort(PortMic,buildMicPacket());std::this_thread::sleep_for(std::chrono::duration<double>(double(SamplesPerDdcPacket)/state_.sampleRate));}
}
}