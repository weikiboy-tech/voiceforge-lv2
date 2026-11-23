#include "dsp/core/processor.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <numbers>

using voiceforge::Controls;
using voiceforge::Processor;

void require(bool ok, const char* message) {
    if (!ok) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

void test(double rate) {
    std::array<float, 16384> left{}, right{}, outLeft{}, outRight{};
    left.fill(0.25f);
    right.fill(-0.5f);
    const std::array<const float*, 2> inputs{left.data(), right.data()};
    const std::array<float*, 2> outputs{outLeft.data(), outRight.data()};

    Processor processor(rate, 2);
    processor.process(inputs, outputs, 1, {});
    require(outLeft[0] == left[0] && outRight[0] == right[0], "unity pass-through");

    processor.process(inputs, outputs, 1, {6.0f, 6.0f});
    const double firstGain = 1.0 - std::expm1(-1.0 / (0.005 * rate)) *
        (std::pow(10.0, 6.0 / 20.0) - 1.0);
    require(std::abs(outLeft[0] - 0.25 * firstGain * firstGain) < 1e-6,
            "gain changes follow the configured smoothing time");
    processor.process(inputs, outputs, left.size(), {6.0f, 6.0f});
    require(std::abs(outLeft.back() - 0.25 * std::pow(10.0, 12.0 / 20.0)) < 1e-5,
            "both gains settle at the expected value");
    require(outRight.back() == -2.0f * outLeft.back(), "stereo gain is identical");

    processor.process(inputs, outputs, 1, {6.0f, 6.0f, false});
    require(outLeft[0] > 0.95f, "bypass fades instead of switching abruptly");
    processor.process(inputs, outputs, left.size(), {6.0f, 6.0f, false});
    require(outLeft.back() == left.back() && outRight.back() == right.back(), "dry bypass");

    Processor mono(rate, 1);
    mono.process({left.data(), nullptr}, {left.data(), nullptr}, 0, {24.0f, 24.0f});
    mono.process({left.data(), nullptr}, {left.data(), nullptr}, 1, {});
    require(left[0] == 0.25f, "zero-frame run cannot prime a mono instance");

    mono.reset();
    left[0] = std::numeric_limits<float>::infinity();
    left[1] = std::numeric_limits<float>::quiet_NaN();
    left[2] = std::numeric_limits<float>::max();
    mono.process(inputs, outputs, 3, {24.0f, 24.0f, true, 300.0f});
    require(outLeft[0] == 0.0f && outLeft[1] == 0.0f && std::isfinite(outLeft[2]),
            "non-finite input and extreme levels");

    for (auto frequency : {20.0, 1000.0}) {
        mono.reset();
        for (std::size_t i = 0; i < left.size(); ++i)
            left[i] = static_cast<float>(std::sin(2.0 * std::numbers::pi * frequency * i / rate));
        mono.process(inputs, outputs, left.size(), {0.0f, 0.0f, true, 120.0f});
        double before = 0.0, after = 0.0;
        for (auto i = left.size() / 2; i < left.size(); ++i) {
            before += left[i] * left[i];
            after += outLeft[i] * outLeft[i];
        }
        const double ratio = std::sqrt(after / before);
        require(frequency == 20.0 ? ratio < 0.2 : ratio > 0.94, "high-pass frequency response");
    }

    mono.reset();
    left.fill(0.0f);
    for (unsigned block : {3, 1, 47, 2048}) {
        mono.process(inputs, outputs, block, {-24.0f, 24.0f, block != 47, 300.0f});
        for (unsigned i = 0; i < block; ++i)
            require(outLeft[i] == 0.0f, "silence with changing controls and blocks");
    }
}

int main() {
    for (double rate : {8000.0, 44100.0, 48000.0, 96000.0, 192000.0})
        test(rate);
}
