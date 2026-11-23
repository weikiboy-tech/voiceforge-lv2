#pragma once

#include <cmath>
#include <numbers>

namespace voiceforge {

inline double warpedFrequency(double frequency, double sampleRate) noexcept {
    return std::tan(std::numbers::pi * frequency / sampleRate);
}

struct StateVariable {
    double first = 0.0;
    double second = 0.0;

    struct Taps { double band; double low; };

    Taps process(double input, double g, double k, double normalization) noexcept {
        const double band = (first + g * (input - second)) * normalization;
        const double low = second + g * band;
        first = 2.0 * band - first;
        second = 2.0 * low - second;
        if (std::abs(first) < 1e-30)
            first = 0.0;
        if (std::abs(second) < 1e-30)
            second = 0.0;
        return {band, low};
    }
};

} // namespace voiceforge
