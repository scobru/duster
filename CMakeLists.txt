cmake_minimum_required(VERSION 3.15)

project(Duster VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include JUCE framework
add_subdirectory(JUCE)

# Optional AAX SDK Configuration for Pro Tools
if(DEFINED ENV{JUCE_AAX_SDK_PATH} AND NOT DEFINED AAX_SDK_PATH)
    set(AAX_SDK_PATH "$ENV{JUCE_AAX_SDK_PATH}" CACHE PATH "Path to AAX SDK")
else()
    set(AAX_SDK_PATH "${CMAKE_SOURCE_DIR}/aax-sdk" CACHE PATH "Path to AAX SDK")
endif()

if(EXISTS "${AAX_SDK_PATH}/Interfaces/AAX.h")
    message(STATUS "AAX SDK found at: ${AAX_SDK_PATH}")
    juce_set_aax_sdk_path("${AAX_SDK_PATH}")
    set(DUSTER_FORMATS VST3 AU Standalone AAX)
    set(BUILD_AAX ON)
else()
    message(STATUS "AAX SDK not found at: ${AAX_SDK_PATH} - AAX format disabled")
    set(DUSTER_FORMATS VST3 AU Standalone)
    set(BUILD_AAX OFF)
endif()

juce_add_plugin(Duster
    COMPANY_NAME "AcousticLab"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE AcLb
    PLUGIN_CODE Dust
    FORMATS ${DUSTER_FORMATS}
    AAX_CATEGORY "AAX_ePlugInCategory_None"
    PRODUCT_NAME "Duster"
    DESCRIPTION "High-Quality Spectral Denoiser with Learn Profile and Transient Boost"
)

target_sources(Duster
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/DSP/DusterEngine.cpp
        Source/DSP/DusterEngine.h
        Source/DSP/SpectrumAnalyzerBridge.h
        Source/GUI/DusterVisualizer.cpp
        Source/GUI/DusterVisualizer.h
        Source/GUI/CustomRotarySlider.h
)

target_compile_definitions(Duster
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(Duster
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
