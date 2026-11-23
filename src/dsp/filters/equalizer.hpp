#pragma once

#include "dsp/core/parameter_math.hpp"
#include "dsp/filters/state_variable.hpp"

#include <array>

namespace voiceforge {

struct BandSettings {
    bool enabled = false;
    float frequency = 1000.0f;
    float gainDb = 0.0f;
    float q = 0.707f;
};

inline constexpr std::array<float, 6> eqFrequencies{120.0f, 250.0f, 700.0f, 1800.0f, 4200.0f, 8000.0f};

using EqSettings = std::array<BandSettings, eqFrequencies.size()>;

inline constexpr EqSettings defaultEqSettings() noexcept {
    EqSettings settings{};
    for (std::size_t i = 0; i < settings.size(); ++i)
        settings[i].frequency = eqFrequencies[i];
    return settings;
}

class Equalizer {
public:
    void reset() noexcept { bands_ = {}; }

    void prepare(const EqSettings& settings, double rate, bool initial) noexcept {
        for (std::size_t i = 0; i < bands_.size(); ++i) {
            auto& band = bands_[i];
            const auto& setting = settings[i];
            const double gainDb = bounded(setting.gainDb, 0.0, -18.0, 18.0);
            if (!setting.enabled || gainDb == 0.0) {
                band.target = {band.current.g, band.current.k, 0.0, 0.0, 0.0};
                continue;
            }
            const double maxFrequency = std::min(20000.0, 0.475 * rate);
            const double frequency = bounded(setting.frequency,
                std::min<double>(eqFrequencies[i], maxFrequency), 20.0, maxFrequency);
            const double a = amplitude(gainDb / 2.0);
            const double g = warpedFrequency(frequency, rate);
            const double k = 1.0 / bounded(setting.q, 0.707, 0.25, 8.0);
            const double gainChange = a * a - 1.0;

            if (i == 0)
                band.target = {g / std::sqrt(a), k, 0.0, k * gainChange / (a + 1.0), gainChange};
            else if (i == bands_.size() - 1)
                band.target = {g * std::sqrt(a), k, gainChange, -k * a * gainChange / (a + 1.0), -gainChange};
            else
                band.target = {g, k / a, 0.0, k / a * gainChange, 0.0};

            if (initial)
                band.current = band.target;
            else if (band.current.flat())
                band.current = {band.target.g, band.target.k, 0.0, 0.0, 0.0};
        }
    }

    void process(std::array<double, 2>& frame, unsigned channels, double smoothing) noexcept {
        for (auto& band : bands_) {
            if (band.target.flat() && band.current.flat()) {
                band.current = band.target;
                band.filters = {};
                continue;
            }
            auto& c = band.current;
            const auto& t = band.target;
            c = {approach(c.g, t.g, smoothing), approach(c.k, t.k, smoothing),
                 approach(c.direct, t.direct, smoothing), approach(c.band, t.band, smoothing),
                 approach(c.low, t.low, smoothing)};
            const double normalization = 1.0 / (1.0 + c.g * (c.g + c.k));
            for (unsigned ch = 0; ch < channels; ++ch) {
                const auto [bp, lp] = band.filters[ch].process(frame[ch], c.g, c.k, normalization);
                frame[ch] += c.direct * frame[ch] + c.band * bp + c.low * lp;
            }
        }
    }

private:
    struct Coefficients {
        double g = 0.0, k = 1.0, direct = 0.0, band = 0.0, low = 0.0;
        bool flat() const noexcept { return direct == 0.0 && band == 0.0 && low == 0.0; }
    };

    struct Band {
        Coefficients current, target;
        std::array<StateVariable, 2> filters{};
    };
    std::array<Band, eqFrequencies.size()> bands_{};
};

} // namespace voiceforge
