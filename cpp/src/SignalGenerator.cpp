#include "hpsdr/SignalGenerator.h"
#include <cmath>
#include <random>
namespace hpsdr {
SignalGenerator::SignalGenerator(std::uint32_t sampleRate,double toneOffsetHz,double noiseLevel,double amplitude)
 :sampleRate_(sampleRate),toneOffsetHz_(toneOffsetHz),noiseLevel_(noiseLevel),amplitude_(amplitude){}
std::vector<std::complex<float>> SignalGenerator::generateIq(std::size_t nSamples,std::size_t ddcIndex){
 static thread_local std::mt19937_64 rng{std::random_device{}()};
 std::normal_distribution<double> normal(0.0,1.0); double phase=phase_[ddcIndex];
 std::vector<std::complex<float>> iq(nSamples);
 for(std::size_t n=0;n<nSamples;++n){double t=double(n)/sampleRate_+phase;double a=2.0*std::acos(-1.0)*toneOffsetHz_*t;iq[n]={float(amplitude_*std::cos(a)+noiseLevel_*normal(rng)),float(amplitude_*std::sin(a)+noiseLevel_*normal(rng))};}
 phase+=double(nSamples)/sampleRate_;
 if(phase>1e6) phase=toneOffsetHz_!=0.0?std::fmod(phase,1.0/toneOffsetHz_):0.0;
 phase_[ddcIndex]=phase; return iq;
}
}