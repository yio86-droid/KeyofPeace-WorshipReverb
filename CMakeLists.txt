cmake_minimum_required(VERSION 3.25)

# This tells cmake we have goodies in the /cmake folder
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include (PamplejuceVersion)

# Modern concise way to add dependencies to your project
include (CPM)

# Configures universal binaries and decides which version of macOS to support
include(PamplejuceMacOS)

# Couple tweaks that IMO should be JUCE defaults
include(JUCEDefaults)

# Internal project name (no spaces allowed)
set(PROJECT_NAME "KeyofPeaceWorshipReverb")

# Display name in your DAW
set(PRODUCT_NAME "KeyofPeace Worship Reverb")

# Company / Project name
set(COMPANY_NAME "KeyofPeace")

# Bundle identifier
set(BUNDLE_ID "com.keyofpeace.worshipreverb")

# Build only Standalone and VST3 to save compile time
set(FORMATS Standalone VST3)

# Force JUCE to work with sccache
set(CMAKE_POLICY_DEFAULT_CMP0141        NEW      CACHE STRING "" FORCE)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded CACHE STRING "" FORCE)

project(${PROJECT_NAME} VERSION ${CURRENT_VERSION})

# Log build environment info
include(PamplejuceLog)

# Defines the pamplejuce_* functions
include(PamplejuceFunctions)

# JUCE submodule
add_subdirectory(JUCE)

# Add CLAP format
add_subdirectory(modules/clap-juce-extensions EXCLUDE_FROM_ALL)

# Melatonin inspector module
add_subdirectory (modules/melatonin_inspector)

juce_add_plugin("${PROJECT_NAME}"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icon.png"
    COMPANY_NAME "${COMPANY_NAME}"
    BUNDLE_ID "${BUNDLE_ID}"
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE KoPp
    PLUGIN_CODE Kp01
    FORMATS "${FORMATS}"
    PRODUCT_NAME "${PRODUCT_NAME}")

# Generate JuceHeader.h so Claude's #include <JuceHeader.h> works seamlessly
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

# Disabled factory tests so your custom UI/DSP compiles cleanly
# pamplejuce_add_tests()

pamplejuce_add_benchmarks()

include(GitHubENV)
