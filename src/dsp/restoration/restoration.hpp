#pragma once

#include "dsp/analysis/voice_activity.hpp"
#include "dsp/restoration/de_click.hpp"
#include "dsp/restoration/de_plosive.hpp"
#include "dsp/restoration/noise_reduction.hpp"

namespace voiceforge {

struct RestorationSettings {
    bool studio = false;
    DeClickSettings click;
    DePlosiveSettings plosive;
    NoiseSettings noise;
    RoomSettings room;
    float vadSensitivity = 0.5f;
};

class Restoration {
public:
    explicit Restoration(double sampleRate) noexcept : rate_(sampleRate), noise_(sampleRate) {
        click_.configure(rate_);
        activity_.configure(rate_);
    }

    void reset() noexcept {
        click_.reset();
        plosive_ = {};
        noise_.reset();
        activity_.reset();
        dryDelay_.fill({});
        dryPosition_ = 0;
    }

    void prepare(const RestorationSettings& s, bool initial) noexcept {
        click_.prepare(s.click, rate_, s.studio, initial);
        plosive_.prepare(s.plosive, rate_, initial);
        noise_.prepare(s.noise, s.studio, initial, s.room);
        vadSensitivity_ = bounded(s.vadSensitivity, .5, 0.0, 1.0);
        targetStudio_ = s.studio ? 1.0 : 0.0;
        if (initial)
            studio_ = targetStudio_;
    }

    void process(std::array<double, 2>& dry, std::array<double, 2>& wet,
                 unsigned channels, double smoothing) noexcept {
        activity_.process(wet, channels, vadSensitivity_);
        const auto repaired = click_.process(wet, channels, smoothing);
        const auto studio = noise_.process(repaired, channels, activity_.active(), smoothing);
        const auto delayedDry = dryDelay_[dryPosition_];
        dryDelay_[dryPosition_] = dry;
        if (++dryPosition_ == delay())
            dryPosition_ = 0;

        studio_ = approach(studio_, targetStudio_, smoothing);
        for (unsigned ch = 0; ch < channels; ++ch) {
            dry[ch] = std::lerp(dry[ch], delayedDry[ch], studio_);
            wet[ch] = std::lerp(wet[ch], studio[ch], studio_);
        }
        plosive_.process(wet, channels, smoothing);
    }

    unsigned delay() const noexcept { return click_.delay() + noise_.delay(); }
    bool voiceActive() const noexcept { return activity_.active(); }

private:
    double rate_;
    std::array<std::array<double, 2>, NoiseReduction::maxSize + 256> dryDelay_{};
    unsigned dryPosition_ = 0;
    double studio_ = 0.0, targetStudio_ = 0.0, vadSensitivity_ = .5;
    VoiceActivity activity_;
    DeClick click_;
    DePlosive plosive_;
    NoiseReduction noise_;
};

} // namespace voiceforge
