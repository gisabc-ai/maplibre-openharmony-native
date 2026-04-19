cmake_minimum_required(VERSION 3.18.1)

# ============================================================
# OpenHarmony NDK Toolchain
# ============================================================
set(OH_SDK_ROOT "$ENV{OH_SDK_ROOT}")
if(NOT OH_SDK_ROOT)
    set(OH_SDK_ROOT "$ENV{HOME}/oh-sdk")
endif()
if(NOT EXISTS "$OH_SDK_ROOT")
    set(OH_SDK_ROOT "$ENV{HOME}/oh-sdk")
endif()

set(CMAKE_TOOLCHAIN_FILE "${OH_SDK_ROOT}/build/cmake/ohos.toolchain.cmake")
set(OHOS_ARCH arm64)

project(maplibre_ohos)

# ============================================================
# Compile options
# ============================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# Source files
# ============================================================
add_library(maplibre_ohos SHARED
    main.cpp
    map_renderer.cpp
    tile_loader.cpp
    coordinate.cpp
    gesture_handler.cpp
)

# ============================================================
# Include dirs
# ============================================================
target_include_directories(maplibre_ohos PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ============================================================
# System libs
# ============================================================
find_library(LOG-lib     NAMES log REQUIRED)
find_library(EGL-lib    NAMES EGL REQUIRED)
find_library(GLESv2-lib NAMES GLESv2 REQUIRED)
find_library(curl-lib   NAMES curl)

target_link_libraries(maplibre_ohos PRIVATE
    ${LOG-lib}
    ${EGL-lib}
    ${GLESv2-lib}
)

# ============================================================
# Output
# ============================================================
set_target_properties(maplibre_ohos PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../libs/${OHOS_ARCH}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../libs/${OHOS_ARCH}
)
