#pragma once

#include "dsp/core/parameter_math.hpp"

#include <array>

namespace voiceforge {

struct RiderSettings {
    bool enabled = false;
    float targetDb = -20.0f;
    float speedMs = 650.0f;
    float maximumGainDb = 9.0f;
    float maximumReductionDb = 12.0f;
    float voiceInfluence = 1.0f;
};

class LevelRider {
public:
    void prepare(const RiderSettings& s, double rate, bool initial) noexcept {
        target_ = bounded(s.targetDb, -20.0, -36.0, -9.0);
        boost_ = bounded(s.maximumGainDb, 9.0, 0.0, 18.0);
        reduction_ = bounded(s.maximumReductionDb, 12.0, 0.0, 24.0);
        influence_ = bounded(s.voiceInfluence, 1.0, 0.0, 1.0);
        speed_ = stepForMs(bounded(s.speedMs, 650.0, 180.0, 3000.0), rate);
        energyRate_ = stepForMs(80.0, rate);
        targetMix_ = s.enabled ? 1.0 : 0.0;
        if (initial)
            mix_ = targetMix_;
    }

    void process(std::array<double, 2>& frame, unsigned channels, double smoothing, bool voice) noexcept {
        mix_ = approach(mix_, targetMix_, smoothing);
        if (mix_ == 0.0) {
            gain_ = energy_ = appliedDb_ = 0.0;
            return;
        }
        double power = 0.0;
        for (unsigned ch = 0; ch < channels; ++ch)
            power = std::max(power, frame[ch] * frame[ch]);
        energy_ += energyRate_ * (power - energy_);
        if (energy_ < 1e-24)
            energy_ = 0.0;
        const double level = 10.0 * std::log10(std::max(energy_, 1e-24));
        const double wanted = std::clamp(target_ - level, -reduction_, boost_);
        const double gate = std::clamp((level + 52.0) / 12.0, 0.0, 1.0);
        const double confidence = voice ? 1.0 : 1.0 - influence_;
        gain_ = approach(gain_, wanted * gate * confidence, speed_);
        appliedDb_ = gain_ * mix_;
        const double factor = amplitude(appliedDb_);
        for (unsigned ch = 0; ch < channels; ++ch)
            frame[ch] *= factor;
    }

    float appliedDb() const noexcept { return static_cast<float>(appliedDb_); }

private:
    double target_ = -20.0, boost_ = 0.0, reduction_ = 0.0, influence_ = 1.0;
    double speed_ = 0.0, energyRate_ = 0.0, energy_ = 0.0, gain_ = 0.0, appliedDb_ = 0.0;
    double targetMix_ = 0.0, mix_ = 0.0;
};

} // namespace voiceforge
