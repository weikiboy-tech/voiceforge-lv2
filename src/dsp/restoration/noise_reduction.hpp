#pragma once

#include <array>
#include <complex>

namespace voiceforge {

struct NoiseSettings {
    bool enabled = false;
    float reductionDb = 12.0f;
    float thresholdDb = -32.0f;
    float sensitivity = 0.55f;
    float smoothing = 0.55f;
    float speechProtection = 0.65f;
    float artifactControl = 0.55f;
};

struct RoomSettings {
    bool enabled = false;
    float strength = 0.55f;
    float tail = 0.45f;
    float speechProtection = 0.8f;
};

class NoiseReduction {
public:
    static constexpr unsigned maxSize = 8192;
    explicit NoiseReduction(double rate) noexcept;
    void reset() noexcept;
    void prepare(const NoiseSettings& settings, bool active, bool initial,
                 const RoomSettings& room = {}) noexcept;
    std::array<double, 2> process(const std::array<double, 2>& input, unsigned channels,
                                  bool speech, double smoothing) noexcept;
    unsigned delay() const noexcept { return size_; }

private:
    using Complex = std::complex<double>;
    struct Bin { double power = 0.0, noise = 0.0, tail = 0.0, gain = 1.0; };

    void transform(std::array<Complex, maxSize>& spectrum, bool inverse) const noexcept;
    void analyze(unsigned channels, bool speech) noexcept;

    std::array<std::array<double, 2>, maxSize> history_{}, delta_{};
    std::array<std::array<Complex, maxSize>, 2> spectrum_{};
    std::array<Complex, maxSize / 2> twiddles_{};
    std::array<unsigned, maxSize> reversed_{};
    std::array<double, maxSize> window_{};
    std::array<Bin, maxSize / 2 + 1> bins_{};
    std::array<double, maxSize / 2 + 1> target_{};

    unsigned size_ = 256, position_ = 0, learned_ = 0;
    double hopSeconds_ = 0.0;
    double mix_ = 0.0, targetMix_ = 0.0;
    double noiseMix_ = 0.0, roomMix_ = 0.0, noiseTarget_ = 0.0, roomTarget_ = 0.0;
    double floor_ = 1.0, thresholdPower_ = 0.0, sensitivity_ = 0.0;
    double smoothing_ = 0.0, protection_ = 0.0, artifact_ = 0.0;
    double roomDepth_ = 0.0, roomDecay_ = 0.0, roomProtection_ = 0.0, roomEnvelope_ = 0.0;
};

} // namespace voiceforge
