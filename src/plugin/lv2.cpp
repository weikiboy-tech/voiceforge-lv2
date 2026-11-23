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
    limiterEnabled, limiterCeiling, limiterReduction, controlCount
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
    if (!std::isfinite(rate) || rate < 8000.0)
        return nullptr;
    return new (std::nothrow) Instance(rate, std::strcmp(descriptor->URI, stereoUri) == 0 ? 2u : 1u);
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
    const auto value = [&ports](Control port, float fallback) {
        return ports[port] && std::isfinite(*ports[port]) ? *ports[port] : fallback;
    };
    if (ports[latency])
        *ports[latency] = 0.0f;
    instance.processor.process(instance.inputs, instance.outputs, frames,
        {value(inputGain, 0.0f), value(outputGain, 0.0f), value(enabled, 1.0f) >= 0.5f,
         value(highPass, 0.0f), value(limiterEnabled, 0.0f) >= 0.5f,
         value(limiterCeiling, -1.0f)});
    if (ports[limiterReduction])
        *ports[limiterReduction] = instance.processor.limiterReduction();
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
