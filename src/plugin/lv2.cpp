#include "dsp/core/processor.hpp"
#include "common/compare_state.hpp"

#include <lv2/atom/util.h>
#include <lv2/core/lv2.h>
#include <lv2/state/state.h>

#include <array>
#include <atomic>
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
    sampleRate, spectrumStart, phaseInvert = spectrumStart + voiceforge::Spectrum::frequencies.size() + 2,
    dcRemoval, roomEnabled, roomStrength, roomTail, roomSpeech, controlCount
};
static_assert(phaseInvert == voiceforge::compareOutputOffset + 1);
static_assert(controlCount <= voiceforge::compareCapacity && std::atomic<float>::is_always_lock_free);

struct Instance {
    Instance(double rate, unsigned channelCount, const LV2_URID_Map* map)
        : processor(rate, channelCount), channels(channelCount), rate(rate), ids(map) {}

    voiceforge::Processor processor;
    unsigned channels;
    double rate;
    std::array<const float*, 2> inputs{};
    std::array<float*, 2> outputs{};
    std::array<float*, controlCount> controls{};
    voiceforge::CompareUrids ids;
    const LV2_Atom_Sequence* compareIn = nullptr;
    LV2_Atom_Sequence* compareOut = nullptr;
    std::atomic<unsigned> compareRevision{};
    std::array<std::atomic<float>, voiceforge::CompareState{}.values.size()> comparison{};
    bool reply = false;

    void update(const voiceforge::CompareState& state) noexcept {
        compareRevision.fetch_add(1);
        for (unsigned i = 0; i < comparison.size(); ++i)
            comparison[i].store(std::isfinite(state.values[i]) ? state.values[i] : 0.0f);
        compareRevision.fetch_add(1);
    }

    bool copy(voiceforge::CompareState& state) const noexcept {
        state.vector.child_type = ids.floating;
        for (unsigned retry = 0; retry < 32; ++retry) {
            const auto before = compareRevision.load();
            if (before % 2 != 0)
                continue;
            for (unsigned i = 0; i < comparison.size(); ++i)
                state.values[i] = comparison[i].load();
            if (before == compareRevision.load())
                return true;
        }
        return false;
    }

    void exchange(unsigned frames) noexcept {
        if (ids.sequence == 0)
            return;
        if (compareIn && compareIn->atom.type == ids.sequence) {
            LV2_ATOM_SEQUENCE_FOREACH(compareIn, event) {
                if (event->body.type == ids.boolean)
                    reply = true;
                else if (event->body.type == ids.vector && event->body.size == sizeof(voiceforge::CompareState)) {
                    voiceforge::CompareState state;
                    std::memcpy(&state, LV2_ATOM_BODY_CONST(&event->body), sizeof(state));
                    if (state.vector.child_type == ids.floating && state.vector.child_size == sizeof(float))
                        update(state);
                }
            }
        }
        if (!compareOut)
            return;
        const unsigned capacity = compareOut->atom.size;
        if (capacity < sizeof(LV2_Atom_Sequence_Body))
            return;
        compareOut->atom.type = ids.sequence;
        compareOut->body.unit = compareOut->body.pad = 0;
        lv2_atom_sequence_clear(compareOut);
        if (frames && reply) {
            struct { LV2_Atom_Event event; voiceforge::CompareState state; } message{};
            message.event.body = {sizeof(message.state), ids.vector};
            if (copy(message.state) && lv2_atom_sequence_append_event(compareOut, capacity, &message.event))
                reply = false;
        }
    }
};

LV2_Handle instantiate(const LV2_Descriptor* descriptor, double rate, const char*,
                       const LV2_Feature* const* features) {
    if (!std::isfinite(rate) || rate < 8000.0 || rate > 384000.0)
        return nullptr;
    try {
        const LV2_URID_Map* map = nullptr;
        for (auto f = features; f && *f; ++f)
            if (std::strcmp((*f)->URI, LV2_URID__map) == 0)
                map = static_cast<const LV2_URID_Map*>((*f)->data);
        return new Instance(rate, std::strcmp(descriptor->URI, stereoUri) == 0 ? 2u : 1u, map);
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
    else if (port == 2 * channels + voiceforge::compareInputOffset)
        instance.compareIn = static_cast<const LV2_Atom_Sequence*>(data);
    else if (port == 2 * channels + voiceforge::compareOutputOffset)
        instance.compareOut = static_cast<LV2_Atom_Sequence*>(data);
    else if (port < 2 * channels + controlCount)
        instance.controls[port - 2 * channels] = static_cast<float*>(data);
}

void activate(LV2_Handle handle) {
    static_cast<Instance*>(handle)->processor.reset();
}

void run(LV2_Handle handle, uint32_t frames) {
    auto& instance = *static_cast<Instance*>(handle);
    instance.exchange(frames);
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
    auto& room = restoration.room;
    room = {value(roomEnabled, 0.0f) >= 0.5f, value(roomStrength, room.strength),
            value(roomTail, room.tail), value(roomSpeech, room.speechProtection)};
    settings.phaseInvert = value(phaseInvert, 0.0f) >= 0.5f;
    settings.dcRemoval = value(dcRemoval, 0.0f) >= 0.5f;
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

LV2_State_Status save(LV2_Handle handle, LV2_State_Store_Function store, LV2_State_Handle state,
                      uint32_t, const LV2_Feature* const*) {
    const auto& plugin = *static_cast<Instance*>(handle);
    if (!plugin.ids.property)
        return LV2_STATE_SUCCESS;
    voiceforge::CompareState snapshot;
    if (!plugin.copy(snapshot))
        return LV2_STATE_ERR_UNKNOWN;
    if (snapshot.values[1] == 0.0f && snapshot.values[2] == 0.0f)
        return LV2_STATE_SUCCESS;
    return store(state, plugin.ids.property, &snapshot, sizeof(snapshot), plugin.ids.vector,
                 LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);
}

LV2_State_Status restore(LV2_Handle handle, LV2_State_Retrieve_Function retrieve, LV2_State_Handle state,
                         uint32_t, const LV2_Feature* const*) {
    auto& plugin = *static_cast<Instance*>(handle);
    if (!plugin.ids.property)
        return LV2_STATE_SUCCESS;
    size_t size = 0;
    uint32_t type = 0, flags = 0;
    const void* data = retrieve(state, plugin.ids.property, &size, &type, &flags);
    voiceforge::CompareState snapshot;
    if (data && type == plugin.ids.vector && size == sizeof(snapshot))
        std::memcpy(&snapshot, data, sizeof(snapshot));
    if (snapshot.vector.child_size != sizeof(float) || snapshot.vector.child_type != plugin.ids.floating)
        snapshot = {};
    plugin.update(snapshot);
    return LV2_STATE_SUCCESS;
}

const void* extension(const char* uri) {
    static const LV2_State_Interface state{save, restore};
    return std::strcmp(uri, LV2_STATE__interface) == 0 ? &state : nullptr;
}

constexpr LV2_Descriptor descriptors[] = {
    {monoUri, instantiate, connectPort, activate, run, nullptr, cleanup, extension},
    {stereoUri, instantiate, connectPort, activate, run, nullptr, cleanup, extension}
};
} // namespace

LV2_SYMBOL_EXPORT
const LV2_Descriptor* lv2_descriptor(uint32_t index) {
    return index < 2 ? &descriptors[index] : nullptr;
}
