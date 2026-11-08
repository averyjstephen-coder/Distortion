cmake_minimum_required(VERSION 3.22)
project(RockIconsDrive VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

# JUCE is downloaded automatically the first time you configure.
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(RockIconsDrive
    COMPANY_NAME "Personal"
    PLUGIN_MANUFACTURER_CODE Pers
    PLUGIN_CODE Rdrv
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Rock Icons Drive"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD TRUE)

target_sources(RockIconsDrive PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(RockIconsDrive PRIVATE Source)

target_compile_definitions(RockIconsDrive PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(RockIconsDrive
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Plain C++ tools (no JUCE needed): engine tests and the demo renderer.
add_executable(rid_engine_tests tests/test_engine.cpp)
target_include_directories(rid_engine_tests PRIVATE Source tests)
add_executable(rid_render_demo tools/render_demo.cpp)
target_include_directories(rid_render_demo PRIVATE Source tests)
