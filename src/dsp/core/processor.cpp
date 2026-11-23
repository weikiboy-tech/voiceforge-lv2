#include "dsp/core/processor.hpp"
#include "dsp/core/parameter_math.hpp"

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

} // namespace

Processor::Processor(double sampleRate, unsigned channels)
    : sampleRate_(bounded(sampleRate, 48000.0, 8000.0, 384000.0)),
      channels_(std::clamp(channels, 1u, 2u)),
      smoothing_(-std::expm1(-1.0 / (0.005 * sampleRate_))),
      dcStep_(-std::expm1(-2.0 * std::numbers::pi * 5.0 / sampleRate_)),
      restoration_(std::make_unique<Restoration>(sampleRate_)),
      limiter_(sampleRate_), loudness_(sampleRate_), analysis_(sampleRate_), spectrum_(sampleRate_) {}

void Processor::reset() noexcept {
    filters_ = {};
    dcFilters_ = {};
    restoration_->reset();
    equalizer_.reset();
    compressor_ = {};
    expander_ = {};
    deEsser_ = {};
    limiter_.reset();
    rider_ = {};
    loudness_.reset();
    analysis_.clear();
    spectrum_.reset();
    analysis_.command(false, false);
    measurements_ = {};
    meterReset_ = false;
    limiterReduction_ = compressorReduction_ = deEsserReduction_ = 0.0f;
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
                        std::uint32_t frames, const Controls& controls) noexcept {
    if (frames == 0)
        return;
    const float inputTarget = safeGain(controls.inputDb);
    const float outputTarget = safeGain(controls.outputDb);
    const float wetTarget = controls.enabled ? 1.0f : 0.0f;
    const double phaseTarget = controls.phaseInvert ? -1.0 : 1.0;
    const double dcTarget = controls.dcRemoval ? 1.0 : 0.0;
    const float cutoff = std::isfinite(controls.highPassHz) ? controls.highPassHz : 0.0f;
    const float highPassTarget = cutoff > 0.0f ? 1.0f : 0.0f;
    const double limiterTarget = controls.limiterEnabled ? 1.0 : 0.0;
    const double ceilingTarget = safeGain(std::isfinite(controls.limiterCeilingDb)
        ? std::clamp(controls.limiterCeilingDb, -12.0f, 0.0f) : -1.0f);
    const float coefficientTarget = static_cast<float>(
        -std::expm1(-2.0 * std::numbers::pi * std::clamp(cutoff, 20.0f, 300.0f) / sampleRate_));
    equalizer_.prepare(controls.equalizer, sampleRate_, !primed_);
    compressor_.prepare(controls.compressor, sampleRate_, !primed_);
    expander_.prepare(controls.expander, sampleRate_, !primed_);
    deEsser_.prepare(controls.deEsser, sampleRate_, !primed_);
    restoration_->prepare(controls.restoration, !primed_);
    rider_.prepare(controls.rider, sampleRate_, !primed_);
    if (controls.resetMeter && !meterReset_) {
        loudness_.reset();
        measurements_ = {};
    }
    meterReset_ = controls.resetMeter;
    analysis_.command(controls.analyze, controls.resetAnalysis, channels_);
    if (!primed_) {
        inputGain_ = inputTarget;
        outputGain_ = outputTarget;
        wet_ = wetTarget;
        phase_ = phaseTarget;
        dcMix_ = dcTarget;
        highPassCoefficient_ = coefficientTarget;
        highPassMix_ = highPassTarget;
        limiterMix_ = limiterTarget;
        limiterCeiling_ = ceilingTarget;
        primed_ = true;
    }

    double minimumGain = 1.0;
    double compressorGain = 1.0;
    double deEsserGain = 1.0;
    double peakIn = 0.0, peakOut = 0.0, inputPower = 0.0;
    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        inputGain_ = approach(inputGain_, inputTarget, smoothing_);
        outputGain_ = approach(outputGain_, outputTarget, smoothing_);
        wet_ = approach(wet_, wetTarget, smoothing_);
        phase_ = approach(phase_, phaseTarget, smoothing_);
        dcMix_ = approach(dcMix_, dcTarget, smoothing_);
        highPassCoefficient_ = approach(highPassCoefficient_, coefficientTarget, smoothing_);
        highPassMix_ = approach(highPassMix_, highPassTarget, smoothing_);
        limiterMix_ = approach(limiterMix_, limiterTarget, smoothing_);
        limiterCeiling_ = approach(limiterCeiling_, ceilingTarget, smoothing_);
        std::array<double, 2> dry{}, wet{};
        for (unsigned ch = 0; ch < channels_; ++ch) {
            dry[ch] = inputs[ch] ? safeSample(inputs[ch][frame]) : 0.0f;
            peakIn = std::max(peakIn, std::abs(dry[ch]));
            inputPower += dry[ch] * dry[ch];
            const float sample = safeSample(dry[ch] * inputGain_ * phase_);
            const float blocked = dcFilters_[ch].process(sample, static_cast<float>(dcStep_));
            wet[ch] = std::lerp(sample, blocked, static_cast<float>(dcMix_));
        }

        analysis_.process(dry, channels_, voiceActive());
        if (controls.spectrum)
            spectrum_.process(dry, channels_);
        restoration_->process(dry, wet, channels_, smoothing_);
        for (unsigned ch = 0; ch < channels_; ++ch) {
            const float gained = safeSample(wet[ch]);
            const float high = filters_[ch].process(gained, static_cast<float>(highPassCoefficient_));
            wet[ch] = std::lerp(gained, high, static_cast<float>(highPassMix_));
        }
        expander_.process(wet, channels_, smoothing_);
        equalizer_.process(wet, channels_, smoothing_);
        const double ess = deEsser_.process(wet, channels_, smoothing_);
        rider_.process(wet, channels_, smoothing_, voiceActive());
        const double comp = compressor_.process(wet, channels_, smoothing_);
        deEsserGain = std::min(deEsserGain, std::lerp(1.0, ess, wet_));
        compressorGain = std::min(compressorGain, std::lerp(1.0, comp, wet_));
        for (unsigned ch = 0; ch < channels_; ++ch)
            wet[ch] *= outputGain_;
        const double gain = std::lerp(1.0, limiter_.process(peakOf(wet, channels_), limiterCeiling_), limiterMix_);
        minimumGain = std::min(minimumGain, std::lerp(1.0, gain, wet_));
        std::array<float, 2> final{};
        for (unsigned ch = 0; ch < channels_; ++ch) {
            final[ch] = std::lerp(safeSample(dry[ch]), safeSample(wet[ch] * gain),
                                  static_cast<float>(wet_));
            peakOut = std::max(peakOut, std::abs(static_cast<double>(final[ch])));
            if (outputs[ch])
                outputs[ch][frame] = final[ch];
        }
        loudness_.process(final, channels_);
    }
    limiterReduction_ = static_cast<float>(-20.0 * std::log10(minimumGain));
    compressorReduction_ = static_cast<float>(-decibels(compressorGain));
    deEsserReduction_ = static_cast<float>(-decibels(deEsserGain));
    measurements_.inputPeak = static_cast<float>(std::max(-120.0, decibels(peakIn)));
    measurements_.inputRms = static_cast<float>(std::max(-120.0,
        decibels(std::sqrt(inputPower / (static_cast<double>(frames) * channels_)))));
    measurements_.outputPeak = static_cast<float>(std::max(-120.0, decibels(peakOut)));
    measurements_.truePeak = loudness_.truePeak();
    measurements_.momentary = loudness_.momentary();
    measurements_.shortTerm = loudness_.shortTerm();
    measurements_.integrated = loudness_.integrated();
    measurements_.inputClip = measurements_.inputClip || peakIn >= 1.0;
    measurements_.outputClip = measurements_.outputClip || peakOut >= 1.0 || measurements_.truePeak >= 0.0f;
}

} // namespace voiceforge
