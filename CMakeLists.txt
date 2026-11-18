cmake_minimum_required(VERSION 3.25)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include (PamplejuceVersion)
include (CPM)
include(PamplejuceMacOS)
include(JUCEDefaults)

set(PROJECT_NAME "KeyofPeaceWorshipReverb")
set(PRODUCT_NAME "KeyofPeace Worship Reverb")
set(COMPANY_NAME "KeyofPeace")
set(BUNDLE_ID "com.keyofpeace.worshipreverb")
set(FORMATS Standalone VST3)

set(CMAKE_POLICY_DEFAULT_CMP0141        NEW      CACHE STRING "" FORCE)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded CACHE STRING "" FORCE)

# Solución compatibilidad MSVC 14.52 / Visual Studio 18 en GitHub Actions
add_compile_definitions(_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH)
set(JUCE_VST3_AUTO_MANIFEST OFF CACHE BOOL "" FORCE)

project(${PROJECT_NAME} VERSION ${CURRENT_VERSION})

include(PamplejuceLog)
include(PamplejuceFunctions)

add_subdirectory(JUCE)
add_subdirectory(modules/clap-juce-extensions EXCLUDE_FROM_ALL)
add_subdirectory (modules/melatonin_inspector)

juce_add_plugin("${PROJECT_NAME}"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icon.png"
    COMPANY_NAME "${COMPANY_NAME}"
    BUNDLE_ID "${BUNDLE_ID}"
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE KoPp
    PLUGIN_CODE Kp01
    FORMATS "${FORMATS}"
    VST3_AUTO_MANIFEST FALSE
    PRODUCT_NAME "${PRODUCT_NAME}")

juce_generate_juce_header("${PROJECT_NAME}")

add_library(SharedCode INTERFACE)

clap_juce_extensions_plugin(TARGET "${PROJECT_NAME}"
    CLAP_ID "${BUNDLE_ID}"
    CLAP_FEATURES audio-effect)

pamplejuce_shared_code_defaults()

file(GLOB_RECURSE SourceFiles CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/source/*.h")
target_sources(SharedCode INTERFACE ${SourceFiles})

pamplejuce_add_assets()
pamplejuce_xcode_prettify()

target_compile_definitions(SharedCode
    INTERFACE
    _ALLOW_COMPILER_AND_STL_VERSION_MISMATCH
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
    VERSION="${CURRENT_VERSION}"
    PRODUCT_NAME_WITHOUT_VERSION="KeyofPeace Worship Reverb"
)

target_link_libraries(SharedCode
    INTERFACE
    Assets
    melatonin_inspector
    juce_audio_utils
    juce_audio_processors
    juce_dsp
    juce_gui_basics
    juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

target_link_libraries("${PROJECT_NAME}" PRIVATE SharedCode)

include(PamplejuceIPP)

# pamplejuce_add_tests()
pamplejuce_add_benchmarks()
include(GitHubENV)
