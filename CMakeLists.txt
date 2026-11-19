cmake_minimum_required(VERSION 3.22)
project(NXProC3 VERSION 0.2.1 LANGUAGES C CXX)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

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

include(CTest)
if(BUILD_TESTING)
    FetchContent_Declare(doctest
        URL https://codeload.github.com/doctest/doctest/tar.gz/refs/tags/v2.4.12
        URL_HASH SHA256=73381c7aa4dee704bd935609668cf41880ea7f19fa0504a200e13b74999c2d70)
    FetchContent_MakeAvailable(doctest)
    add_executable(NXCompressorTests tests/CompressorTests.cpp)
    target_compile_features(NXCompressorTests PRIVATE cxx_std_17)
    target_compile_definitions(NXCompressorTests PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1)
    target_include_directories(NXCompressorTests PRIVATE src "${juce_SOURCE_DIR}/modules")
    target_link_libraries(NXCompressorTests PRIVATE NXProC3 doctest::doctest)
    add_test(NAME NXCompressorTests COMMAND NXCompressorTests)
endif()
