cmake_minimum_required(VERSION 3.22)
project(NXProC3 VERSION 0.1.0 LANGUAGES C CXX)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 29396c22c93392d6738e021b83196283d64d850)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(NXProC3
    COMPANY_NAME "NX Audio"
    BUNDLE_ID audio.nx.proc3
    PLUGIN_MANUFACTURER_CODE NxAu
    PLUGIN_CODE Nxc3
    FORMATS VST3
    PRODUCT_NAME "NX PRO C 3")
target_sources(NXProC3 PRIVATE src/Plugin.cpp)
target_compile_features(NXProC3 PRIVATE cxx_std_17)
target_compile_definitions(NXProC3 PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(NXProC3 PRIVATE juce::juce_audio_processors juce::juce_audio_utils juce::juce_recommended_config_flags)
