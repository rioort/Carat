cmake_minimum_required(VERSION 3.22)

# Su Windows include le librerie di sistema nel plugin (non serve installare
# il Visual C++ Redistributable sul computer di chi lo usa)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(Carat VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

# JUCE: usa una copia locale se indicata (-DJUCE_DIR=...), altrimenti la scarica
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

set(CARAT_FORMATS VST3)
if(APPLE)
    list(APPEND CARAT_FORMATS AU)
endif()

juce_add_plugin(Carat
    COMPANY_NAME "Vibe Audio"
    BUNDLE_ID "com.vibeaudio.carat"
    PLUGIN_MANUFACTURER_CODE Vibe
    PLUGIN_CODE Crt1
    PRODUCT_NAME "Carat"
    DESCRIPTION "Mastering converter clipper"
    FORMATS ${CARAT_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Dynamics Mastering
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD ${CARAT_COPY_AFTER_BUILD})

target_sources(Carat PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(Carat PRIVATE Source)

target_compile_definitions(Carat PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Carat
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Test offline del motore DSP (nessuna dipendenza da JUCE)
option(CARAT_BUILD_TESTS "Compila i test del DSP" OFF)
if(CARAT_BUILD_TESTS)
    add_executable(CaratDspTests tests/DspTests.cpp)
    target_include_directories(CaratDspTests PRIVATE Source)
endif()
