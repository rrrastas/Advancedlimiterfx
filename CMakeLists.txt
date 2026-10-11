cmake_minimum_required(VERSION 3.21)
project(AdvancedLimiterFX VERSION 1.0.0 LANGUAGES C CXX)

include(FetchContent)
FetchContent_Declare(clap
  GIT_REPOSITORY https://github.com/free-audio/clap.git
  GIT_TAG 1.2.2
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(clap)

add_library(AdvancedLimiterFX MODULE src/plugin.cpp)
target_link_libraries(AdvancedLimiterFX PRIVATE clap)
target_include_directories(AdvancedLimiterFX PRIVATE ${clap_SOURCE_DIR}/include)
target_compile_features(AdvancedLimiterFX PRIVATE cxx_std_17)
set_target_properties(AdvancedLimiterFX PROPERTIES
  PREFIX ""
  OUTPUT_NAME "AdvancedLimiterFX"
  BUNDLE TRUE
  BUNDLE_EXTENSION "clap"
  MACOSX_BUNDLE_GUI_IDENTIFIER "audio.rrrastas.advancedlimiterfx"
  MACOSX_BUNDLE_BUNDLE_NAME "AdvancedLimiterFX"
  MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
)
if(APPLE)
  set_target_properties(AdvancedLimiterFX PROPERTIES
    OSX_ARCHITECTURES "x86_64"
    MACOSX_DEPLOYMENT_TARGET "10.13"
  )
endif()
