cmake_minimum_required(VERSION 3.22)

project(OpenFruit VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/utf-8)
    add_compile_definitions(_USE_MATH_DEFINES NOMINMAX)
endif()

option(OF_BUILD_PLUGIN "Build the plugin" ON)
option(OF_BUILD_TESTS "Build the unit tests" ON)
option(OF_BUILD_TOOLS "Build developer tools" OFF)

include(FetchContent)

add_library(openfruit_core STATIC
    Source/Core/SonicAnalyzer.cpp
    Source/Core/ToneCache.cpp
    Source/Core/WavReader.cpp
    Source/Core/AnalysisQueue.cpp
    Source/Core/SampleVoice.cpp
    Source/Core/FruitEngine.cpp)
target_include_directories(openfruit_core PUBLIC Source/Core)
set_target_properties(openfruit_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(OF_BUILD_PLUGIN)
    if(EXISTS "/Users/alan.marcero/Development/openClapper/build/_deps/juce-src/CMakeLists.txt")
        FetchContent_Declare(juce
            SOURCE_DIR "/Users/alan.marcero/Development/openClapper/build/_deps/juce-src")
    else()
        FetchContent_Declare(juce
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 8.0.9
            GIT_SHALLOW TRUE)
    endif()
    FetchContent_MakeAvailable(juce)

    juce_add_binary_data(FruitAssets SOURCES
        assets/openFruit-hero.png
        assets/cyber-fruit.png
        assets/openfruit-faceplate.png)
    set_target_properties(FruitAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

    juce_add_plugin(OpenFruit
        COMPANY_NAME "Alan Marcero"
        BUNDLE_ID "com.alanmarcero.openfruit"
        PRODUCT_NAME "openFruit"
        PLUGIN_MANUFACTURER_CODE Amrc
        PLUGIN_CODE Ofrt
        FORMATS VST3 AU LV2 Standalone
        LV2URI "https://github.com/alanmarcero/openFruit"
        VST3_CATEGORIES Instrument Drum Sampler
        AU_MAIN_TYPE kAudioUnitType_MusicDevice
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE)

    target_sources(OpenFruit PRIVATE
        Source/Plugin/CyberLookAndFeel.cpp
        Source/Plugin/Parameters.cpp
        Source/Plugin/DefaultKit.cpp
        Source/Plugin/Controls.cpp
        Source/Plugin/SampleBrowserModal.cpp
        Source/Plugin/Panels.cpp
        Source/Plugin/PluginProcessor.cpp
        Source/Plugin/PluginEditor.cpp)

    target_include_directories(OpenFruit PRIVATE
        Source/Core
        Source/Plugin)

    target_compile_definitions(OpenFruit PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries(OpenFruit
        PRIVATE
            openfruit_core
            FruitAssets
            juce::juce_audio_processors
            juce::juce_audio_utils
            juce::juce_gui_basics
            juce::juce_gui_extra
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    if(OF_BUILD_TOOLS)
        add_executable(render_ui_snapshot
            tools/render_ui_snapshot.cpp
            Source/Plugin/CyberLookAndFeel.cpp
            Source/Plugin/Parameters.cpp
            Source/Plugin/DefaultKit.cpp
            Source/Plugin/Controls.cpp
            Source/Plugin/SampleBrowserModal.cpp
            Source/Plugin/Panels.cpp
            Source/Plugin/PluginProcessor.cpp
            Source/Plugin/PluginEditor.cpp)

        target_include_directories(render_ui_snapshot PRIVATE
            Source/Core
            Source/Plugin)

        target_link_libraries(render_ui_snapshot
            PRIVATE
                openfruit_core
                FruitAssets
                juce::juce_audio_processors
                juce::juce_audio_utils
                juce::juce_gui_basics
                juce::juce_gui_extra)
    endif()
endif()

if(OF_BUILD_TESTS)
    if(EXISTS "/Users/alan.marcero/Development/openClapper/build/_deps/catch2-src/CMakeLists.txt")
        FetchContent_Declare(Catch2
            SOURCE_DIR "/Users/alan.marcero/Development/openClapper/build/_deps/catch2-src")
    else()
        FetchContent_Declare(Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v3.9.1
            GIT_SHALLOW TRUE)
    endif()
    FetchContent_MakeAvailable(Catch2)

    add_executable(openfruit_tests
        Tests/SonicAnalyzerTests.cpp
        Tests/ToneCacheTests.cpp
        Tests/SampleVoiceTests.cpp
        Tests/FruitEngineTests.cpp)

    target_include_directories(openfruit_tests PRIVATE Tests)
    target_link_libraries(openfruit_tests PRIVATE openfruit_core Catch2::Catch2WithMain)

    enable_testing()
    include(Catch)
    catch_discover_tests(openfruit_tests)
endif()
