cmake_minimum_required(VERSION 3.22)

# macOS: universal binary (Apple Silicon + Intel), runs on macOS 10.13+
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures")
endif()

project(Smoothie VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

# JUCE: use the bundled copy in ./JUCE if present, otherwise download it.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.17-security
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(SMOOTHIE_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND SMOOTHIE_FORMATS AU)
endif()

juce_add_plugin(Smoothie
    COMPANY_NAME "Smoothie Audio"
    PLUGIN_MANUFACTURER_CODE Smth
    PLUGIN_CODE Smo1
    FORMATS ${SMOOTHIE_FORMATS}
    PRODUCT_NAME "Smoothie"
    BUNDLE_ID com.smoothieaudio.smoothie
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES Fx Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect
    SUPPRESS_AU_PLIST_RESOURCE_USAGE TRUE)

target_sources(Smoothie PRIVATE
    Source/Engine.cpp
    Source/Engine.h
    Source/Parameters.h
    Source/Presets.h
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h)

target_compile_definitions(Smoothie PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(Smoothie
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---- DSP test harness (headless, no GUI) ----
juce_add_console_app(SmoothieTests PRODUCT_NAME "SmoothieTests")
target_sources(SmoothieTests PRIVATE Tests/dsp_test.cpp Source/Engine.cpp Source/Engine.h)
target_include_directories(SmoothieTests PRIVATE Source)
target_compile_definitions(SmoothieTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(SmoothieTests PRIVATE juce::juce_dsp juce::juce_recommended_config_flags)

# ---- optional dev tool: render the editor to a PNG (needs a display, e.g. xvfb) ----
option(SMOOTHIE_BUILD_SNAPSHOT "Build the GUI snapshot dev tool" OFF)
if(SMOOTHIE_BUILD_SNAPSHOT)
    juce_add_console_app(SmoothieSnapshot PRODUCT_NAME "SmoothieSnapshot")
    target_sources(SmoothieSnapshot PRIVATE Tests/gui_snapshot.cpp Source/Engine.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_include_directories(SmoothieSnapshot PRIVATE Source)
    target_compile_definitions(SmoothieSnapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(SmoothieSnapshot PRIVATE juce::juce_audio_utils juce::juce_dsp)
endif()
