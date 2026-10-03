#include "hpsdr/EchoBuffer.h"
#include <algorithm>
#include <cmath>
namespace hpsdr {
EchoBuffer::EchoBuffer(std::uint32_t rate,double maxDuration):sampleRate_(rate),maxDuration_(maxDuration){}
void EchoBuffer::startRecording(std::uint32_t f){if(recordingActive_)commit();recording_.clear();recordingFrequency_=f;recordingActive_=true;}
void EchoBuffer::feed(const std::vector<std::complex<float>>& s){if(recordingActive_&&!s.empty())recording_.insert(recording_.end(),s.begin(),s.end());}
void EchoBuffer::stopRecording(){if(recordingActive_){commit();recordingActive_=false;}}
void EchoBuffer::commit(){if(recording_.empty())return;if(recordingFrequency_==0){recording_.clear();return;}auto maxSamples=std::size_t(double(sampleRate_)*maxDuration_);if(recording_.size()>maxSamples)recording_.resize(maxSamples);if(recording_.empty())return;echoes_[recordingFrequency_]=recording_;playbackPos_[recordingFrequency_]=0;recording_.clear();}
std::vector<std::complex<float>> EchoBuffer::generateEcho(std::size_t n,std::uint32_t rx,std::uint32_t rate){
 std::vector<std::complex<float>> result(n,{0.0F,0.0F}); if(echoes_.empty())return result;const double halfBw=double(rate)/2.0;const double pi=std::acos(-1.0);
 for(auto& [freq,buf]:echoes_){if(buf.empty())continue;const double offset=double(rx)-double(freq);if(std::abs(offset)>halfBw)continue;auto&pos=playbackPos_[freq];std::vector<std::complex<float>> chunk(n);for(std::size_t w=0;w<n;){auto avail=std::min(n-w,buf.size()-pos);std::copy_n(buf.begin()+pos,avail,chunk.begin()+w);pos=(pos+avail)%buf.size();w+=avail;}
  if(offset!=0){double phase=shiftPhase_[freq],step=2.0*pi*offset/double(rate);for(std::size_t i=0;i<n;++i){double a=phase+step*double(i);chunk[i]*=std::complex<float>(float(std::cos(a)),float(std::sin(a)));}phase+=step*double(n);if(std::abs(phase)>1e6)phase=std::fmod(phase,2.0*pi);shiftPhase_[freq]=phase;}
  for(std::size_t i=0;i<n;++i)result[i]+=chunk[i];
 }
 for(auto&v:result)v*=float(Attenuation);return result;
}
}