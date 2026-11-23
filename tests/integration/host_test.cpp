#include <lilv/lilv.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/urid/urid.h>
#include "common/compare_state.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {
struct Mapping {
    std::vector<std::string> uris;

    static LV2_URID map(LV2_URID_Map_Handle handle, const char* uri) {
        auto& self = *static_cast<Mapping*>(handle);
        for (std::size_t i = 0; i < self.uris.size(); ++i)
            if (self.uris[i] == uri)
                return static_cast<LV2_URID>(i + 1);
        self.uris.emplace_back(uri);
        return static_cast<LV2_URID>(self.uris.size());
    }
    static const char* unmap(LV2_URID_Unmap_Handle handle, LV2_URID id) {
        const auto& self = *static_cast<Mapping*>(handle);
        return id > 0 && id <= self.uris.size() ? self.uris[id - 1].c_str() : nullptr;
    }
};

struct Values {
    LV2_URID type;
    float inputGain;

    static const void* read(const char* symbol, void* data, uint32_t* size, uint32_t* type) {
        auto& self = *static_cast<Values*>(data);
        if (std::strcmp(symbol, "input_gain") != 0)
            return nullptr;
        *size = sizeof(float);
        *type = self.type;
        return &self.inputGain;
    }

    static void write(const char* symbol, void* data, const void* value,
                      uint32_t size, uint32_t type) {
        auto& self = *static_cast<Values*>(data);
        if (std::strcmp(symbol, "input_gain") == 0 && type == self.type && size == sizeof(float))
            std::memcpy(&self.inputGain, value, size);
    }
};

