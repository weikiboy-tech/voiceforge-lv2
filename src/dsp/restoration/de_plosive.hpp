#pragma once

#include "dsp/core/parameter_math.hpp"

#include <array>
#include <numbers>

namespace voiceforge {

struct DePlosiveSettings {
    bool enabled = false;
    float sensitivity = 0.55f;
    float frequency = 135.0f;
    float reductionDb = 14.0f;
    float recoveryMs = 100.0f;
};

class DePlosive {
public:
    void prepare(const DePlosiveSettings& settings, double rate, bool initial) noexcept {
        const double frequency = bounded(settings.frequency, 135.0, 50.0, 220.0);
        targetStep_ = -std::expm1(-2.0 * std::numbers::pi * frequency / rate);
        sensitivity_ = bounded(settings.sensitivity, 0.55, 0.0, 1.0);
        targetDepth_ = settings.enabled ? 1.0 - amplitude(-bounded(settings.reductionDb, 14.0, 0.0, 24.0)) : 0.0;
        release_ = stepForMs(bounded(settings.recoveryMs, 100.0, 25.0, 400.0), rate);
        attack_ = stepForMs(2.5, rate);
        detector_ = stepForMs(6.0, rate);
        baseline_ = stepForMs(170.0, rate);
        if (initial) {
            step_ = targetStep_;
            depth_ = targetDepth_;
        }
    }

    void process(std::array<double, 2>& frame, unsigned channels, double smoothing) noexcept {
        depth_ = approach(depth_, targetDepth_, smoothing);
        if (depth_ == 0.0) {
            step_ = targetStep_;
            low_ = {};
            lowEnergy_ = totalEnergy_ = lowBaseline_ = amount_ = 0.0;
            return;
        }
        step_ = approach(step_, targetStep_, smoothing);
        double lowPeak = 0.0, peak = 0.0;
        for (unsigned ch = 0; ch < channels; ++ch) {
            low_[ch] += step_ * (frame[ch] - low_[ch]);
            if (std::abs(low_[ch]) < 1e-24)
                low_[ch] = 0.0;
            lowPeak = std::max(lowPeak, low_[ch] * low_[ch]);
            peak = std::max(peak, frame[ch] * frame[ch]);
        }
        lowEnergy_ = approach(lowEnergy_, lowPeak, detector_);
        totalEnergy_ = approach(totalEnergy_, peak, detector_);
        lowBaseline_ = approach(lowBaseline_, lowPeak, baseline_);
        const double absolute = std::lerp(0.004, 0.00012, sensitivity_);
        const double relative = std::lerp(5.0, 1.6, sensitivity_);
        const double threshold = std::max(absolute, relative * lowBaseline_);
        const double onset = std::clamp((lowEnergy_ / threshold - 1.0) / 1.15, 0.0, 1.0);
        const double bassRatio = lowEnergy_ / std::max(totalEnergy_, 1e-12);
        const double bass = std::clamp((bassRatio - 0.35) / 0.32, 0.0, 1.0);
        const double target = onset * bass;
        amount_ = approach(amount_, target, target > amount_ ? attack_ : release_);
        for (unsigned ch = 0; ch < channels; ++ch)
            frame[ch] -= depth_ * amount_ * low_[ch];
    }

private:
    std::array<double, 2> low_{};
    double targetStep_ = 0.0, step_ = 0.0, sensitivity_ = 0.55;
    double targetDepth_ = 0.0, depth_ = 0.0;
    double release_ = 0.0, attack_ = 0.0, detector_ = 0.0, baseline_ = 0.0;
    double lowEnergy_ = 0.0, totalEnergy_ = 0.0, lowBaseline_ = 0.0, amount_ = 0.0;
};

} // namespace voiceforge
