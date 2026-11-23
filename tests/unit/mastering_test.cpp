#include "dsp/core/processor.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <numbers>

using namespace voiceforge;
namespace {
void require(bool valid, const char* message) {
    if (!valid) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

double tone(double peak, double frequency, unsigned i, double rate) {
    return peak * std::sin(2.0 * std::numbers::pi * frequency * i / rate);
}

void meterTest(unsigned rate) {
    Loudness meter(rate);
    const auto signal = [&](double db, unsigned frames) {
        for (unsigned i = 0; i < frames; ++i) {
            const float x = static_cast<float>(tone(amplitude(db), 1000.0, i, rate));
            meter.process({x, x}, 2);
        }
    };
    signal(-23.0, rate / 5);
    require(meter.momentary() == -120 && meter.shortTerm() == -120 && meter.integrated() == -120,
            "meter waits for complete windows");
    signal(-23.0, rate * 4);
    require(std::abs(meter.momentary() + 23.0) < 0.18 &&
            std::abs(meter.shortTerm() + 23.0) < 0.18 &&
            std::abs(meter.integrated() + 23.0) < 0.18, "stereo loudness reference");
    signal(-52.0, rate * 5);
    require(std::abs(meter.integrated() + 23.0) < 1.0 &&
            std::abs(meter.shortTerm() + 52.0) < 0.18, "relative gate excludes quiet sound");
    meter.reset();
    require(meter.integrated() == -120 && meter.truePeak() == -120, "clear integrated and true peak");
}

void truePeakTest() {
    Loudness meter(48000.0);
    constexpr double peak = 0.9;
    for (unsigned i = 0; i < 400; ++i) {
        const double fade = std::clamp(i / 100.0, 0.0, 1.0);
        const float x = static_cast<float>(peak * fade *
            std::sin(std::numbers::pi * i / 2 + std::numbers::pi / 4));
        meter.process({x, 0.0f}, 1);
    }
    require(std::abs(meter.truePeak() - decibels(peak)) < 0.12,
            "true peak recovers the peak between source samples");
}

void riderTest() {
    LevelRider rider;
    RiderSettings settings;
    settings.enabled = true;
    settings.speedMs = 180.0f;
    rider.prepare(settings, 8000.0, true);
    const auto send = [&](double peak, bool voice) {
        for (unsigned i = 0; i < 8000; ++i) {
            const double x = tone(peak, 440.0, i, 8000.0);
            std::array<double, 2> frame{x, -x};
            rider.process(frame, 2, 0.05, voice);
            require(frame[0] == -frame[1], "linked level riding");
        }
        return rider.appliedDb();
    };
    require(send(0.024, true) > 8.5f, "raises quieter speech within the boost limit");
    require(send(0.55, true) < -11.4f, "reduces louder speech within the attenuation limit");
    require(std::abs(send(0.007, false)) < 0.1, "pauses return to unity without raising ambient noise");
}

void analysisTest() {
    VoiceAnalysis analysis(8000.0);
    analysis.command(true, false);
    for (unsigned i = 0; i < 64000; ++i) {
        const bool voice = i >= 8000;
        const double x = tone(voice ? 0.2 : 0.001, voice ? 600.0 : 120.0, i, 8000.0);
        analysis.process({x, 0.0}, 1, voice);
    }
    const auto& report = analysis.report();
    require(report.progress == 1.0f && report.speech == 0.875f &&
            report.floor < -60 && report.highPass != 0 && report.loudness > -30 &&
            report.rms < report.peak, "completed finite voice report based on audio");
    analysis.command(false, true);
    require(report.progress == 0.0f && report.loudness == -120.0f, "analysis resets");
    analysis.command(true, false);
    for (unsigned i = 0; i < 16000; ++i)
        analysis.process({tone(0.10, 600.0, i, 8000.0), 0.0}, 1, true);
    analysis.command(false, false);
    analysis.command(true, false);
    require(report.progress == 1.0f && report.loudness > -30, "stop reports the audio captured so far");

    Spectrum spectrum(48000.0);
    for (unsigned i = 0; i < 24000; ++i)
        spectrum.process({tone(0.2, 1000.0, i, 48000.0), 0.0}, 1);
    require(spectrum.level(6) > spectrum.level(0) + 15 &&
            spectrum.level(6) > spectrum.level(11) + 15, "metered spectrum tracks actual input frequencies");
}

void inputStageTest() {
    Processor processor(48000.0, 1);
    Controls controls;
    controls.phaseInvert = true;
    const float input = 0.25f;
    float output = 0.0f;
    processor.process({&input,nullptr},{&output,nullptr},1,controls);
    require(output == -input, "polarity operates at unity");
    controls.phaseInvert = false;
    processor.process({&input,nullptr},{&output,nullptr},1,controls);
    require(output < -0.24f && output > -0.25f, "polarity automation fades smoothly");
    processor.reset();
    controls.dcRemoval = true;
    for (unsigned i = 0; i < 24000; ++i)
        processor.process({&input,nullptr},{&output,nullptr},1,controls);
    require(std::abs(output) < 0.0001f, "DC blocker removes constant offset");
    processor.reset();
    double source = 0.0, actual = 0.0;
    for (unsigned i = 0; i < 9600; ++i) {
        const float sample = static_cast<float>(tone(0.2, 100.0, i, 48000.0));
        processor.process({&sample,nullptr},{&output,nullptr},1,controls);
        if (i > 4800) {
            source += sample * sample;
            actual += output * output;
        }
    }
    require(std::abs(10.0 * std::log10(actual / source)) < 0.08, "DC blocker preserves bass");
}
} // namespace

int main() {
    for (unsigned rate : {44100, 48000, 96000})
        meterTest(rate);
    truePeakTest();
    riderTest();
    analysisTest();
    inputStageTest();
}
