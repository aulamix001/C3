cmake_minimum_required(VERSION 3.22)
project(NXProC3 VERSION 0.1.0 LANGUAGES C CXX)

include(FetchContent)
FetchContent_Declare(JUCE
    URL https://codeload.github.com/juce-framework/JUCE/tar.gz/29396c22c93392d6738e021b83196283d6e4d850
    URL_HASH SHA256=97cc01e0fe15278f6edadc895a3cf0aac2fd98bcd023823384d5ca7025beb91c)
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
