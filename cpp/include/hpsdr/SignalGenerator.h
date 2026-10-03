#pragma once
#include <complex>
#include <cstdint>
#include <unordered_map>
#include <vector>
namespace hpsdr {
class SignalGenerator {
public:
    SignalGenerator(std::uint32_t sampleRate=48000,double toneOffsetHz=1000.0,double noiseLevel=3e-6,double amplitude=0.3);
    void setSampleRate(std::uint32_t sampleRate){sampleRate_=sampleRate;}
    std::uint32_t sampleRate() const{return sampleRate_;}
    std::vector<std::complex<float>> generateIq(std::size_t nSamples,std::size_t ddcIndex);
private:
    std::uint32_t sampleRate_; double toneOffsetHz_,noiseLevel_,amplitude_;
    std::unordered_map<std::size_t,double> phase_;
};
}