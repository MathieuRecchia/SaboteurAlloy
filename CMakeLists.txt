cmake_minimum_required(VERSION 3.20)
project(SaboteurScalpel VERSION 1.0.0)

include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.1
)
FetchContent_MakeAvailable(juce)

juce_add_plugin(SaboteurScalpel
    COMPANY_NAME "Saboteur"
    PLUGIN_MANUFACTURER_CODE "SbtR"
    PLUGIN_CODE "SbSc"
    FORMATS VST3
    PRODUCT_NAME "Saboteur Scalpel"
    NEEDS_JUCE_HEADER TRUE
)

target_sources(SaboteurScalpel PRIVATE
    Source/SaboteurScalpelProcessor.cpp
    Source/SaboteurScalpelEditor.cpp
)

target_compile_features(SaboteurScalpel PRIVATE cxx_std_20)
target_compile_definitions(SaboteurScalpel PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(SaboteurScalpel PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
)