void require(bool ok, const char* message) {
    if (!ok) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

void test(LilvWorld* world, const char* uri, unsigned channels) {
    std::unique_ptr<LilvNode, decltype(&lilv_node_free)> id(lilv_new_uri(world, uri), lilv_node_free);
    const auto* plugin = lilv_plugins_get_by_uri(lilv_world_get_all_plugins(world), id.get());
    require(plugin && lilv_plugin_verify(plugin), "discover valid plugin with Lilv");
    require(lilv_plugin_get_num_ports(plugin) == 2 * channels + 124, "metadata port count");
    require(lilv_plugin_has_latency(plugin) &&
            lilv_plugin_get_latency_port_index(plugin) == 2 * channels + 3, "latency designation");

    Mapping mapping;
    LV2_URID_Map map{&mapping, Mapping::map};
    LV2_URID_Unmap unmap{&mapping, Mapping::unmap};
    const LV2_Feature mapFeature{LV2_URID__map, &map};
    const LV2_Feature* features[]{&mapFeature, nullptr};
    const voiceforge::CompareUrids compare(&map);
    Values values{Mapping::map(&mapping, LV2_ATOM__Float), 6.0f};
    std::unique_ptr<LilvInstance, decltype(&lilv_instance_free)> instance(
        lilv_plugin_instantiate(plugin, 48000.0, features), lilv_instance_free);
    require(instance != nullptr, "load and instantiate plugin");

    std::unique_ptr<LilvState, decltype(&lilv_state_free)> state(
        lilv_state_new_from_instance(plugin, instance.get(), &map, nullptr, nullptr, nullptr,
            nullptr, Values::read, &values, 0, nullptr), lilv_state_free);
    require(state != nullptr, "save host control ports");
    values.inputGain = 0.0f;
    lilv_state_restore(state.get(), instance.get(), Values::write, &values, 0, nullptr);
    require(values.inputGain == 6.0f, "restore saved control port through Lilv");

    std::array<float, 2> in{0.25f, -0.125f}, out{};
    float outputGain = 0.0f, enabled = 1.0f, latency = -1.0f, highPass = 0.0f;
    float limiterEnabled = 0.0f, limiterCeiling = -1.0f, limiterReduction = 0.0f;
    for (unsigned ch = 0; ch < channels; ++ch) {
        lilv_instance_connect_port(instance.get(), ch, &in[ch]);
        lilv_instance_connect_port(instance.get(), channels + ch, &out[ch]);
    }
    lilv_instance_connect_port(instance.get(), 2 * channels, &values.inputGain);
    lilv_instance_connect_port(instance.get(), 2 * channels + 1, &outputGain);
    lilv_instance_connect_port(instance.get(), 2 * channels + 2, &enabled);
    lilv_instance_connect_port(instance.get(), 2 * channels + 3, &latency);
    lilv_instance_connect_port(instance.get(), 2 * channels + 4, &highPass);
    lilv_instance_connect_port(instance.get(), 2 * channels + 5, &limiterEnabled);
    lilv_instance_connect_port(instance.get(), 2 * channels + 6, &limiterCeiling);
    lilv_instance_connect_port(instance.get(), 2 * channels + 7, &limiterReduction);
    lilv_instance_activate(instance.get());
    lilv_instance_run(instance.get(), 0);
    require(latency == 0.0f, "report zero samples on a zero-frame run");
    lilv_instance_run(instance.get(), 1);
    for (unsigned ch = 0; ch < channels; ++ch)
        require(std::abs(out[ch] - in[ch] * std::pow(10.0, 6.0 / 20.0)) < 1e-6,
                "restored gain applies on the first audio frame");
    enabled = 0.0f;
    lilv_instance_deactivate(instance.get());
    lilv_instance_activate(instance.get());
    lilv_instance_run(instance.get(), 1);
    require(out[0] == in[0] && (channels == 1 || out[1] == in[1]), "initial bypass");
    lilv_instance_deactivate(instance.get());
    enabled = limiterEnabled = 1.0f;
    limiterCeiling = -12.0f;
    lilv_instance_activate(instance.get());
    lilv_instance_run(instance.get(), 1);
    require(std::abs(out[0] - std::pow(10.0, -12.0 / 20.0)) < 1e-6 &&
            limiterReduction > 5.0f, "limiter control and reduction meter");
    lilv_instance_deactivate(instance.get());

    const auto connect = [&](const char* symbol, float& storage) {
        std::unique_ptr<LilvNode, decltype(&lilv_node_free)> name(
            lilv_new_string(world, symbol), lilv_node_free);
        const auto* port = lilv_plugin_get_port_by_symbol(plugin, name.get());
        require(port != nullptr, symbol);
        lilv_instance_connect_port(instance.get(), lilv_port_get_index(plugin, port), &storage);
    };
    float compressorOn = 1.0f, threshold = -20.0f, ratio = 4.0f, attack = 0.1f;
    float compressorReduction = 0.0f, deEsserReduction = -1.0f;
    connect("comp_enabled", compressorOn);
    connect("comp_threshold", threshold);
    connect("comp_ratio", ratio);
    connect("comp_attack", attack);
    connect("comp_reduction", compressorReduction);
    connect("de_esser_reduction", deEsserReduction);
    values.inputGain = limiterEnabled = 0.0f;
    in = {1.0f, -0.5f};
    lilv_instance_activate(instance.get());
    for (unsigned i = 0; i < 250; ++i)
        lilv_instance_run(instance.get(), 1);
    require(std::abs(out[0] - std::pow(10.0, -15.0 / 20.0)) < 1e-5 &&
            std::abs(compressorReduction - 15.0f) < 0.001f, "compressor control and reduction meter");
    require(deEsserReduction == 0.0f, "disabled de-esser reports no reduction");
    lilv_instance_deactivate(instance.get());

    float mode = 1.0f, active = -1.0f;
    connect("mode", mode);
    connect("voice_active", active);
    compressorOn = 0.0f;
    enabled = 0.0f;
    in = {0.6f, -0.25f};
    lilv_instance_activate(instance.get());
    lilv_instance_run(instance.get(), 0);
    require(latency == 1058.0f, "Studio latency available before first audio sample");
    lilv_instance_run(instance.get(), 1);
    require(out[0] == 0.0f && active == 0.0f, "Studio bypass delays dry audio, VAD starts idle");
    in = {};
    for (unsigned i = 1; i < 1058; ++i) {
        lilv_instance_run(instance.get(), 1);
        require(out[0] == 0.0f, "no audio before the Studio delay");
    }
    lilv_instance_run(instance.get(), 1);
    require(out[0] == 0.6f && (channels == 1 || out[1] == -0.25f), "Studio dry delay matches report");
    lilv_instance_deactivate(instance.get());

    alignas(8) std::array<unsigned char, 2048> inBytes{}, outBytes{};
    auto* compareIn = reinterpret_cast<LV2_Atom_Sequence*>(inBytes.data());
    auto* compareOut = reinterpret_cast<LV2_Atom_Sequence*>(outBytes.data());
    struct { LV2_Atom_Event event; voiceforge::CompareState body; } message{};
    compareIn->atom.type = compare.sequence;
    lv2_atom_sequence_clear(compareIn);
    message.event.body = {sizeof(message.body), compare.vector};
    message.body.vector.child_type = compare.floating;
    message.body.values[0] = message.body.values[1] = message.body.values[2] = 1.0f;
    message.body.values[3] = -6.5f;
    message.body.values[3 + voiceforge::compareCapacity] = 9.0f;
    require(lv2_atom_sequence_append_event(compareIn, inBytes.size() - sizeof(LV2_Atom), &message.event),
            "populate comparison exchange input");
    lilv_instance_connect_port(instance.get(), 2 * channels + 122, compareIn);
    lilv_instance_connect_port(instance.get(), 2 * channels + 123, compareOut);
    compareOut->atom.size = outBytes.size() - sizeof(LV2_Atom);
    lilv_instance_activate(instance.get());
    lilv_instance_run(instance.get(), 1);
    lilv_instance_deactivate(instance.get());
    state.reset(lilv_state_new_from_instance(plugin, instance.get(), &map, nullptr, nullptr, nullptr, nullptr,
        Values::read, &values, 0, features));
    require(state && lilv_state_get_num_properties(state.get()) == 1, "snapshot both A/B sides with LV2 State");
    char* serialized = lilv_state_to_string(world, &map, &unmap, state.get(),
        "urn:voiceforge:test-state", nullptr);
    require(serialized != nullptr, "serialize portable A/B snapshot");
    state.reset(lilv_state_new_from_string(world, &map, serialized));
    lilv_free(serialized);
    require(state && lilv_state_get_num_properties(state.get()) == 1, "parse serialized A/B");
    message.body.values[3] = -2.0f;
    lv2_atom_sequence_clear(compareIn);
    lv2_atom_sequence_append_event(compareIn, inBytes.size() - sizeof(LV2_Atom), &message.event);
    compareOut->atom.size = outBytes.size() - sizeof(LV2_Atom);
    lilv_instance_activate(instance.get());
    lilv_instance_run(instance.get(), 1);
    lilv_instance_deactivate(instance.get());
    lilv_state_restore(state.get(), instance.get(), Values::write, &values, 0, features);
    struct { LV2_Atom_Event event; int32_t value; } request{};
    request.event.body = {sizeof(request.value), compare.boolean};
    lv2_atom_sequence_clear(compareIn);
    lv2_atom_sequence_append_event(compareIn, inBytes.size() - sizeof(LV2_Atom), &request.event);
    compareOut->atom.size = outBytes.size() - sizeof(LV2_Atom);
    lilv_instance_activate(instance.get());
    lilv_instance_run(instance.get(), 1);
    lilv_instance_deactivate(instance.get());
    const auto* reply = lv2_atom_sequence_begin(&compareOut->body);
    require(compareOut->atom.size > sizeof(LV2_Atom_Sequence_Body) && reply->body.type == compare.vector,
            "reply to UI requesting the restored A/B snapshot");
    voiceforge::CompareState restored;
    std::memcpy(&restored, LV2_ATOM_BODY_CONST(&reply->body), sizeof(restored));
    require(restored.values[0] == 1.0f && restored.values[3] == -6.5f &&
            restored.values[3 + voiceforge::compareCapacity] == 9.0f, "both saved parameter sets survive reload");
}
} // namespace

int main(int argc, char** argv) {
    require(argc == 2 && setenv("LV2_PATH", argv[1], 1) == 0, "set LV2 host search path");
    std::unique_ptr<LilvWorld, decltype(&lilv_world_free)> world(lilv_world_new(), lilv_world_free);
    require(world != nullptr, "create Lilv world");
    lilv_world_load_all(world.get());
    test(world.get(), "https://weikiboy-tech.github.io/voiceforge-lv2/mono", 1);
    test(world.get(), "https://weikiboy-tech.github.io/voiceforge-lv2/stereo", 2);
}
