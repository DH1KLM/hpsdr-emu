#pragma once
#include <complex>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>
namespace hpsdr {
//DH1KLM: TX IQ is recorded per frequency and replayed as a low-level RF echo.
class EchoBuffer {
public:
 static constexpr double AttenuationDb=80.0;
 static constexpr double Attenuation=0.0001;
 explicit EchoBuffer(std::uint32_t sampleRate=48000,double maxDuration=10.0);
 void setSampleRate(std::uint32_t rate){sampleRate_=rate;}
 void startRecording(std::uint32_t txFrequency);
 void feed(const std::vector<std::complex<float>>& samples);
 void stopRecording();
 std::vector<std::complex<float>> generateEcho(std::size_t nSamples,std::uint32_t rxFrequency,std::uint32_t sampleRate);
private:
 void commit();
 std::uint32_t sampleRate_; double maxDuration_;
 std::unordered_map<std::uint32_t,std::vector<std::complex<float>>> echoes_;
 std::vector<std::complex<float>> recording_; std::uint32_t recordingFrequency_=0; bool recordingActive_=false;
 std::unordered_map<std::uint32_t,std::size_t> playbackPos_;
 std::unordered_map<std::uint32_t,double> shiftPhase_;
};
}