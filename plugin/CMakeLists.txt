cmake_minimum_required(VERSION 3.22)

project(JackCompressor VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE'u GitHub'dan çeker. Yerelde indirdiysen:  -DFETCHCONTENT_SOURCE_DIR_JUCE=/yol/JUCE
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(JackCompressor
    COMPANY_NAME "Wavejackers"
    PLUGIN_MANUFACTURER_CODE Wvjk
    PLUGIN_CODE Jcmp
    FORMATS VST3
    PRODUCT_NAME "Jack Compressor"
    VST3_CATEGORIES Fx Dynamics
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(JackCompressor PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(JackCompressor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../core)

target_compile_definitions(JackCompressor PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=1)

# Türkçe karakterler için kaynak dosyalar UTF-8 okunmalı (MSVC)
target_compile_options(JackCompressor PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

target_link_libraries(JackCompressor
    PRIVATE
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
