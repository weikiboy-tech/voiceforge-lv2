#pragma once

#include <array>
#include <cstdint>

namespace voiceforge {

struct Controls {
    float inputDb = 0.0f;
    float outputDb = 0.0f;
    bool enabled = true;
    float highPassHz = 0.0f;
};

class Processor {
public:
    Processor(double sampleRate, unsigned channels) noexcept;
    void reset() noexcept;
    void process(const std::array<const float*, 2>& inputs,
                 const std::array<float*, 2>& outputs,
                 std::uint32_t frames, Controls controls) noexcept;

private:
    struct Filter {
        float low = 0.0f;
        float process(float signal, float coefficient) noexcept;
    };

    double sampleRate_;
    unsigned channels_;
    double smoothing_;
    bool primed_ = false;
    double inputGain_ = 1.0;
    double outputGain_ = 1.0;
    double wet_ = 1.0;
    double highPassCoefficient_ = 0.0;
    double highPassMix_ = 0.0;
    std::array<Filter, 2> filters_{};
};

} // namespace voiceforge
