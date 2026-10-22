cmake_minimum_required(VERSION 3.20)
project(SaboteurAlloy VERSION 1.0.0)

include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.1
)
FetchContent_MakeAvailable(juce)

# --- OPTIONnel : Décommenter ce bloc plus tard pour ajouter le format CLAP ---
# FetchContent_Declare(
#     clap_juce_extensions
#     GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
#     GIT_TAG        main
# )
# FetchContent_MakeAvailable(clap_juce_extensions)

juce_add_plugin(SaboteurAlloy
    COMPANY_NAME "Saboteur"
    PLUGIN_MANUFACTURER_CODE "SbtR"
    PLUGIN_CODE "SbAl"
    FORMATS VST3
    PRODUCT_NAME "Saboteur Alloy"
    NEEDS_JUCE_HEADER TRUE
)

# Décommenter également cette ligne lors du passage à CLAP :
# clap_juce_extensions_plugin(TARGET SaboteurAlloy CLAP_ID "com.saboteur.alloy" CLAP_FEATURES audio-effect compressor distortion)

target_sources(SaboteurAlloy PRIVATE
    Source/SaboteurAlloyProcessor.cpp
    Source/SaboteurAlloyEditor.cpp
)

target_compile_features(SaboteurAlloy PRIVATE cxx_std_20)
target_compile_definitions(SaboteurAlloy PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
)

target_link_libraries(SaboteurAlloy PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
)