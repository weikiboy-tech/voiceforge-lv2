#include "dsp/mastering/loudness.hpp"
#include "dsp/core/parameter_math.hpp"

#include <numbers>

namespace voiceforge {
namespace {
constexpr double offset = -0.691;
float lufs(double power) noexcept {
    return static_cast<float>(std::max(-120.0, offset + 10.0 * std::log10(std::max(power, 1e-20))));
}

double sinc(double x) noexcept {
    return x == 0.0 ? 1.0 : std::sin(std::numbers::pi * x) / (std::numbers::pi * x);
}
} // namespace

Loudness::Loudness(double rate, bool measureTruePeak) noexcept {
    const double head = std::tan(std::numbers::pi * 1681.974450955533 / rate);
    const double shelfQ = 0.7071752369554196;
    const double shelfNorm = 1.0 + head / shelfQ + head * head;
    const double high = amplitude(3.999843853973347);
    const double band = std::pow(high, 0.4996667741545416);
    shelf_ = {(high + band * head / shelfQ + head * head) / shelfNorm,
              2.0 * (head * head - high) / shelfNorm,
              (high - band * head / shelfQ + head * head) / shelfNorm,
              2.0 * (head * head - 1.0) / shelfNorm,
              (1.0 - head / shelfQ + head * head) / shelfNorm};
    const double rlb = std::tan(std::numbers::pi * 38.13547087602444 / rate);
    const double q = 0.5003270373238773;
    const double rlbNorm = 1.0 + rlb / q + rlb * rlb;
    highPass_ = {1.0, -2.0, 1.0, 2.0 * (rlb * rlb - 1.0) / rlbNorm,
                 (1.0 - rlb / q + rlb * rlb) / rlbNorm};

    tickSamples_ = static_cast<unsigned>(std::round(rate / 10.0));
    phaseCount_ = measureTruePeak ? (rate >= 96000.0 ? 1u : 3u) : 0u;
    for (unsigned phase = 0; phase < phaseCount_; ++phase) {
        const double fraction = static_cast<double>(phase + 1) / (phaseCount_ + 1);
        double sum = 0.0;
        for (unsigned i = 0; i < taps; ++i) {
            const double x = i - double(taps / 2 - 1) - fraction;
            const double w = 0.42 + 0.5 * std::cos(std::numbers::pi * x / (taps / 2)) +
                0.08 * std::cos(2.0 * std::numbers::pi * x / (taps / 2));
            interpolator_[phase][i] = sinc(x) * w;
            sum += interpolator_[phase][i];
        }
        for (auto& coefficient : interpolator_[phase])
            coefficient /= sum;
    }
}

void Loudness::reset() noexcept {
    shelfState_ = highPassState_ = {};
    history_ = {};
    blocks_.fill(0.0);
    histogram_.fill({});
    position_ = blockPosition_ = blockCount_ = tickCount_ = 0;
    energy_ = peak_ = gatedEnergy_ = 0.0;
    gatedCount_ = 0;
    momentary_ = shortTerm_ = integrated_ = -120.0f;
}

double Loudness::State::filter(double x, const Coefficients& c) noexcept {
    const double y = c.b0 * x + z1;
    z1 = c.b1 * x - c.a1 * y + z2;
    z2 = c.b2 * x - c.a2 * y;
    if (std::abs(z1) < 1e-30)
        z1 = 0.0;
    if (std::abs(z2) < 1e-30)
        z2 = 0.0;
    return y;
}

void Loudness::process(const std::array<float, 2>& frame, unsigned channels) noexcept {
    for (unsigned ch = 0; ch < channels; ++ch) {
        const double sample = frame[ch];
        const double weighted = highPassState_[ch].filter(shelfState_[ch].filter(sample, shelf_), highPass_);
        energy_ += weighted * weighted;
        auto& buffer = history_[ch];
        buffer[position_] = buffer[position_ + taps] = sample;
        peak_ = std::max(peak_, std::abs(sample));
        const double* tail = buffer.data() + position_ + 1;
        for (unsigned phase = 0; phase < phaseCount_; ++phase) {
            double value = 0.0;
            for (unsigned i = 0; i < taps; ++i)
                value += tail[i] * interpolator_[phase][i];
            peak_ = std::max(peak_, std::abs(value));
        }
    }
    position_ = (position_ + 1) & (taps - 1);
    if (++tickCount_ == tickSamples_)
        measureWindow();
}

float Loudness::truePeak() const noexcept {
    return static_cast<float>(std::max(-120.0, decibels(peak_)));
}

double Loudness::sumBlocks(unsigned count) const noexcept {
    double sum = 0.0;
    for (unsigned i = 1; i <= count; ++i)
        sum += blocks_[(blockPosition_ + blocks_.size() - i) % blocks_.size()];
    return sum;
}

void Loudness::measureWindow() noexcept {
    blocks_[blockPosition_] = energy_ / tickSamples_;
    blockPosition_ = static_cast<unsigned>((blockPosition_ + 1) % blocks_.size());
    blockCount_ = std::min(blockCount_ + 1, static_cast<unsigned>(blocks_.size()));
    energy_ = 0.0;
    tickCount_ = 0;
    if (blockCount_ == blocks_.size())
        shortTerm_ = lufs(sumBlocks(30) / 30.0);
    if (blockCount_ < 4)
        return;
    const double power = sumBlocks(4) * 0.25;
    momentary_ = lufs(power);
    if (momentary_ < -70.0f)
        return;

    const auto bucket = static_cast<std::size_t>(std::clamp((momentary_ + 70.0) * 10.0,
        0.0, static_cast<double>(bucketCount - 1)));
    auto& bin = histogram_[bucket];
    bin.sum += power;
    ++bin.count;
    gatedEnergy_ += power;
    ++gatedCount_;

    const float relative = lufs(gatedEnergy_ / static_cast<double>(gatedCount_)) - 10.0f;
    const auto first = static_cast<std::size_t>(std::clamp((relative + 70.0) * 10.0,
        0.0, static_cast<double>(bucketCount - 1)));
    double keptEnergy = 0.0;
    std::uint64_t keptCount = 0;
    for (std::size_t i = first; i < histogram_.size(); ++i) {
        keptEnergy += histogram_[i].sum;
        keptCount += histogram_[i].count;
    }
    if (keptCount != 0)
        integrated_ = lufs(keptEnergy / static_cast<double>(keptCount));
}

} // namespace voiceforge
