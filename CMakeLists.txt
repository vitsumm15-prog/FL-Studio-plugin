cmake_minimum_required(VERSION 3.22)
project(ZaryaSynth VERSION 0.9.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Static MSVC runtime: the installed plugin does not need the VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(ZARYA_BUILD_TESTS "Build the offline DSP test executable" ON)

# JUCE: a local copy in ./JUCE is used if present, otherwise JUCE 8 is downloaded.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(ZARYA_DSP_SOURCES
    Source/dsp/Wavetable.cpp
    Source/dsp/Voice.cpp
    Source/dsp/VoiceOsc.cpp
    Source/dsp/SynthEngine.cpp)

juce_add_plugin(Zarya
    COMPANY_NAME "Zarya Audio"
    PRODUCT_NAME "Zarya"
    VERSION 0.9.1
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Zary
    PLUGIN_CODE Zrya
    VST3_CATEGORIES Instrument Synth
    FORMATS VST3 Standalone)

juce_generate_juce_header(Zarya)

juce_add_binary_data(ZaryaAssets SOURCES Resources/avatar.png)

target_sources(Zarya PRIVATE
    Source/PluginProcessor.cpp
    Source/Parameters.cpp
    Source/PluginEditor.cpp
    ${ZARYA_DSP_SOURCES})

target_compile_definitions(Zarya PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Zarya
    PRIVATE
        ZaryaAssets
        juce::juce_audio_utils
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

if(MSVC)
    target_compile_options(Zarya PRIVATE /utf-8)
endif()

# JACK (and PipeWire via its JACK layer) for the Linux standalone; libjack is loaded at runtime.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(Zarya PUBLIC JUCE_JACK=1)
endif()

if(ZARYA_BUILD_TESTS)
    add_executable(ZaryaEngineTest Tests/EngineTest.cpp ${ZARYA_DSP_SOURCES})
    target_compile_features(ZaryaEngineTest PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(ZaryaEngineTest PRIVATE /utf-8)
    endif()
endif()
