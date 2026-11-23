#pragma once

#include <algorithm>
#include <cmath>

namespace voiceforge {

inline double bounded(double value, double fallback, double lower, double upper) noexcept {
    return std::isfinite(value) ? std::clamp(value, lower, upper) : fallback;
}

inline double amplitude(double db) noexcept {
    return std::pow(10.0, db / 20.0);
}

inline double decibels(double gain) noexcept {
    return 20.0 * std::log10(std::max(gain, 1e-12));
}

inline double stepForMs(double milliseconds, double sampleRate) noexcept {
    return -std::expm1(-1000.0 / (milliseconds * sampleRate));
}

inline double approach(double current, double target, double amount) noexcept {
    const double delta = target - current;
    return std::abs(delta) < 1e-6 ? target : current + amount * delta;
}

} // namespace voiceforge
