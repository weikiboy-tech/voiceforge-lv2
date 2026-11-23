#include "dsp/core/processor.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <numbers>
#include <random>
#include <vector>

namespace {
using namespace voiceforge;
constexpr double rate = 48000.0;

void require(bool passed, const char* message) {
    if (!passed) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

double tone(double frequency, unsigned sample, double sampleRate = rate) {
    return std::sin(2.0 * std::numbers::pi * frequency * sample / sampleRate);
}

void voiceTest() {
    VoiceActivity vad;
    vad.configure(rate);
    vad.reset();
    const auto send = [&](double frequency, double gain, unsigned frames) {
        for (unsigned i = 0; i < frames; ++i)
            vad.process({0.0, gain * tone(frequency, i)}, 2, 0.5);
    };
    send(1000.0, 0.0, 9600);
    send(55.0, 0.1, 9600);
    require(!vad.active(), "VAD rejects silence and low-frequency noise");
    send(660.0, 0.1, 1440);
    require(vad.active(), "VAD detects activity in one stereo channel");
    send(660.0, 0.0, 4800);
    require(vad.active(), "VAD keeps short intra-word pauses active");
    send(660.0, 0.0, 4800);
    require(!vad.active(), "VAD releases after silence");
}

void latencyTest(double sampleRate) {
    for (bool enabled : {false, true}) {
        Processor processor(sampleRate, 2);
        Controls settings;
        settings.enabled = enabled;
        settings.restoration.studio = true;
        const auto latency = processor.latency(true);
        require(latency > 0 && processor.latency(false) == 0, "mode-specific latency");
        std::vector<float> left(latency + 10), right(left.size()), outLeft(left.size()), outRight(left.size());
        left[0] = 0.5f;
        right[0] = -0.25f;
        for (unsigned offset = 0; offset < left.size();) {
            const auto length = std::min<unsigned>(1 + (offset % 83), static_cast<unsigned>(left.size()) - offset);
            processor.process({left.data() + offset, right.data() + offset},
                {outLeft.data() + offset, outRight.data() + offset}, length, settings);
            offset += length;
        }
        for (unsigned i = 0; i < left.size(); ++i) {
            require(outLeft[i] == (i == latency ? 0.5f : 0.0f) &&
                    outRight[i] == (i == latency ? -0.25f : 0.0f),
                    "measured Studio impulse matches reported latency, including bypass");
        }
    }
}

void clickTest() {
    DeClick click;
    click.configure(rate);
    DeClickSettings settings;
    settings.enabled = true;
    settings.strength = 1.0f;
    click.prepare(settings, rate, true, true);

    for (unsigned i = 0; i < 2000; ++i) {
        const auto clean = [](unsigned n) {
            const double speech = n >= 1000 && n < 1400
                ? 0.20 * tone(2400.0, n) * std::min(1.0, std::min(n - 1000, 1400 - n) / 45.0)
                : 0.0;
            return 0.065 * tone(180.0, n) + speech + (n >= 1500 ? 0.3 : 0.0);
        };
        const std::array<double, 4> clickWave{0.34, -0.22, 0.13, -0.07};
        const double error = i >= 800 && i < 804 ? clickWave[i - 800] : 0.0;
        const auto sample = click.process({clean(i) + error, clean(i)}, 2, stepForMs(5.0, rate));
        if (i < click.delay())
            continue;
        const unsigned source = i - click.delay();
        require(std::abs(sample[0] - clean(source)) < 0.002, "De-Click repairs the glitch and keeps a sustained onset");
        require(sample[1] == clean(source), "De-Click leaves the unaffected channel unchanged");
    }
}

double plosiveTest(double frequency) {
    DePlosive plosive;
    DePlosiveSettings settings;
    settings.enabled = true;
    plosive.prepare(settings, rate, true);
    double input = 0.0, output = 0.0;
    for (unsigned i = 0; i < 5280; ++i) {
        const double envelope = std::pow(std::sin(std::numbers::pi * i / 5280.0), 2);
        const double signal = 0.36 * envelope * tone(frequency, i);
        std::array<double, 2> frame{signal, -signal};
        plosive.process(frame, 2, stepForMs(5.0, rate));
        require(frame[1] == -frame[0], "De-Plosive links the stereo channels");
        input += signal * signal;
        output += frame[0] * frame[0];
    }
    return 10.0 * std::log10(input / output);
}

void noiseTest() {
    auto reduction = std::make_unique<NoiseReduction>(rate);
    NoiseSettings settings;
    settings.enabled = true;
    reduction->prepare(settings, true, true);
    std::minstd_rand rng(17);
    std::uniform_real_distribution<double> noise(-0.005, 0.005);
    double original = 0.0, filtered = 0.0;
    for (unsigned i = 0; i < 48000; ++i) {
        const double sample = noise(rng) + 0.009 * tone(120.0, i);
        const auto output = reduction->process({sample, -sample}, 2, false, stepForMs(5.0, rate));
        require(std::isfinite(output[0]) && output[1] == -output[0], "spectral mask shared by both channels");
        if (i >= 24000) {
            original += sample * sample;
            filtered += output[0] * output[0];
        }
    }
    require(original > 4.0 * filtered, "stationary noise suppression of at least 6 dB");
}

void modeSwitchTest() {
    Processor processor(rate, 1);
    Controls controls;
    controls.restoration.noise.enabled = true;
    controls.restoration.click.enabled = true;
    controls.restoration.plosive.enabled = true;
    float out = 0.0f, previous = 0.0f;
    for (unsigned i = 0; i < 16000; ++i) {
        const float in = static_cast<float>(0.25 * tone(220.0, i));
        controls.restoration.studio = i >= 4000 && i < 8000;
        processor.process({&in, nullptr}, {&out, nullptr}, 1, controls);
        require(std::isfinite(out) && std::abs(out - previous) < 0.02f, "smooth mode switching");
        previous = out;
    }
}
} // namespace

int main() {
    voiceTest();
    clickTest();
    require(plosiveTest(52.0) > 2.5, "De-Plosive attenuates a low-frequency burst");
    require(std::abs(plosiveTest(700.0)) < 0.1, "De-Plosive protects upper speech frequencies");
    noiseTest();
    modeSwitchTest();
    for (double sampleRate : {8000.0, 44100.0, 48000.0, 96000.0, 192000.0, 384000.0})
        latencyTest(sampleRate);
}
