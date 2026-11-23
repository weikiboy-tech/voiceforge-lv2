#include "dsp/core/processor.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <numbers>

namespace {
using namespace voiceforge;
constexpr double rate = 48000.0;

void require(bool passed, const char* message) {
    if (!passed) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

double signal(double frequency, std::size_t sample, double sampleRate = rate) {
    return std::sin(2.0 * std::numbers::pi * frequency * sample / sampleRate);
}

double eqResponse(unsigned band, float frequency, float gain, double tone) {
    Equalizer eq;
    auto settings = defaultEqSettings();
    settings[band] = {true, frequency, gain, 0.707f};
    eq.prepare(settings, rate, true);
    double sum = 0.0;
    for (std::size_t i = 0; i < 9600; ++i) {
        const double sample = signal(tone, i);
        std::array<double, 2> frame{sample, -sample};
        eq.process(frame, 2, stepForMs(5.0, rate));
        require(frame[1] == -frame[0], "EQ: identical stereo coefficients and independent state");
        if (i >= 4800)
            sum += frame[0] * frame[0];
    }
    return decibels(std::sqrt(sum / 2400.0));
}

void equalizerTest() {
    require(std::abs(eqResponse(0, 200.0f, 9.0f, 30.0) - 9.0) < 0.1, "EQ: low shelf boosts bass");
    require(std::abs(eqResponse(0, 200.0f, 9.0f, 3000.0)) < 0.1, "EQ: low shelf leaves far treble unchanged");
    for (unsigned band = 1; band <= 4; ++band) {
        const float center = eqFrequencies[band];
        require(std::abs(eqResponse(band, center, -12.0f, center) + 12.0) < 0.025,
                "EQ: bell center matches the requested cut");
    }
    require(std::abs(eqResponse(5, 2000.0f, 6.0f, 12000.0) - 6.0) < 0.05,
            "EQ: high shelf boosts the upper band");
    require(std::abs(eqResponse(5, 2000.0f, 6.0f, 100.0)) < 0.05,
            "EQ: high shelf leaves far bass unchanged");
}

void compressorTest() {
    Compressor comp;
    CompressorSettings settings;
    settings.enabled = true;
    settings.thresholdDb = -20.0f;
    settings.ratio = 4.0f;
    settings.kneeDb = 0.0f;
    settings.attackMs = 1.0f;
    settings.releaseMs = 100.0f;
    comp.prepare(settings, rate, true);
    constexpr double expected = -15.0;
    const auto sample = [&](double input) {
        std::array<double, 2> frame{input, input * -0.25};
        const double attenuation = comp.process(frame, 2, stepForMs(5.0, rate));
        require(frame[1] == frame[0] * -0.25, "compressor: linked channels");
        return decibels(attenuation);
    };
    require(sample(1.0) > expected * 0.05, "compressor: attack ramps");
    for (unsigned i = 0; i < 47; ++i)
        sample(1.0);
    require(std::abs(sample(1.0) - expected * (1.0 - std::exp(-49.0 / 48.0))) < 0.001,
            "compressor: attack time constant");
    for (unsigned i = 0; i < 800; ++i)
        sample(1.0);
    require(std::abs(sample(1.0) - expected) < 0.001, "compressor: steady ratio");
    require(sample(0.01) < -14.9, "compressor: no abrupt release");
    for (unsigned i = 0; i < 4800; ++i)
        sample(0.01);
    require(std::abs(sample(0.01) - expected / std::numbers::e) < 0.01,
            "compressor: release time constant");
    for (double edge : {-3.0, 3.0}) {
        require(std::abs(overThreshold(edge - 1e-5, 6.0) - overThreshold(edge + 1e-5, 6.0)) < 3e-5,
                "compressor: soft knee continuity");
    }
    settings.mix = 0.5f;
    settings.makeupDb = 6.0f;
    settings.autoMakeup = true;
    comp = {};
    comp.prepare(settings, rate, true);
    std::array<double, 2> frame{};
    for (unsigned i = 0; i < 1000; ++i) {
        frame = {1.0, 1.0};
        comp.process(frame, 2, stepForMs(5.0, rate));
    }
    require(std::abs(frame[0] - (1.0 + amplitude(-3.0)) * 0.5) < 0.001,
            "compressor: parallel mix and combined manual and automatic makeup");
}

void expanderTest() {
    Expander exp;
    ExpanderSettings settings;
    settings.enabled = true;
    settings.thresholdDb = -40.0f;
    settings.ratio = 2.0f;
    settings.rangeDb = 12.0f;
    settings.attackMs = 1.0f;
    settings.releaseMs = 10.0f;
    exp.prepare(settings, rate, true);
    const auto run = [&](double input) {
        std::array<double, 2> frame{};
        for (unsigned i = 0; i < 7200; ++i) {
            frame = {input, -0.5 * input};
            exp.process(frame, 2, stepForMs(5.0, rate));
        }
        require(frame[1] == -0.5 * frame[0], "expander: linked channels");
        return decibels(frame[0] / input);
    };
    require(std::abs(run(0.1)) < 0.001, "expander: above threshold remains at unity");
    require(std::abs(run(amplitude(-47.0)) + 7.0) < 0.001, "expander: downward ratio");
    require(std::abs(run(0.0001) + 12.0) < 0.001, "expander: maximum attenuation");
}

double deEssResponse(double tone, float mix) {
    DeEsser ess;
    DeEsserSettings settings;
    settings.enabled = true;
    settings.mix = mix;
    ess.prepare(settings, rate, true);
    double in = 0.0, out = 0.0;
    for (std::size_t i = 0; i < 9600; ++i) {
        const double sample = 0.6 * signal(tone, i);
        std::array<double, 2> frame{sample, -0.5 * sample};
        ess.process(frame, 2, stepForMs(5.0, rate));
        require(frame[1] == -0.5 * frame[0], "de-esser: linked channels");
        if (i >= 4800) {
            in += sample * sample;
            out += frame[0] * frame[0];
        }
    }
    return decibels(std::sqrt(out / in));
}

void deEsserTest() {
    require(deEssResponse(6500.0, 1.0f) < -11.0, "de-esser: tames the selected band");
    require(std::abs(deEssResponse(150.0, 1.0f)) < 0.1, "de-esser: preserves bass outside the selected band");
    require(std::abs(deEssResponse(6500.0, 0.0f)) < 0.001, "de-esser: zero mix remains dry");
}

void automationTest(double sampleRate) {
    Processor processor(sampleRate, 2);
    Controls controls;
    std::array<float, 127> input{}, other{}, left{}, right{};
    std::size_t sample = 0;
    for (unsigned block = 0; block < 1024; ++block) {
        const unsigned size = 1 + (block * 13) % input.size();
        const bool toggle = (block / 7) % 2 == 0;
        controls.enabled = block % 31 != 0;
        controls.expander.enabled = true;
        controls.deEsser.enabled = toggle;
        controls.deEsser.frequency = toggle ? 14000.0f : 2000.0f;
        controls.compressor.enabled = !toggle;
        controls.compressor.thresholdDb = toggle ? -60.0f : 0.0f;
        controls.limiterEnabled = true;
        for (std::size_t band = 0; band < controls.equalizer.size(); ++band)
            controls.equalizer[band] = {true, toggle ? 20.0f : 20000.0f, toggle ? 18.0f : -18.0f, 8.0f};
        for (unsigned i = 0; i < size; ++i, ++sample) {
            input[i] = static_cast<float>(0.34 * signal(400.0, sample, sampleRate));
            other[i] = static_cast<float>(0.25 * signal(2900.0, sample, sampleRate));
        }
        if (block == 0)
            input[0] = std::numeric_limits<float>::infinity();
        processor.process({input.data(), other.data()}, {left.data(), right.data()}, size, controls);
        for (unsigned i = 0; i < size; ++i)
            require(std::isfinite(left[i]) && std::isfinite(right[i]) &&
                    std::abs(left[i]) <= 1.0f && std::abs(right[i]) <= 1.0f,
                    "extreme automated values produce bounded stereo output");
        require(std::isfinite(processor.compressorReduction()) &&
                std::isfinite(processor.deEsserReduction()), "finite dynamics meters");
    }
}
} // namespace

int main() {
    equalizerTest();
    compressorTest();
    expanderTest();
    deEsserTest();
    for (double sampleRate : {8000.0, 44100.0, 48000.0, 96000.0, 192000.0})
        automationTest(sampleRate);
}
