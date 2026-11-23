#include "dsp/restoration/noise_reduction.hpp"
#include "dsp/core/parameter_math.hpp"

#include <numbers>

namespace voiceforge {

NoiseReduction::NoiseReduction(double rate) noexcept {
    while (size_ < maxSize && size_ < rate * (1024.0 / 48000.0))
        size_ *= 2;

    unsigned mirror = 0;
    for (unsigned i = 0; i < size_; ++i) {
        reversed_[i] = mirror;
        for (unsigned bit = size_ / 2; bit != 0; bit /= 2) {
            mirror ^= bit;
            if ((mirror & bit) != 0)
                break;
        }
        const double angle = 2.0 * std::numbers::pi * i / size_;
        window_[i] = std::sqrt(std::max(0.0, 0.5 - 0.5 * std::cos(angle)));
        if (i < size_ / 2)
            twiddles_[i] = {std::cos(angle), -std::sin(angle)};
    }
}

void NoiseReduction::reset() noexcept {
    history_.fill({});
    delta_.fill({});
    bins_.fill({});
    position_ = learned_ = 0;
    mix_ = 0.0;
}

void NoiseReduction::prepare(const NoiseSettings& s, bool active, bool initial) noexcept {
    const double reduction = bounded(s.reductionDb, 12.0, 0.0, 24.0);
    artifact_ = bounded(s.artifactControl, .55, 0.0, 1.0);
    floor_ = amplitude(-reduction);
    const double threshold = amplitude(bounded(s.thresholdDb, -32.0, -72.0, -18.0));
    thresholdPower_ = threshold * threshold;
    sensitivity_ = std::lerp(.75, 1.8, bounded(s.sensitivity, .55, 0.0, 1.0));
    smoothing_ = bounded(s.smoothing, .55, 0.0, 1.0);
    protection_ = bounded(s.speechProtection, .65, 0.0, 1.0);
    targetMix_ = active && s.enabled && reduction != 0.0 ? 1.0 : 0.0;
    if (initial)
        mix_ = targetMix_;
}

std::array<double, 2> NoiseReduction::process(const std::array<double, 2>& input,
                                             unsigned channels, bool speech,
                                             double smoothing) noexcept {
    mix_ = approach(mix_, targetMix_, smoothing);
    auto output = history_[position_];
    for (unsigned ch = 0; ch < channels; ++ch)
        output[ch] += mix_ * delta_[position_][ch];

    delta_[position_] = {};
    history_[position_] = input;
    position_ = (position_ + 1) & (size_ - 1);
    if ((position_ & (size_ / 4 - 1)) == 0 && targetMix_ != 0.0)
        analyze(channels, speech);
    return output;
}

void NoiseReduction::transform(std::array<Complex, maxSize>& spectrum, bool inverse) const noexcept {
    for (unsigned i = 0; i < size_; ++i)
        if (i < reversed_[i])
            std::swap(spectrum[i], spectrum[reversed_[i]]);
    for (unsigned width = 2; width <= size_; width *= 2) {
        const unsigned half = width / 2;
        const unsigned stride = size_ / width;
        for (unsigned begin = 0; begin < size_; begin += width) {
            for (unsigned k = 0; k < half; ++k) {
                const auto root = twiddles_[k * stride];
                const auto even = spectrum[begin + k];
                const auto odd = spectrum[begin + k + half] * (inverse ? std::conj(root) : root);
                spectrum[begin + k] = even + odd;
                spectrum[begin + k + half] = even - odd;
            }
        }
    }
}

void NoiseReduction::analyze(unsigned channels, bool speech) noexcept {
    double framePower = 0.0;
    const unsigned wrap = size_ - 1;
    for (unsigned ch = 0; ch < channels; ++ch) {
        double channelPower = 0.0;
        auto& spectrum = spectrum_[ch];
        for (unsigned i = 0; i < size_; ++i) {
            const double input = history_[(position_ + i) & wrap][ch];
            channelPower += input * input;
            spectrum[i] = input * window_[i];
        }
        framePower = std::max(framePower, channelPower / size_);
        transform(spectrum, false);
    }
    const bool learn = !speech && framePower < thresholdPower_ && framePower > 1e-11;
    const unsigned nyquist = size_ / 2;
    for (unsigned k = 0; k <= nyquist; ++k) {
        auto& bin = bins_[k];
        double power = std::norm(spectrum_[0][k]);
        if (channels == 2)
            power = std::max(power, std::norm(spectrum_[1][k]));
        bin.power = std::lerp(bin.power, power, 0.46);
        if (learn) {
            const double weight = learned_ == 0 ? 1.0 : (learned_ < 24 ? 0.16 : 0.045);
            bin.noise = std::lerp(bin.noise, bin.power, weight);
        }
        const double excess = sensitivity_ * bin.noise / std::max(bin.power, 1e-16);
        const double wiener = std::clamp(1.0 - excess, floor_, 1.0);
        target_[k] = speech ? std::lerp(wiener, 1.0, 0.50 * protection_) : wiener;
    }
    if (learn && learned_ < 24)
        ++learned_;

    for (unsigned k = 0; k <= nyquist; ++k) {
        const double neighborhood = 0.5 * (target_[k > 0 ? k - 1 : k] + target_[std::min(k + 1, nyquist)]);
        auto& gain = bins_[k].gain;
        const double target = std::lerp(target_[k], neighborhood, 0.48 * artifact_);
        const double weight = std::lerp(0.64, 0.20, smoothing_);
        gain = std::lerp(gain, target, target > gain ? std::max(weight, 0.55) : weight);
        const double correction = gain - 1.0;
        for (unsigned ch = 0; ch < channels; ++ch) {
            spectrum_[ch][k] *= correction;
            if (k != 0 && k != nyquist)
                spectrum_[ch][size_ - k] *= correction;
        }
    }

    // Four overlapping square-root Hann windows sum to two after synthesis.
    const double scale = 0.5 / size_;
    for (unsigned ch = 0; ch < channels; ++ch) {
        auto& spectrum = spectrum_[ch];
        transform(spectrum, true);
        for (unsigned i = 0; i < size_; ++i)
            delta_[(position_ + i) & wrap][ch] += spectrum[i].real() * window_[i] * scale;
    }
}

} // namespace voiceforge
