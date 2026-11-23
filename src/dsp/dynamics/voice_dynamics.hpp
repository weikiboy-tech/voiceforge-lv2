#pragma once

#include "dsp/core/parameter_math.hpp"
#include "dsp/filters/state_variable.hpp"

#include <array>

namespace voiceforge {

struct CompressorSettings {
    bool enabled = false;
    float thresholdDb = -20.0f;
    float ratio = 3.0f;
    float attackMs = 12.0f;
    float releaseMs = 120.0f;
    float kneeDb = 6.0f;
    float makeupDb = 0.0f;
    float mix = 1.0f;
    bool autoMakeup = false;
};

struct ExpanderSettings {
    bool enabled = false;
    float thresholdDb = -48.0f;
    float ratio = 2.0f;
    float attackMs = 4.0f;
    float releaseMs = 140.0f;
    float rangeDb = 18.0f;
};

struct DeEsserSettings {
    bool enabled = false;
    float frequency = 6500.0f;
    float thresholdDb = -24.0f;
    float ratio = 4.0f;
    float attackMs = 2.0f;
    float releaseMs = 75.0f;
    float mix = 1.0f;
};

class GainEnvelope {
public:
    void reset(double value = 0.0) noexcept { db_ = value; }
    double update(double target, double lower, double higher) noexcept {
        db_ = approach(db_, target, target < db_ ? lower : higher);
        return db_;
    }
private:
    double db_ = 0.0;
};

inline double peakOf(const std::array<double, 2>& frame, unsigned channels) noexcept {
    return channels == 2 ? std::max(std::abs(frame[0]), std::abs(frame[1])) : std::abs(frame[0]);
}

inline double overThreshold(double above, double knee) noexcept {
    if (knee == 0.0 || std::abs(above) >= knee / 2.0)
        return std::max(above, 0.0);
    const double across = above + knee / 2.0;
    return (across * across) / (2.0 * knee);
}

class Compressor {
public:
    void prepare(CompressorSettings settings, double rate, bool initial) noexcept;
    double process(std::array<double, 2>& frame, unsigned channels, double smoothing) noexcept;
private:
    GainEnvelope envelope_;
    double threshold_ = 0.0, slope_ = 0.0, attack_ = 0.0, release_ = 0.0, knee_ = 0.0;
    double targetMix_ = 0.0, mix_ = 0.0, targetMakeup_ = 1.0, makeup_ = 1.0;
};

class Expander {
public:
    void prepare(ExpanderSettings settings, double rate, bool initial) noexcept;
    void process(std::array<double, 2>& frame, unsigned channels, double smoothing) noexcept;
private:
    GainEnvelope envelope_;
    double threshold_ = 0.0, slope_ = 0.0, open_ = 0.0, close_ = 0.0, range_ = 0.0;
    double targetMix_ = 0.0, mix_ = 0.0;
};

class DeEsser {
public:
    void prepare(DeEsserSettings settings, double rate, bool initial) noexcept;
    double process(std::array<double, 2>& frame, unsigned channels, double smoothing) noexcept;
private:
    GainEnvelope envelope_;
    std::array<StateVariable, 2> filters_{};
    double threshold_ = 0.0, slope_ = 0.0, attack_ = 0.0, release_ = 0.0;
    double targetMix_ = 0.0, mix_ = 0.0, targetG_ = 0.0, g_ = 0.0;
};

} // namespace voiceforge
