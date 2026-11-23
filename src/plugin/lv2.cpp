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
    noiseSpeechProtection, noiseArtifactControl, vadSensitivity, voiceActive,
    riderOn, riderTarget, riderSpeed, riderMaxGain, riderMaxReduction, riderVoice, riderGain,
    meterReset, inPeak, inRms, outPeak, truePeak, lufsM, lufsS, lufsI, inClip, outClip,
    analyze, analysisReset, analysisProgress, analysisPeak, analysisRms, analysisFloor,
    analysisRange, analysisSpeech, analysisLow, analysisHigh, analysisSibilance,
    analysisClip, analysisLoudness, recommendHpf, recommendComp, recommendEss, recommendNoise,
    sampleRate, spectrumStart, controlCount = spectrumStart + voiceforge::Spectrum::frequencies.size()
};

struct Instance {
    Instance(double rate, unsigned channelCount)
        : processor(rate, channelCount), channels(channelCount), rate(rate) {}

    voiceforge::Processor processor;
    unsigned channels;
    double rate;
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
    auto& rider = settings.rider;
    rider = {value(riderOn, 0.0f) >= 0.5f, value(riderTarget, rider.targetDb),
             value(riderSpeed, rider.speedMs), value(riderMaxGain, rider.maximumGainDb),
             value(riderMaxReduction, rider.maximumReductionDb), value(riderVoice, rider.voiceInfluence)};
    settings.resetMeter = value(meterReset, 0.0f) >= 0.5f;
    settings.analyze = value(analyze, 0.0f) >= 0.5f;
    settings.resetAnalysis = value(analysisReset, 0.0f) >= 0.5f;
    settings.spectrum = ports[spectrumStart] != nullptr;
    instance.processor.process(instance.inputs, instance.outputs, frames, settings);
    write(latency, static_cast<float>(instance.processor.latency(restoration.studio)));
    write(limiterReduction, instance.processor.limiterReduction());
    write(compReduction, instance.processor.compressorReduction());
    write(deReduction, instance.processor.deEsserReduction());
    write(voiceActive, instance.processor.voiceActive() ? 1.0f : 0.0f);
    write(riderGain, instance.processor.riderGain());
    const auto& meter = instance.processor.measurements();
    write(inPeak, meter.inputPeak);
    write(inRms, meter.inputRms);
    write(outPeak, meter.outputPeak);
    write(truePeak, meter.truePeak);
    write(lufsM, meter.momentary);
    write(lufsS, meter.shortTerm);
    write(lufsI, meter.integrated);
    write(inClip, meter.inputClip ? 1.0f : 0.0f);
    write(outClip, meter.outputClip ? 1.0f : 0.0f);
    const auto& report = instance.processor.analysis();
    write(analysisProgress, report.progress);
    write(analysisPeak, report.peak);
    write(analysisRms, report.rms);
    write(analysisFloor, report.floor);
    write(analysisRange, report.range);
    write(analysisSpeech, report.speech);
    write(analysisLow, report.low);
    write(analysisHigh, report.high);
    write(analysisSibilance, report.sibilance);
    write(analysisClip, report.clipped ? 1.0f : 0.0f);
    write(analysisLoudness, report.loudness);
    write(recommendHpf, report.highPass);
    write(recommendComp, report.compressor);
    write(recommendEss, report.deEsser);
    write(recommendNoise, report.noise);
    write(sampleRate, static_cast<float>(instance.rate));
    if (settings.spectrum)
        for (unsigned i = 0; i < voiceforge::Spectrum::frequencies.size(); ++i)
            write(static_cast<Control>(spectrumStart + i), instance.processor.spectrumLevel(i));
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
