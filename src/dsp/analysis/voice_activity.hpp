#pragma once

#include "dsp/core/parameter_math.hpp"

#include <array>
#include <numbers>

namespace voiceforge {

class VoiceActivity {
public:
    void configure(double rate) noexcept {
        frameSize_ = static_cast<unsigned>(std::round(rate * 0.010));
        lowStep_ = -std::expm1(-2.0 * std::numbers::pi * 180.0 / rate);
        highStep_ = -std::expm1(-2.0 * std::numbers::pi * 3600.0 / rate);
    }

    void reset() noexcept {
        channels_ = {};
        count_ = hits_ = hold_ = 0;
        floor_ = 1e-5;
        active_ = false;
    }

    void process(const std::array<double, 2>& signal, unsigned channels, double sensitivity) noexcept {
        for (unsigned i = 0; i < channels; ++i) {
            auto& state = channels_[i];
            state.low += lowStep_ * (signal[i] - state.low);
            state.high += highStep_ * (signal[i] - state.high);
            const double band = state.high - state.low;
            state.total += signal[i] * signal[i];
            state.band += band * band;
        }
        if (++count_ < frameSize_)
            return;

        double band = 0.0, total = 0.0;
        for (unsigned i = 0; i < channels; ++i) {
            const auto& state = channels_[i];
            if (state.band > band) {
                band = state.band;
                total = state.total;
            }
            if (std::abs(state.low) < 1e-24)
                channels_[i].low = 0.0;
            if (std::abs(state.high) < 1e-24)
                channels_[i].high = 0.0;
            channels_[i].total = channels_[i].band = 0.0;
        }
        band /= count_;
        total /= count_;
        count_ = 0;
        const double minimum = std::lerp(6.3e-5, 2.5e-6, sensitivity);
        const double ratio = std::lerp(8.0, 2.8, sensitivity) * (active_ ? 0.55 : 1.0);
        const bool candidate = band > std::max(minimum, floor_ * ratio) && band > 0.28 * total;
        if (candidate) {
            hits_ = std::min(hits_ + 1, 3u);
            if (hits_ == 3)
                hold_ = 15;
        } else {
            hits_ = 0;
            if (hold_ != 0)
                --hold_;
            const double target = std::clamp(band, 1e-8, 1e-2);
            floor_ += (target < floor_ ? 0.16 : 0.045) * (target - floor_);
        }
        active_ = hold_ != 0;
    }

    bool active() const noexcept { return active_; }

private:
    struct Channel {
        double low = 0.0, high = 0.0, band = 0.0, total = 0.0;
    };
    std::array<Channel, 2> channels_{};
    unsigned frameSize_ = 1, count_ = 0, hits_ = 0, hold_ = 0;
    double lowStep_ = 0.0, highStep_ = 0.0, floor_ = 1e-5;
    bool active_ = false;
};

} // namespace voiceforge
