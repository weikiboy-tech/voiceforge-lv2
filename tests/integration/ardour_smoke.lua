local directory = assert(os.getenv("VOICEFORGE_ARDOUR_SESSION"))
local base = "https://weikiboy-tech.github.io/voiceforge-lv2/"
local tracks = {
    {name = "Voice mono", count = 1, variant = "mono", gain = -3.0},
    {name = "Voice stereo", count = 2, variant = "stereo", gain = 6.0},
}

local function verify(session)
    for _, spec in ipairs(tracks) do
        local route = session:route_by_name(spec.name)
        assert(not route:isnil(), "Missing route " .. spec.name)
        local processor = route:nth_plugin(0)
        assert(not processor:isnil(), "Missing instance on " .. spec.name)
        local info = processor:to_insert():plugin(0):get_info()
        assert(info.unique_id == base .. spec.variant, "Wrong plugin on " .. spec.name)
        local gain, ok = ARDOUR.LuaAPI.get_processor_param(processor, 0)
        assert(ok and math.abs(gain - spec.gain) < 0.001, "Wrong saved gain on " .. spec.name)
    end
end

assert(AudioEngine:set_backend("None (Dummy)", "", ""), "Dummy audio backend unavailable")
if os.getenv("VOICEFORGE_ARDOUR_REOPEN") then
    local session = assert(load_session(directory, "VoiceForgeQA"))
    verify(session)
    close_session()
    print("VoiceForge Ardour session restored")
else
    local session = assert(create_session(directory, "VoiceForgeQA", 48000))
    for _, spec in ipairs(tracks) do
        session:new_audio_track(spec.count, 2, nil, 1, spec.name, ARDOUR.PresentationInfo.max_order,
                                ARDOUR.TrackMode.Normal, true)
        local route = session:route_by_name(spec.name)
        assert(not route:isnil(), "Could not create " .. spec.name)
        local processor = ARDOUR.LuaAPI.new_plugin(session, base .. spec.variant, ARDOUR.PluginType.LV2, "Natural Voice")
        assert(not processor:isnil(), "Ardour did not load " .. spec.variant)
        assert(route:add_processor_by_index(processor, 0, nil, true) == 0, "Could not insert VoiceForge")
        local highpass, valid = ARDOUR.LuaAPI.get_processor_param(processor, 3)
        assert(valid and math.abs(highpass - 65.0) < 0.001, "Ardour did not apply the Natural Voice preset")
        assert(ARDOUR.LuaAPI.set_processor_param(processor, 0, spec.gain), "Could not set input gain")
    end
    verify(session)
    assert(session:save_state("") == 0, "Could not save session")
    close_session()
    print("VoiceForge Ardour session created")
end
