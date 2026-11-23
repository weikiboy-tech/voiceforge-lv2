#pragma once

#include "dsp/core/parameter_math.hpp"
#include "dsp/filters/state_variable.hpp"

#include <array>

namespace voiceforge {
class Spectrum {
public:
    static constexpr std::array<double, 12> frequencies{60, 100, 160, 250, 400, 630,
                                                        1000, 1600, 2500, 4000, 6300, 10000};
    explicit Spectrum(double rate) noexcept : smoothing_(stepForMs(95.0, rate)) {
        for (unsigned i = 0; i < bands_.size(); ++i) {
            if (frequencies[i] < .45 * rate) {
                const double g = warpedFrequency(frequencies[i], rate);
                bands_[i].g = g;
                bands_[i].normalization = 1.0 / (1.0 + g * (g + damping));
            }
        }
    }
    void process(const std::array<double, 2>& frame, unsigned channels) noexcept {
        for (auto& band : bands_) {
            if (band.g == 0.0)
                continue;
            double energy = 0.0;
            for (unsigned ch = 0; ch < channels; ++ch) {
                const double signal = damping * band.filters[ch].process(frame[ch], band.g, damping, band.normalization).band;
                energy += signal * signal;
            }
            band.energy += smoothing_ * (energy / channels - band.energy);
            if (band.energy < 1e-24)
                band.energy = 0.0;
        }
    }
    float level(unsigned index) const noexcept {
        return static_cast<float>(std::clamp(10.0 * std::log10(std::max(bands_[index].energy, 1e-12)), -120.0, 800.0));
    }
    void reset() noexcept {
        for (auto& band : bands_) {
            band.energy = 0.0;
            band.filters = {};
        }
    }
private:
    static constexpr double damping = .75;
    struct Band {
        double g = 0.0, normalization = 0.0, energy = 0.0;
        std::array<StateVariable, 2> filters{};
    };
    double smoothing_;
    std::array<Band, frequencies.size()> bands_{};
};
} // namespace voiceforge
