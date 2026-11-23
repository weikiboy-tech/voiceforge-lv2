#include "dsp/dynamics/voice_dynamics.hpp"

namespace voiceforge {

void Compressor::prepare(CompressorSettings settings, double rate, bool initial) noexcept {
    threshold_ = bounded(settings.thresholdDb, -20.0, -60.0, 0.0);
    slope_ = 1.0 / bounded(settings.ratio, 3.0, 1.0, 20.0) - 1.0;
    attack_ = stepForMs(bounded(settings.attackMs, 12.0, 0.1, 100.0), rate);
    release_ = stepForMs(bounded(settings.releaseMs, 120.0, 10.0, 1000.0), rate);
    knee_ = bounded(settings.kneeDb, 6.0, 0.0, 24.0);
    const double autoDb = settings.autoMakeup ? std::clamp((threshold_ + 12.0) * slope_, 0.0, 12.0) : 0.0;
    targetMakeup_ = amplitude(std::clamp(autoDb + bounded(settings.makeupDb, 0.0, -12.0, 24.0), -12.0, 24.0));
    targetMix_ = settings.enabled ? bounded(settings.mix, 1.0, 0.0, 1.0) : 0.0;
    if (initial) {
        mix_ = targetMix_;
        makeup_ = targetMakeup_;
    }
}

double Compressor::process(std::array<double, 2>& frame, unsigned channels, double smoothing) noexcept {
    mix_ = approach(mix_, targetMix_, smoothing);
    if (mix_ == 0.0) {
        makeup_ = targetMakeup_;
        envelope_.reset();
        return 1.0;
    }
    makeup_ = approach(makeup_, targetMakeup_, smoothing);
    const double above = decibels(peakOf(frame, channels)) - threshold_;
    const double gain = amplitude(envelope_.update(slope_ * overThreshold(above, knee_), attack_, release_));
    const double applied = std::lerp(1.0, makeup_ * gain, mix_);
    for (unsigned ch = 0; ch < channels; ++ch)
        frame[ch] *= applied;
    return std::lerp(1.0, gain, mix_);
}

void Expander::prepare(ExpanderSettings settings, double rate, bool initial) noexcept {
    threshold_ = bounded(settings.thresholdDb, -48.0, -80.0, -12.0);
    slope_ = bounded(settings.ratio, 2.0, 1.0, 6.0) - 1.0;
    open_ = stepForMs(bounded(settings.attackMs, 4.0, 0.1, 100.0), rate);
    close_ = stepForMs(bounded(settings.releaseMs, 140.0, 10.0, 1000.0), rate);
    range_ = bounded(settings.rangeDb, 18.0, 0.0, 36.0);
    targetMix_ = settings.enabled ? 1.0 : 0.0;
    if (initial) {
        mix_ = targetMix_;
        envelope_.reset(mix_ > 0.0 ? -range_ : 0.0);
    }
}

void Expander::process(std::array<double, 2>& frame, unsigned channels, double smoothing) noexcept {
    mix_ = approach(mix_, targetMix_, smoothing);
    if (mix_ == 0.0) {
        envelope_.reset();
        return;
    }
    const double below = threshold_ - decibels(peakOf(frame, channels));
    const double target = -std::min(range_, slope_ * overThreshold(below, 6.0));
    const double gain = std::lerp(1.0, amplitude(envelope_.update(target, close_, open_)), mix_);
    for (unsigned ch = 0; ch < channels; ++ch)
        frame[ch] *= gain;
}

void DeEsser::prepare(DeEsserSettings settings, double rate, bool initial) noexcept {
    const double maxFrequency = std::min(14000.0, 0.475 * rate);
    targetG_ = warpedFrequency(bounded(settings.frequency, std::min(6500.0, maxFrequency),
        2000.0, maxFrequency), rate);
    threshold_ = bounded(settings.thresholdDb, -24.0, -60.0, 0.0);
    slope_ = 1.0 / bounded(settings.ratio, 4.0, 1.0, 12.0) - 1.0;
    attack_ = stepForMs(bounded(settings.attackMs, 2.0, 0.1, 30.0), rate);
    release_ = stepForMs(bounded(settings.releaseMs, 75.0, 10.0, 400.0), rate);
    targetMix_ = settings.enabled ? bounded(settings.mix, 1.0, 0.0, 1.0) : 0.0;
    if (initial) {
        g_ = targetG_;
        mix_ = targetMix_;
    }
}

double DeEsser::process(std::array<double, 2>& frame, unsigned channels, double smoothing) noexcept {
    mix_ = approach(mix_, targetMix_, smoothing);
    if (mix_ == 0.0) {
        g_ = targetG_;
        envelope_.reset();
        filters_ = {};
        return 1.0;
    }
    g_ = approach(g_, targetG_, smoothing);
    constexpr double damping = 1.414213562373095;
    const double normalization = 1.0 / (1.0 + g_ * (g_ + damping));
    std::array<double, 2> band{};
    for (unsigned ch = 0; ch < channels; ++ch)
        band[ch] = damping * filters_[ch].process(frame[ch], g_, damping, normalization).band;
    const double above = decibels(peakOf(band, channels)) - threshold_;
    const double target = std::max(-24.0, slope_ * overThreshold(above, 3.0));
    const double gain = std::lerp(1.0, amplitude(envelope_.update(target, attack_, release_)), mix_);
    for (unsigned ch = 0; ch < channels; ++ch)
        frame[ch] += (gain - 1.0) * band[ch];
    return gain;
}

} // namespace voiceforge
