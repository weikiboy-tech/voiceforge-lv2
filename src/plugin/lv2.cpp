#include "dsp/core/processor.hpp"

#include <lv2/core/lv2.h>

#include <array>
#include <cmath>
#include <cstring>
#include <new>

namespace {
constexpr char monoUri[] = "https://weikiboy-tech.github.io/voiceforge-lv2/mono";
constexpr char stereoUri[] = "https://weikiboy-tech.github.io/voiceforge-lv2/stereo";

enum Control : unsigned {
    inputGain, outputGain, enabled, latency, highPass,
    limiterEnabled, limiterCeiling, limiterReduction,
    eqStart, compEnabled = eqStart + 4 * voiceforge::eqFrequencies.size(),
    compThreshold, compRatio, compAttack, compRelease, compKnee, compMakeup,
    compMix, compAutoMakeup, compReduction,
    expEnabled, expThreshold, expRatio, expAttack, expRelease, expRange,
    deEnabled, deFrequency, deThreshold, deRatio, deAttack, deRelease, deMix,
    deReduction, mode, clickEnabled, clickSensitivity, clickStrength, clickDuration,
    plosiveEnabled, plosiveSensitivity, plosiveFrequency, plosiveReduction, plosiveRecovery,
    noiseEnabled, noiseReduction, noiseThreshold, noiseSensitivity, noiseSmoothing,
    noiseSpeechProtection, noiseArtifactControl, vadSensitivity, voiceActive, controlCount
};

struct Instance {
    Instance(double rate, unsigned channelCount)
        : processor(rate, channelCount), channels(channelCount) {}

    voiceforge::Processor processor;
    unsigned channels;
    std::array<const float*, 2> inputs{};
    std::array<float*, 2> outputs{};
    std::array<float*, controlCount> controls{};
};

LV2_Handle instantiate(const LV2_Descriptor* descriptor, double rate, const char*,
                       const LV2_Feature* const*) {
    if (!std::isfinite(rate) || rate < 8000.0 || rate > 384000.0)
        return nullptr;
    try {
        return new Instance(rate, std::strcmp(descriptor->URI, stereoUri) == 0 ? 2u : 1u);
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}

void connectPort(LV2_Handle handle, uint32_t port, void* data) {
    auto& instance = *static_cast<Instance*>(handle);
    const unsigned channels = instance.channels;
    if (port < channels)
        instance.inputs[port] = static_cast<const float*>(data);
    else if (port < 2 * channels)
        instance.outputs[port - channels] = static_cast<float*>(data);
    else if (port < 2 * channels + controlCount)
        instance.controls[port - 2 * channels] = static_cast<float*>(data);
}

void activate(LV2_Handle handle) {
    static_cast<Instance*>(handle)->processor.reset();
}

void run(LV2_Handle handle, uint32_t frames) {
    auto& instance = *static_cast<Instance*>(handle);
    const auto& ports = instance.controls;
    const auto value = [&ports](std::size_t port, float fallback) {
        return ports[port] && std::isfinite(*ports[port]) ? *ports[port] : fallback;
    };
    const auto write = [&ports](Control port, float measured) {
        if (ports[port])
            *ports[port] = measured;
    };
    voiceforge::Controls settings{};
    settings.inputDb = value(inputGain, settings.inputDb);
    settings.outputDb = value(outputGain, settings.outputDb);
    settings.enabled = value(enabled, 1.0f) >= 0.5f;
    settings.highPassHz = value(highPass, settings.highPassHz);
    settings.limiterEnabled = value(limiterEnabled, 0.0f) >= 0.5f;
    settings.limiterCeilingDb = value(limiterCeiling, settings.limiterCeilingDb);
    for (std::size_t i = 0; i < settings.equalizer.size(); ++i) {
        const auto port = eqStart + 4 * i;
        auto& band = settings.equalizer[i];
        band = {value(port, 0.0f) >= 0.5f, value(port + 1, band.frequency),
                value(port + 2, band.gainDb), value(port + 3, band.q)};
    }
    auto& comp = settings.compressor;
    comp = {value(compEnabled, 0.0f) >= 0.5f, value(compThreshold, comp.thresholdDb),
            value(compRatio, comp.ratio), value(compAttack, comp.attackMs),
            value(compRelease, comp.releaseMs), value(compKnee, comp.kneeDb),
            value(compMakeup, comp.makeupDb), value(compMix, comp.mix),
            value(compAutoMakeup, 0.0f) >= 0.5f};
    auto& expander = settings.expander;
    expander = {value(expEnabled, 0.0f) >= 0.5f, value(expThreshold, expander.thresholdDb),
                value(expRatio, expander.ratio), value(expAttack, expander.attackMs),
                value(expRelease, expander.releaseMs), value(expRange, expander.rangeDb)};
    auto& ess = settings.deEsser;
    ess = {value(deEnabled, 0.0f) >= 0.5f, value(deFrequency, ess.frequency),
           value(deThreshold, ess.thresholdDb), value(deRatio, ess.ratio),
           value(deAttack, ess.attackMs), value(deRelease, ess.releaseMs),
           value(deMix, ess.mix)};
    auto& restoration = settings.restoration;
    restoration.studio = value(mode, 0.0f) >= 0.5f;
    auto& click = restoration.click;
    click = {value(clickEnabled, 0.0f) >= 0.5f, value(clickSensitivity, click.sensitivity),
             value(clickStrength, click.strength), value(clickDuration, click.maximumMs)};
    auto& plosive = restoration.plosive;
    plosive = {value(plosiveEnabled, 0.0f) >= 0.5f, value(plosiveSensitivity, plosive.sensitivity),
               value(plosiveFrequency, plosive.frequency), value(plosiveReduction, plosive.reductionDb),
               value(plosiveRecovery, plosive.recoveryMs)};
    auto& noise = restoration.noise;
    noise = {value(noiseEnabled, 0.0f) >= 0.5f, value(noiseReduction, noise.reductionDb),
             value(noiseThreshold, noise.thresholdDb), value(noiseSensitivity, noise.sensitivity),
             value(noiseSmoothing, noise.smoothing), value(noiseSpeechProtection, noise.speechProtection),
             value(noiseArtifactControl, noise.artifactControl)};
    restoration.vadSensitivity = value(vadSensitivity, restoration.vadSensitivity);
    instance.processor.process(instance.inputs, instance.outputs, frames, settings);
    write(latency, static_cast<float>(instance.processor.latency(restoration.studio)));
    write(limiterReduction, instance.processor.limiterReduction());
    write(compReduction, instance.processor.compressorReduction());
    write(deReduction, instance.processor.deEsserReduction());
    write(voiceActive, instance.processor.voiceActive() ? 1.0f : 0.0f);
}

void cleanup(LV2_Handle handle) { delete static_cast<Instance*>(handle); }

constexpr LV2_Descriptor descriptors[] = {
    {monoUri, instantiate, connectPort, activate, run, nullptr, cleanup, nullptr},
    {stereoUri, instantiate, connectPort, activate, run, nullptr, cleanup, nullptr}
};
} // namespace

LV2_SYMBOL_EXPORT
const LV2_Descriptor* lv2_descriptor(uint32_t index) {
    return index < 2 ? &descriptors[index] : nullptr;
}
