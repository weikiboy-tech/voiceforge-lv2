#include "dsp/core/processor.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace voiceforge {
namespace {
float safeGain(float db) noexcept {
    return std::isfinite(db) ? std::pow(10.0f, std::clamp(db, -24.0f, 24.0f) / 20.0f) : 1.0f;
}

float safeSample(double value) noexcept {
    constexpr double max = std::numeric_limits<float>::max();
    return std::isfinite(value) ? static_cast<float>(std::clamp(value, -max, max)) : 0.0f;
}

double approach(double current, double target, double amount) noexcept {
    const double delta = target - current;
    if (std::abs(delta) < 1e-6)
        return target;
    return current + amount * delta;
}
} // namespace

Processor::Processor(double sampleRate, unsigned channels) noexcept
    : sampleRate_(std::isfinite(sampleRate) && sampleRate >= 8000.0 ? sampleRate : 48000.0),
      channels_(std::clamp(channels, 1u, 2u)),
      smoothing_(-std::expm1(-1.0 / (0.005 * sampleRate_))) {}

void Processor::reset() noexcept {
    filters_ = {};
    primed_ = false;
}

float Processor::Filter::process(float signal, float coefficient) noexcept {
    low = static_cast<float>((1.0 - coefficient) * low + coefficient * static_cast<double>(signal));
    if (std::abs(low) < 1e-25f)
        low = 0.0f;
    return safeSample(static_cast<double>(signal) - low);
}

void Processor::process(const std::array<const float*, 2>& inputs,
                        const std::array<float*, 2>& outputs,
                        std::uint32_t frames, Controls controls) noexcept {
    if (frames == 0)
        return;
    const float inputTarget = safeGain(controls.inputDb);
    const float outputTarget = safeGain(controls.outputDb);
    const float wetTarget = controls.enabled ? 1.0f : 0.0f;
    const float cutoff = std::isfinite(controls.highPassHz) ? controls.highPassHz : 0.0f;
    const float highPassTarget = cutoff > 0.0f ? 1.0f : 0.0f;
    const float coefficientTarget = static_cast<float>(
        -std::expm1(-2.0 * std::numbers::pi * std::clamp(cutoff, 20.0f, 300.0f) / sampleRate_));
    if (!primed_) {
        inputGain_ = inputTarget;
        outputGain_ = outputTarget;
        wet_ = wetTarget;
        highPassCoefficient_ = coefficientTarget;
        highPassMix_ = highPassTarget;
        primed_ = true;
    }

    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        inputGain_ = approach(inputGain_, inputTarget, smoothing_);
        outputGain_ = approach(outputGain_, outputTarget, smoothing_);
        wet_ = approach(wet_, wetTarget, smoothing_);
        highPassCoefficient_ = approach(highPassCoefficient_, coefficientTarget, smoothing_);
        highPassMix_ = approach(highPassMix_, highPassTarget, smoothing_);
        std::array<float, 2> dry{};
        for (unsigned ch = 0; ch < channels_; ++ch)
            dry[ch] = inputs[ch] ? safeSample(inputs[ch][frame]) : 0.0f;

        for (unsigned ch = 0; ch < channels_; ++ch) {
            const float gained = safeSample(static_cast<double>(dry[ch]) * inputGain_);
            const float high = filters_[ch].process(gained, static_cast<float>(highPassCoefficient_));
            const float filtered = std::lerp(gained, high, static_cast<float>(highPassMix_));
            const float wet = safeSample(static_cast<double>(filtered) * outputGain_);
            if (outputs[ch])
                outputs[ch][frame] = std::lerp(dry[ch], wet, static_cast<float>(wet_));
        }
    }
}

} // namespace voiceforge
