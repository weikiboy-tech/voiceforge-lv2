#pragma once

#include <array>
#include <cstdint>

namespace voiceforge {

struct Measurements {
    float inputPeak = -120.0f, inputRms = -120.0f, outputPeak = -120.0f, truePeak = -120.0f;
    float momentary = -120.0f, shortTerm = -120.0f, integrated = -120.0f;
    bool inputClip = false, outputClip = false;
};

class Loudness {
public:
    explicit Loudness(double rate, bool measureTruePeak = true) noexcept;
    void reset() noexcept;
    void process(const std::array<float, 2>& frame, unsigned channels) noexcept;
    float momentary() const noexcept { return momentary_; }
    float shortTerm() const noexcept { return shortTerm_; }
    float integrated() const noexcept { return integrated_; }
    float truePeak() const noexcept;

private:
    struct Coefficients { double b0, b1, b2, a1, a2; };
    struct State {
        double z1 = 0.0, z2 = 0.0;
        double filter(double x, const Coefficients& c) noexcept;
    };
    struct Bucket { double sum = 0.0; std::uint64_t count = 0; };
    static constexpr unsigned taps = 32;
    static constexpr unsigned bucketCount = 1800;
    void measureWindow() noexcept;
    double sumBlocks(unsigned count) const noexcept;

    Coefficients shelf_{}, highPass_{};
    std::array<State, 2> shelfState_{}, highPassState_{};
    std::array<std::array<double, taps>, 3> interpolator_{};
    std::array<std::array<double, taps * 2>, 2> history_{};
    std::array<double, 30> blocks_{};
    std::array<Bucket, bucketCount> histogram_{};
    unsigned phaseCount_ = 3, tickSamples_ = 4800;
    unsigned position_ = 0, blockPosition_ = 0, blockCount_ = 0, tickCount_ = 0;
    double energy_ = 0.0, peak_ = 0.0, gatedEnergy_ = 0.0;
    std::uint64_t gatedCount_ = 0;
    float momentary_ = -120.0f, shortTerm_ = -120.0f, integrated_ = -120.0f;
};

} // namespace voiceforge
