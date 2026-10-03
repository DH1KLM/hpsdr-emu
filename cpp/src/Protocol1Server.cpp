#include "hpsdr/Protocol1Server.h"
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
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
using SocketType=int; static constexpr SocketType InvalidSocket=-1;
#endif
namespace hpsdr {
namespace {
constexpr std::uint16_t Port=1024; constexpr std::size_t PacketSize=1032;
constexpr std::array<std::uint8_t,3> Sync{0x7f,0x7f,0x7f};
constexpr std::array<std::uint8_t,4> ResponseAddrs{0x00,0x08,0x10,0x18};
void closeSocket(SocketType s){
#ifdef _WIN32
 closesocket(s);
#else
 close(s);
#endif
}
}
Protocol1Server::Protocol1Server(RadioState& s,SignalGenerator& g,EchoBuffer* e):state_(s),siggen_(g),echo_(e){}
Protocol1Server::~Protocol1Server(){stop();}
bool Protocol1Server::start(){
#ifdef _WIN32
 WSADATA wsa{}; if(WSAStartup(MAKEWORD(2,2),&wsa)!=0)return false;
#endif
 socket_=int(::socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP)); if(socket_<0)return false;
 int reuse=1; setsockopt(socket_,SOL_SOCKET,SO_REUSEADDR,
#ifdef _WIN32
 reinterpret_cast<const char*>(&reuse),sizeof(reuse)
#else
 &reuse,sizeof(reuse)
#endif
 );
 sockaddr_in local{}; local.sin_family=AF_INET; local.sin_addr.s_addr=htonl(INADDR_ANY); local.sin_port=htons(Port);
 if(bind(socket_,reinterpret_cast<sockaddr*>(&local),sizeof(local))<0){closeSocket(SocketType(socket_));socket_=-1;
#ifdef _WIN32
 WSACleanup();
#endif
 return false;}
 running_=true; receiveThread_=std::thread(&Protocol1Server::receiveLoop,this); streamThread_=std::thread(&Protocol1Server::streamLoop,this); return true;
}
void Protocol1Server::stop(){
 if(!running_.exchange(false))return; state_.running=false;
 if(socket_>=0){
#ifdef _WIN32
 shutdown(SOCKET(socket_),SD_BOTH);
#else
 shutdown(socket_,SHUT_RDWR);
#endif
 closeSocket(SocketType(socket_)); socket_=-1;
 }
 if(receiveThread_.joinable())receiveThread_.join(); if(streamThread_.joinable())streamThread_.join();
#ifdef _WIN32
 WSACleanup();
#endif
}
void Protocol1Server::receiveLoop(){
 std::array<std::uint8_t,2048> b{};
 while(running_){sockaddr_in peer{};
#ifdef _WIN32
 int len=sizeof(peer);
#else
 socklen_t len=sizeof(peer);
#endif
 auto n=recvfrom(socket_,reinterpret_cast<char*>(b.data()),int(b.size()),0,reinterpret_cast<sockaddr*>(&peer),&len);
 if(n<=0)continue; std::array<std::uint8_t,4> ip{};std::memcpy(ip.data(),&peer.sin_addr.s_addr,4);
 handleDatagram(b.data(),std::size_t(n),ip,ntohs(peer.sin_port));}
}
void Protocol1Server::handleDatagram(const std::uint8_t*d,std::size_t n,const std::array<std::uint8_t,4>&ip,std::uint16_t p){
 if(n<4||d[0]!=0xef||d[1]!=0xfe)return; switch(d[2]){
 case 0x02:handleDiscovery(ip,p);break; case 0x04:if(d[3]==1)handleStart(ip,p);else if(d[3]==0)handleStop();break;
 case 0x01:handleHostData(d,n);clientIp_=ip;clientPort_=p;haveClient_=true;break; default:break;}
}
void Protocol1Server::handleDiscovery(const std::array<std::uint8_t,4>&ip,std::uint16_t p){
 sockaddr_in dst{};dst.sin_family=AF_INET;std::memcpy(&dst.sin_addr.s_addr,ip.data(),4);dst.sin_port=htons(p);
 std::array<std::uint8_t,60> r{};r[0]=0xef;r[1]=0xfe;r[2]=2;std::copy(state_.mac.begin(),state_.mac.end(),r.begin()+3);
 r[9]=state_.firmwareVersion;r[10]=hwInfo(state_.hw).code;r[11]=0;r[14]=state_.mercuryVersions[0];r[15]=state_.mercuryVersions[1];r[16]=state_.mercuryVersions[2];r[17]=state_.mercuryVersions[3];r[18]=state_.pennyVersion;r[19]=state_.metisVersion;r[20]=state_.nddc;
 sendto(socket_,reinterpret_cast<const char*>(r.data()),r.size(),0,reinterpret_cast<sockaddr*>(&dst),sizeof(dst));
}
void Protocol1Server::handleStart(const std::array<std::uint8_t,4>&ip,std::uint16_t p){clientIp_=ip;clientPort_=p;haveClient_=true;state_.running=true;}
void Protocol1Server::handleStop(){state_.running=false;}
void Protocol1Server::handleHostData(const std::uint8_t*d,std::size_t n){
 if(n<PacketSize)return; for(std::size_t o:{std::size_t(8),std::size_t(520)}){if(std::memcmp(d+o,Sync.data(),3)!=0)continue;processControl(d[o+3],d[o+4],d[o+5],d[o+6],d[o+7]);}
}
void Protocol1Server::processControl(std::uint8_t c0,std::uint8_t c1,std::uint8_t c2,std::uint8_t c3,std::uint8_t c4){
 const bool mox=(c0&1)!=0;const auto addr=std::uint8_t(c0&0xfeU);state_.ptt=mox;
 if(addr==0){static constexpr std::array<std::uint32_t,4> rates{48000,96000,192000,384000};state_.sampleRate=rates[c1&3];siggen_.setSampleRate(state_.sampleRate);state_.nddc=std::uint8_t(((c4>>3)&7)+1);}
 else if(addr==2){const std::array<std::uint8_t,4> v{c1,c2,c3,c4};state_.txFrequency=readU32BE(v.data());}
 else if(addr>=4&&addr<0x12&&(addr%2)==0){auto ddc=std::size_t((addr-4)/2);if(ddc<state_.rxFrequencies.size()){const std::array<std::uint8_t,4> v{c1,c2,c3,c4};state_.rxFrequencies[ddc]=readU32BE(v.data());}}
 else if(addr==0x12)state_.txDrive=c1;
}
std::vector<std::uint8_t> Protocol1Server::buildDataPacket(){std::vector<std::uint8_t> p(PacketSize);p[0]=0xef;p[1]=0xfe;p[2]=1;p[3]=6;writeU32BE(p.data()+4,state_.nextSeq("p1_data"));fillSubframe(p,8);fillSubframe(p,520);return p;}
void Protocol1Server::fillSubframe(std::vector<std::uint8_t>&p,std::size_t o){
 auto nddc=std::max<std::size_t>(1,state_.nddc);auto spr=504/(6*nddc+2);p[o]=p[o+1]=p[o+2]=0x7f;
 auto a=ResponseAddrs[controlIndex_++%4];p[o+3]=std::uint8_t(a|0x80|(state_.ptt?1:0));
 if(a==0){p[o+4]=0;p[o+5]=state_.firmwareVersion;p[o+6]=state_.pennyVersion;p[o+7]=0;}
 else if(a==8){auto e=state_.ptt?std::uint16_t(state_.txDrive*10):0;auto f=state_.ptt?std::uint16_t((state_.txDrive*state_.txDrive)>>4):0;writeI16BE(p.data()+o+4,std::int16_t(e));writeI16BE(p.data()+o+6,std::int16_t(f));}
 else if(a==0x10){auto f=state_.ptt?std::uint16_t((state_.txDrive*state_.txDrive)>>4):0;auto r=(state_.ptt&&state_.txDrive>0)?std::max<std::uint16_t>(1,f/50):0;writeI16BE(p.data()+o+4,std::int16_t(r));writeI16BE(p.data()+o+6,3200);}
 else {auto pa=state_.ptt?std::uint16_t(state_.txDrive*5):0;writeI16BE(p.data()+o+4,std::int16_t(pa));writeI16BE(p.data()+o+6,3200);}
 std::vector<std::vector<std::complex<float>>> samples;for(std::size_t d=0;d<nddc;++d)samples.push_back(siggen_.generateIq(spr,d));
 auto*dst=p.data()+o+8;for(std::size_t row=0;row<spr;++row){for(std::size_t d=0;d<nddc;++d){auto iq=samples[d][row];auto i=std::int32_t(std::clamp(iq.real(),-1.0F,1.0F)*8388607.0F);auto q=std::int32_t(std::clamp(iq.imag(),-1.0F,1.0F)*8388607.0F);writeI24BE(dst,i);writeI24BE(dst+3,q);dst+=6;}dst[0]=dst[1]=0;dst+=2;}
}
void Protocol1Server::streamLoop(){while(running_){if(!state_.running||!haveClient_){std::this_thread::sleep_for(std::chrono::milliseconds(2));continue;}auto p=buildDataPacket();sockaddr_in dst{};dst.sin_family=AF_INET;std::memcpy(&dst.sin_addr.s_addr,clientIp_.data(),4);dst.sin_port=htons(clientPort_);sendto(socket_,reinterpret_cast<const char*>(p.data()),p.size(),0,reinterpret_cast<sockaddr*>(&dst),sizeof(dst));auto n=std::max<std::size_t>(1,state_.nddc);auto spr=504/(6*n+2);std::this_thread::sleep_for(std::chrono::duration<double>(double(spr*2)/state_.sampleRate));}}
}