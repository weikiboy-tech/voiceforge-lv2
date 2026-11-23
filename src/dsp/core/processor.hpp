#pragma once

#include "dsp/dynamics/voice_dynamics.hpp"
#include "dsp/filters/equalizer.hpp"
#include "dsp/mastering/peak_limiter.hpp"
#include "dsp/restoration/restoration.hpp"

#include <array>
#include <cstdint>
#include <memory>

namespace voiceforge {

struct Controls {
    float inputDb = 0.0f;
    float outputDb = 0.0f;
    bool enabled = true;
    float highPassHz = 0.0f;
    bool limiterEnabled = false;
    float limiterCeilingDb = -1.0f;
    EqSettings equalizer = defaultEqSettings();
    CompressorSettings compressor;
    ExpanderSettings expander;
    DeEsserSettings deEsser;
    RestorationSettings restoration;
};

class Processor {
public:
    Processor(double sampleRate, unsigned channels);
    void reset() noexcept;
    void process(const std::array<const float*, 2>& inputs,
                 const std::array<float*, 2>& outputs,
                 std::uint32_t frames, const Controls& controls) noexcept;
    float limiterReduction() const noexcept { return limiterReduction_; }
    float compressorReduction() const noexcept { return compressorReduction_; }
    float deEsserReduction() const noexcept { return deEsserReduction_; }
    unsigned latency(bool studio) const noexcept { return studio ? restoration_->delay() : 0; }
    bool voiceActive() const noexcept { return restoration_->voiceActive(); }

private:
    struct Filter {
        float low = 0.0f;
        float process(float signal, float coefficient) noexcept;
    };

    double sampleRate_;
    unsigned channels_;
    double smoothing_;
    bool primed_ = false;
    double inputGain_ = 1.0;
    double outputGain_ = 1.0;
    double wet_ = 1.0;
    double highPassCoefficient_ = 0.0;
    double highPassMix_ = 0.0;
    double limiterMix_ = 0.0;
    double limiterCeiling_ = 1.0;
    std::array<Filter, 2> filters_{};
    std::unique_ptr<Restoration> restoration_;
    Equalizer equalizer_;
    Compressor compressor_;
    Expander expander_;
    DeEsser deEsser_;
    PeakLimiter limiter_;
    float limiterReduction_ = 0.0f;
    float compressorReduction_ = 0.0f;
    float deEsserReduction_ = 0.0f;
};

} // namespace voiceforge
