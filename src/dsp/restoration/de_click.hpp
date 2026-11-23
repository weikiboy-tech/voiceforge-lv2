#pragma once

#include "dsp/core/parameter_math.hpp"

#include <array>

namespace voiceforge {

struct DeClickSettings {
    bool enabled = false;
    float sensitivity = 0.55f;
    float strength = 0.85f;
    float maximumMs = 0.25f;
};

class DeClick {
public:
    void configure(double rate) noexcept {
        delay_ = static_cast<unsigned>(std::ceil(rate * 0.00065)) + 2;
        tracking_ = stepForMs(7.0, rate);
    }

    void reset() noexcept {
        ring_ = {};
        states_ = {};
        position_ = 0;
        strength_ = 0.0;
    }

    void prepare(const DeClickSettings& settings, double rate, bool active, bool initial) noexcept {
        const double sensitivity = bounded(settings.sensitivity, 0.55, 0.0, 1.0);
        minimumJump_ = std::lerp(0.064, 0.012, sensitivity);
        transientRatio_ = std::lerp(13.0, 5.0, sensitivity);
        maxLength_ = static_cast<unsigned>(std::max(1.0, std::round(rate * .001 *
            bounded(settings.maximumMs, 0.25, 0.05, 0.65))));
        targetStrength_ = settings.enabled && active ? bounded(settings.strength, .85, 0.0, 1.0) : 0.0;
        if (initial)
            strength_ = targetStrength_;
    }

    std::array<double, 2> process(const std::array<double, 2>& input, unsigned channels,
                                  double smoothing) noexcept {
        ring_[position_] = input;
        const auto start = (position_ + ringSize - delay_) % ringSize;
        position_ = (position_ + 1) % ringSize;
        strength_ = approach(strength_, targetStrength_, smoothing);
        auto result = ring_[start];
        for (unsigned ch = 0; ch < channels; ++ch) {
            auto& state = states_[ch];
            const double raw = result[ch];
            const double jump = raw - state.previous;
            const double limit = std::max(minimumJump_, transientRatio_ * state.gradient);
            const bool abrupt = std::abs(jump - state.previousSlope) > 0.68 * std::abs(jump);
            if (strength_ != 0.0 && state.remaining == 0 && std::abs(jump) > limit && abrupt)
                findEnd(state, start, ch, jump, limit);
            if (state.remaining != 0) {
                state.interpolated += state.increment;
                --state.remaining;
                result[ch] = std::lerp(raw, state.interpolated, strength_);
            }
            const double repaired = state.remaining != 0 || result[ch] != raw ? state.interpolated : raw;
            state.previousSlope = repaired - state.previous;
            const double change = std::abs(state.previousSlope);
            state.gradient += tracking_ * (std::min(change, 0.03 + 4.0 * state.gradient) - state.gradient);
            state.previous = repaired;
        }
        return result;
    }

    unsigned delay() const noexcept { return delay_; }

private:
    struct State {
        double gradient = 0.002, previous = 0.0, previousSlope = 0.0;
        double interpolated = 0.0, increment = 0.0;
        unsigned remaining = 0;
    };

    void findEnd(State& state, unsigned start, unsigned ch, double jump, double limit) const noexcept {
        const double tolerance = std::min(.35 * std::abs(jump), std::max(.014, 4.0 * state.gradient));
        double interior = ring_[start][ch];
        for (unsigned length = 1; length <= maxLength_; ++length) {
            const double right = ring_[(start + length) % ringSize][ch];
            const double next = ring_[(start + length + 1) % ringSize][ch];
            const bool returns = std::abs(right - state.previous) <= tolerance + length * state.gradient;
            const bool settles = std::abs(next - right) <= std::max(tolerance, 0.5 * limit);
            const double step = right - interior;
            const bool abrupt = std::abs(step - (next - right)) > 0.65 * std::abs(step);
            if (returns && settles && abrupt && std::abs(step) >= 0.53 * limit) {
                state.interpolated = state.previous;
                state.increment = (right - state.previous) / (length + 1);
                state.remaining = length;
                return;
            }
            interior = right;
        }
    }

    static constexpr unsigned ringSize = 256;
    std::array<std::array<double, 2>, ringSize> ring_{};
    std::array<State, 2> states_{};
    unsigned position_ = 0, delay_ = 3, maxLength_ = 1;
    double tracking_ = 0.0, minimumJump_ = 0.03, transientRatio_ = 9.0;
    double targetStrength_ = 0.0, strength_ = 0.0;
};

} // namespace voiceforge
